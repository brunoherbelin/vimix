/*
 * This file is part of vimix - video live mixer
 *
 * **Copyright** (C) 2019-2022 Bruno Herbelin <bruno.herbelin@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
**/

#include <algorithm>

#include <tinyxml2.h>

#include "defines.h"
#include "BaseToolkit.h"
#include "Source.h"
#include "Settings.h"
#include "FrameBuffer.h"
#include "SessionCreator.h"
#include "SessionVisitor.h"
#include "ActionManager.h"
#include "RenderSource.h"
#include "MixingGroup.h"
#include "ControlManager.h"
#include "SourceCallback.h"
#include "CountVisitor.h"
#include "Log.h"

#include "Session.h"

SessionNote::SessionNote(const std::string &t, bool l, int s): label(std::to_string(BaseToolkit::uniqueId())),
    text(t), large(l), stick(s), pos(glm::vec2(520.f, 30.f)), size(glm::vec2(220.f, 220.f))
{
}

Session::Session(uint64_t id) : id_(id), active_(true), activation_threshold_(MIXING_MIN_THRESHOLD),
    filename_(""), thumbnail_(nullptr)
{
    // create unique id
    if (id_ == 0)
        id_ = BaseToolkit::uniqueId();

    config_[View::RENDERING] = new Group;
    config_[View::RENDERING]->scale_ = glm::vec3(0.f);

    config_[View::GEOMETRY] = new Group;
    config_[View::GEOMETRY]->scale_ = Settings::application.views[View::GEOMETRY].default_scale;
    config_[View::GEOMETRY]->translation_ = Settings::application.views[View::GEOMETRY].default_translation;

    config_[View::LAYER] = new Group;
    config_[View::LAYER]->scale_ = Settings::application.views[View::LAYER].default_scale;
    config_[View::LAYER]->translation_ = Settings::application.views[View::LAYER].default_translation;

    config_[View::MIXING] = new Group;
    config_[View::MIXING]->scale_ = Settings::application.views[View::MIXING].default_scale;
    config_[View::MIXING]->translation_ = Settings::application.views[View::MIXING].default_translation;

    config_[View::TEXTURE] = new Group;
    config_[View::TEXTURE]->scale_ = Settings::application.views[View::TEXTURE].default_scale;
    config_[View::TEXTURE]->translation_ = Settings::application.views[View::TEXTURE].default_translation;

    input_sync_.resize(INPUT_MAX, Metronome::SYNC_NONE);

    snapshots_.xmlDoc_ = new tinyxml2::XMLDocument;
    start_time_ = gst_util_get_timestamp ();
}


void Session::InputSourceCallback::clear()
{
    // go through all instances stored in InputSourceCallback
    for (auto clb = instances_.begin(); clb != instances_.end(); ++clb) {
        // finish all
        if (clb->second.first != nullptr)
            clb->second.first->finish();
        if (clb->second.second != nullptr)
            clb->second.second->finish();
    }
    // do not keep references: will be deleted when terminated
    instances_.clear();
}

Session::~Session()
{
    // TODO delete all mixing groups?
    auto group_iter = mixing_groups_.begin();
    while ( group_iter != mixing_groups_.end() ){
        delete (*group_iter);
        group_iter = mixing_groups_.erase(group_iter);
    }

    // delete all sources
    for(auto it = sources_.begin(); it != sources_.end(); ) {
        // erase this source from the list
        it = deleteSource(*it);
    }

    // delete all callbacks
    for (auto iter = input_callbacks_.begin(); iter != input_callbacks_.end();
         iter = input_callbacks_.erase(iter))  {
        if ( iter->second.model_ != nullptr)
            delete iter->second.model_;
        iter->second.clear();
    }

    delete config_[View::RENDERING];
    delete config_[View::GEOMETRY];
    delete config_[View::LAYER];
    delete config_[View::MIXING];
    delete config_[View::TEXTURE];

    snapshots_.keys_.clear();
    delete snapshots_.xmlDoc_;
}

uint64_t Session::runtime() const
{
    return gst_util_get_timestamp () - start_time_;
}

void Session::setActive (bool on)
{
    if (active_ != on) {
        active_ = on;
        for(auto it = sources_.begin(); it != sources_.end(); ++it) {
            (*it)->setActive(active_);
        }
    }
}

// update all sources
void Session::update(float dt)
{
    // no update until render view is initialized
    if ( render_.frame() == nullptr )
        return;

    // listen to inputs
    for (auto k = input_callbacks_.begin(); k != input_callbacks_.end(); ++k)
    {
        // get if the input is activated (e.g. key pressed)
        bool input_active = Control::manager().inputActive(k->first);

        // get the callback model for each input
        // if the model is valid and the target variant was filled
        if ( k->second.model_ != nullptr && k->second.target_.index() > 0) {

            // if the value referenced as pressed changed state
            if ( input_active != k->second.active_ ) {

                // ON PRESS
                if (input_active) {

                    // Add callback to the target(s)
                    // 1. Case of variant as Source pointer
                    if (Source * const* v = std::get_if<Source *>(&k->second.target_)) {
                        // verify variant value
                        if ( *v != nullptr ) {
                            // generate a new callback from the model
                            SourceCallback *forward = k->second.model_->clone();
                            // apply value multiplyer from input
                            forward->multiply( Control::manager().inputValue(k->first) );
                            // add delay
                            forward->delay( Metronome::manager().timeToSync( (Metronome::Synchronicity) input_sync_[k->first] ) );
                            // add callback to source
                            (*v)->call( forward );
                            // get the reverse of the callback (can be null)
                            SourceCallback *backward = forward->reverse(*v);;
                            // remember instances
                            k->second.instances_[(*v)->id()] = {forward, backward};

                        }
                    }
                    // 2. Case of variant as index of batch
                    else if ( const size_t* v = std::get_if<size_t>(&k->second.target_)) {
                        // verify variant value
                        if ( *v < batch_.size() )
                        {
                            // loop over all sources in Batch
                            for (auto sid = batch_[*v].begin(); sid != batch_[*v].end(); ++sid){
                                SourceList::const_iterator sit = std::find_if(sources_.begin(), sources_.end(), Source::hasId(*sid));;
                                if ( sit != sources_.end()) {
                                    // generate a new callback from the model
                                    SourceCallback *forward = k->second.model_->clone();
                                    // apply value multiplyer from input
                                    forward->multiply( Control::manager().inputValue(k->first) );
                                    // add delay
                                    forward->delay( Metronome::manager().timeToSync( (Metronome::Synchronicity) input_sync_[k->first] ) );
                                    // add callback to source
                                    (*sit)->call( forward );
                                    // get the reverse of the callback (can be null)
                                    SourceCallback *backward = forward->reverse(*sit);;
                                    // remember instances
                                    k->second.instances_[*sid] = {forward, backward};
                                }
                            }
                        }
                    }
                }
                // ON RELEASE
                else {
                    // go through all instances stored for that action
                    for (auto clb = k->second.instances_.begin(); clb != k->second.instances_.end(); ++clb) {
                        // find the source referenced by each instance
                        SourceList::const_iterator sit = std::find_if(sources_.begin(), sources_.end(), Source::hasId(clb->first));;
                        // if the source is valid
                        if ( sit != sources_.end()) {
                            // either call the reverse if exists (stored as second element in pair)
                            if (clb->second.second != nullptr)
                                (*sit)->call( clb->second.second, true );
                            // or finish the called
                            else
                                (*sit)->finish( clb->second.first );
                        }
                    }
                    // do not keep reference to instances: will be deleted when finished
                    k->second.instances_.clear();
                }

                // remember state of callback
                k->second.active_ = input_active;
            }
        }
    }

    // pre-render all sources
    bool ready = true;
    for( SourceList::iterator it = sources_.begin(); it != sources_.end(); ++it){

        // ensure the RenderSource is rendering *this* session
        RenderSource *rs = dynamic_cast<RenderSource *>( *it );
        if ( rs!= nullptr && rs->session() != this )
            rs->setSession(this);

        // discard failed source
        if ( (*it)->failed() ) {
            // insert source in list of failed
            // (NB: insert in set fails if source is already listed)
            failed_.insert( *it );
            // do not render
            render_.scene.ws()->detach( (*it)->group(View::RENDERING) );
        }
        // render normally
        else {
            // session is not ready if one source is not ready
            if ( !(*it)->ready() )
                ready = false;
            // update the source
            (*it)->setActive(activation_threshold_);
            (*it)->update(dt);
            // render the source
            (*it)->render();
        }
    }

    // update session's mixing groups
    auto group_iter = mixing_groups_.begin();
    while ( group_iter != mixing_groups_.end() ){
        // update all valid groups
        if ((*group_iter)->size() > 1) {
            (*group_iter)->update(dt);
            group_iter++;
        }
        else
            // delete invalid groups (singletons)
            group_iter = deleteMixingGroup(group_iter);
    }

    // update fading requested
    if (fading_.active) {

        // animate
        fading_.progress += dt;

        // update animation
        if ( fading_.duration > 0.f && fading_.progress < fading_.duration ) {
            // interpolation
            float f =  fading_.progress / fading_.duration;
            f = ( 1.f - f ) * fading_.start + f * fading_.target;
            render_.setFading( f );
        }
        // arrived at target
        else {
            // set precise value
            render_.setFading( fading_.target );
            // fading finished
            fading_.active = false;
            fading_.start  = fading_.target;
            fading_.duration = fading_.progress = 0.f;
        }
    }

    // update the scene tree
    render_.update(dt);

    // draw render view in Frame Buffer
    render_.draw();

    // draw the thumbnail only after all sources are ready
    if (ready)
        render_.drawThumbnail();
}

SourceList::iterator Session::addSource(Source *s)
{
    // lock before change
    access_.lock();

    // find the source
    SourceList::iterator its = find(s);

    // ok, it's NOT in the list !
    if (its == sources_.end()) {
        // insert the source in the rendering
        render_.scene.ws()->attach(s->group(View::RENDERING));
        // insert the source to the end of the list
        sources_.push_back(s);
        // return the iterator to the source created at the end
        its = --sources_.end();
    }

    // unlock access
    access_.unlock();

    return its;
}

SourceList::iterator Session::deleteSource(Source *s)
{
    // lock before change
    access_.lock();

    // find the source
    SourceList::iterator its = find(s);
    // ok, its in the list !
    if (its != sources_.end()) {
        // remove Node from the rendering scene
        render_.scene.ws()->detach( s->group(View::RENDERING) );
        // inform group
        if (s->mixingGroup() != nullptr)
            s->mixingGroup()->detach(s);
        // erase the source from the failed list
        failed_.erase(s);
        // erase the source from the update list & get next element
        its = sources_.erase(its);
        // delete the source : safe now
        delete s;
    }

    // unlock access
    access_.unlock();

    // return end of next element
    return its;
}

SourceList::iterator Session::removeSource(Source *s)
{
    SourceList::iterator ret = sources_.end();

    // lock before change
    access_.lock();

    // find the source
    SourceList::iterator its = find(s);
    // ok, its in the list !
    if (its != sources_.end()) {
        // remove Node from the rendering scene
        render_.scene.ws()->detach( s->group(View::RENDERING) );
        // inform group
        if (s->mixingGroup() != nullptr)
            s->mixingGroup()->detach(s);
        // erase the source from the failed list
        failed_.erase(s);
        // erase the source from the update list & get next element
        ret = sources_.erase(its);
    }

    // unlock access
    access_.unlock();

    return ret;
}

Source *Session::popSource()
{
    Source *s = nullptr;

    SourceList::iterator its = sources_.begin();
    if (its != sources_.end())
    {
        s = *its;
        // remove Node from the rendering scene
        render_.scene.ws()->detach( s->group(View::RENDERING) );
        // inform group
        if (s->mixingGroup() != nullptr)
            s->mixingGroup()->detach(s);
        // erase the source from the update list & get next element
        sources_.erase(its);
    }

    return s;
}

static void replaceThumbnail(Session *s)
{
    if (s != nullptr) {
        FrameBufferImage *t = s->renderThumbnail();
        if (t != nullptr) // avoid recursive infinite loop
            s->setThumbnail(t);
    }
}

void Session::setThumbnail(FrameBufferImage *t)
{
    resetThumbnail();
    // replace with given image
    if (t != nullptr)
        thumbnail_ = t;
    // no thumbnail image given: capture from rendering in a parallel thread
    else
        std::thread( replaceThumbnail, this ).detach();
}

void Session::resetThumbnail()
{
    if (thumbnail_ != nullptr)
        delete thumbnail_;
    thumbnail_ = nullptr;
}

void Session::setResolution(glm::vec3 resolution, bool useAlpha)
{
    // setup the render view: if not specified the default config resolution will be used
    render_.setResolution( resolution, useAlpha );
    // store the actual resolution set in the render view
    config_[View::RENDERING]->scale_ = render_.resolution();
}

void Session::setFadingTarget(float f, float duration)
{
    // targetted fading value
    fading_.target = CLAMP(f, 0.f, 1.f);
    // starting point for interpolation
    fading_.start  = fading();
    // initiate animation
    fading_.progress = 0.f;
    fading_.duration = duration;
    // activate update
    fading_.active = true;
}

SourceList::iterator Session::begin()
{
    return sources_.begin();
}

SourceList::iterator Session::end()
{
    return sources_.end();
}

SourceList::iterator Session::find(Source *s)
{
    return std::find(sources_.begin(), sources_.end(), s);
}

SourceList::iterator Session::find(uint64_t id)
{
    return std::find_if(sources_.begin(), sources_.end(), Source::hasId(id));
}

SourceList::iterator Session::find(std::string namesource)
{
    return std::find_if(sources_.begin(), sources_.end(), Source::hasName(namesource));
}

SourceList::iterator Session::find(Node *node)
{
    return std::find_if(sources_.begin(), sources_.end(), Source::hasNode(node));
}

SourceList::iterator Session::find(float depth_from, float depth_to)
{
    return std::find_if(sources_.begin(), sources_.end(), Source::hasDepth(depth_from, depth_to));
}

SourceList Session::getDepthSortedList() const
{
    return depth_sorted(sources_);
}

uint Session::size() const
{
    return sources_.size();
}

uint Session::numSources() const
{
    CountVisitor counter;
    for( SourceList::const_iterator it = sources_.cbegin(); it != sources_.cend(); ++it) {
        (*it)->accept(counter);
    }
    return counter.numSources();
}

SourceIdList Session::getIdList() const
{
    return ids(sources_);
}

std::list<std::string> Session::getNameList(uint64_t exceptid) const
{
    std::list<std::string> namelist;

    for( SourceList::const_iterator it = sources_.cbegin(); it != sources_.cend(); ++it) {
        if ( (*it)->id() != exceptid )
            namelist.push_back( (*it)->name() );
    }

    return namelist;
}


bool Session::empty() const
{
    return sources_.empty();
}

SourceList::iterator Session::at(int index)
{
    if ( index < 0 || index > (int) sources_.size())
        return sources_.end();

    int i = 0;
    SourceList::iterator it = sources_.begin();
    while ( i < index && it != sources_.end() ){
        i++;
        ++it;
    }
    return it;
}

int Session::index(SourceList::iterator it) const
{
    int index = -1;
    int count = 0;
    for(auto i = sources_.begin(); i != sources_.end(); ++i, ++count) {
        if ( i == it ) {
            index = count;
            break;
        }
    }
    return index;
}

void Session::move(int current_index, int target_index)
{
    if ( current_index < 0 || current_index > (int) sources_.size()
         || target_index < 0 || target_index > (int) sources_.size()
         || target_index == current_index )
        return;

    SourceList::iterator from = at(current_index);
    SourceList::iterator to   = at(target_index);
    if ( target_index > current_index )
        ++to;

    Source *s = (*from);
    sources_.erase(from);
    sources_.insert(to, s);
}

bool Session::canlink (SourceList sources)
{
    bool canlink = true;

    // verify that all sources given are valid in the sesion
    validate(sources);

    for (auto it = sources.begin(); it != sources.end(); ++it) {
        // this source is linked
        if ( (*it)->mixingGroup() != nullptr ) {
            // askt its group to detach it
            canlink = false;
        }
    }

    return canlink;
}

void Session::link(SourceList sources, Group *parent)
{
    // we need at least 2 sources to make a group
    if (sources.size() > 1) {

        unlink(sources);

        // create and add a new mixing group
        MixingGroup *g = new MixingGroup(sources);
        mixing_groups_.push_back(g);

        // if provided, attach the group to the parent
        if (g && parent != nullptr)
            g->attachTo( parent );

    }
}

void Session::unlink (SourceList sources)
{
    // verify that all sources given are valid in the sesion
    validate(sources);

    // brute force : detach all given sources
    for (auto it = sources.begin(); it != sources.end(); ++it) {
        // this source is linked
        if ( (*it)->mixingGroup() != nullptr ) {
            // askt its group to detach it
            (*it)->mixingGroup()->detach(*it);
        }
    }
}

void Session::addNote(SessionNote note)
{
    notes_.push_back( note );
}

std::list<SessionNote>::iterator Session::beginNotes ()
{
    return notes_.begin();
}

std::list<SessionNote>::iterator Session::endNotes ()
{
    return notes_.end();
}

std::list<SessionNote>::iterator Session::deleteNote (std::list<SessionNote>::iterator n)
{
    if (n != notes_.end())
        return notes_.erase(n);

    return notes_.end();
}

std::list<SourceList> Session::getMixingGroups () const
{
    std::list<SourceList> lmg;

    for (auto group_it = mixing_groups_.begin(); group_it!= mixing_groups_.end(); ++group_it)
        lmg.push_back( (*group_it)->getCopy() );

    return lmg;
}

std::list<MixingGroup *>::iterator Session::deleteMixingGroup (std::list<MixingGroup *>::iterator g)
{
    if (g != mixing_groups_.end()) {
        delete (*g);
        return mixing_groups_.erase(g);
    }
    return mixing_groups_.end();
}

std::list<MixingGroup *>::iterator Session::beginMixingGroup()
{
    return mixing_groups_.begin();
}

std::list<MixingGroup *>::iterator Session::endMixingGroup()
{
    return mixing_groups_.end();
}


size_t Session::numBatch() const
{
    return batch_.size();
}

void Session::addBatch(const SourceIdList &ids)
{
    batch_.push_back( ids );
}

void Session::addSourceToBatch(size_t i, Source *s)
{
    if (i < batch_.size() )
    {
        if ( std::find(batch_[i].begin(), batch_[i].end(), s->id()) == batch_[i].end() )
            batch_[i].push_back(s->id());
    }
}

void Session::removeSourceFromBatch(size_t i, Source *s)
{
    if (i < batch_.size() )
    {
        if ( std::find(batch_[i].begin(), batch_[i].end(), s->id()) != batch_[i].end() )
            batch_[i].remove( s->id() );
    }
}

void Session::deleteBatch(size_t i)
{
    if (i < batch_.size() )
        batch_.erase( batch_.begin() + i);
}

SourceList Session::getBatch(size_t i) const
{
    SourceList list;

    if (i < batch_.size() )
    {
        for (auto sid = batch_[i].begin(); sid != batch_[i].end(); ++sid){

            SourceList::const_iterator it = std::find_if(sources_.begin(), sources_.end(), Source::hasId( *sid));;
            if ( it != sources_.end())
                list.push_back( *it);

        }
    }

    return list;
}

void Session::validate (SourceList &sources)
{
    // verify that all sources given are valid in the sesion
    // and remove the invalid sources
    for (auto _it = sources.begin(); _it != sources.end(); ) {
        SourceList::iterator found = std::find(sources_.begin(), sources_.end(), *_it);
        if ( found == sources_.end() )
            _it = sources.erase(_it);
        else
            _it++;
    }
}

Session *Session::load(const std::string& filename, uint level)
{
    // create session
    SessionCreator creator(level);
    creator.load(filename);

    // return created session
    return creator.session();
}

std::string Session::save(const std::string& filename, Session *session, const std::string& snapshot_name)
{
    std::string ret;

    if (session) {
        // lock access while saving
        session->access_.lock();

        // capture a snapshot of current version if requested (do not create thread)
        if (!snapshot_name.empty())
            Action::takeSnapshot(session, snapshot_name, false );

        // save file to disk
        if (SessionVisitor::saveSession(filename, session))
            // return filename string on success
            ret = filename;

        // unlock access after saving
        session->access_.unlock();
    }

    return ret;
}

void Session::assignInputCallback(uint input, Target target, SourceCallback *callback)
{
    // find if this callback is already assigned
    auto k = input_callbacks_.begin();
    for (; k != input_callbacks_.end(); ++k)
    {
        // yes, then just change the target
        if ( k->second.model_ == callback) {
            k->second.target_ = target;
            // reverse became invalid
            k->second.clear();
            break;
        }
    }

    // if this callback is not assigned yet (looped until end)
    if ( k == input_callbacks_.end() ) {
        // create new entry
        std::multimap<uint, InputSourceCallback>::iterator added = input_callbacks_.emplace(input, InputSourceCallback() );
        added->second.model_ = callback;
        added->second.target_ = target;
    }
}

void Session::swapInputCallback(uint from, uint to)
{
    std::multimap<uint, InputSourceCallback> swapped_callbacks_;

    for (auto k = input_callbacks_.begin(); k != input_callbacks_.end(); ++k)
    {
        if ( k->first == from )
            swapped_callbacks_.emplace( to,  k->second);
        else
            swapped_callbacks_.emplace( k->first,  k->second);
    }

    input_callbacks_.swap(swapped_callbacks_);
}

void Session::copyInputCallback(uint from, uint to)
{
    if ( input_callbacks_.count(from) > 0 ) {
        auto from_callbacks = getSourceCallbacks(from);
        for (auto it = from_callbacks.cbegin(); it != from_callbacks.cend(); ++it){
            assignInputCallback(to, it->first, it->second->clone() );
        }
    }
}

std::list<std::pair<Target, SourceCallback *> > Session::getSourceCallbacks(uint input)
{
    std::list< std::pair< Target, SourceCallback*> > ret;

    if ( input_callbacks_.count(input) > 0 ) {
        auto result = input_callbacks_.equal_range(input);
        for (auto it = result.first; it != result.second; ++it)
            ret.push_back( std::pair< Target, SourceCallback*>(it->second.target_, it->second.model_) );
    }

    return ret;
}

void Session::deleteInputCallback(SourceCallback *callback)
{
    for (auto k = input_callbacks_.begin(); k != input_callbacks_.end(); ++k)
    {
        if ( k->second.model_ == callback) {
            delete callback;
            k->second.clear();
            input_callbacks_.erase(k);
            break;
        }
    }
}

void Session::deleteInputCallbacks(uint input)
{
    for (auto k = input_callbacks_.begin(); k != input_callbacks_.end();)
    {
        if ( k->first == input) {
            if (k->second.model_)
                delete k->second.model_;
            k->second.clear();
            k = input_callbacks_.erase(k);
        }
        else
            ++k;
    }
}

void Session::deleteInputCallbacks(Target target)
{
    for (auto k = input_callbacks_.begin(); k != input_callbacks_.end();)
    {
        if ( k->second.target_ == target) {
            if (k->second.model_)
                delete k->second.model_;
            k->second.clear();
            k = input_callbacks_.erase(k);
        }
        else
            ++k;
    }
}


void Session::clearInputCallbacks()
{
    for (auto k = input_callbacks_.begin(); k != input_callbacks_.end(); )
    {
        if (k->second.model_)
            delete k->second.model_;
        k->second.clear();

        k = input_callbacks_.erase(k);
    }
}

std::list<uint> Session::assignedInputs()
{
    std::list<uint> inputs;

    // fill with list of keys
    for(const auto& [key, value] : input_callbacks_) {
        inputs.push_back(key);
    }

    // remove duplicates
    inputs.unique();

    return inputs;
}

bool Session::inputAssigned(uint input)
{
    return input_callbacks_.find(input) != input_callbacks_.end();
}

void Session::setInputSynchrony(uint input, Metronome::Synchronicity sync)
{
    input_sync_[input] = sync;
}

std::vector<Metronome::Synchronicity> Session::getInputSynchrony()
{
    return input_sync_;
}

Metronome::Synchronicity Session::inputSynchrony(uint input)
{
    return input_sync_[input];
}


void Session::applySnapshot(uint64_t key)
{
    tinyxml2::XMLElement *snapshot_node_ = snapshots_.xmlDoc_->FirstChildElement( SNAPSHOT_NODE(key).c_str() );;
    if (snapshot_node_) {

        SourceIdList session_sources = getIdList();

        SessionLoader loader( this );
        loader.load( snapshot_node_ );

        // loaded_sources contains map of xml ids of all sources treated by loader
        std::map< uint64_t, Source* > loaded_sources = loader.getSources();

        // remove intersect of both lists (sources were updated by SessionLoader)
        for( auto lsit = loaded_sources.begin(); lsit != loaded_sources.end(); ){
            auto ssit = std::find(session_sources.begin(), session_sources.end(), (*lsit).first);
            if ( ssit != session_sources.end() ) {
                lsit = loaded_sources.erase(lsit);
                session_sources.erase(ssit);
            }
            else
                lsit++;
        }

        // remaining ids in list sessionsources : to remove
        while ( !session_sources.empty() ){
            SourceList::iterator its = find(session_sources.front());
            // its in the list !
            if (its != sources_.end()) {
#ifndef ACTION_DEBUG
                Log::Info("Delete   id %s\n", std::to_string(session_sources.front() ).c_str());
#endif
                // delete source from session
                deleteSource( *its );
            }
            session_sources.pop_front();
        }

        ++View::need_deep_update_;
    }
}

