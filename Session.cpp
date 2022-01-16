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

#include "defines.h"
#include "BaseToolkit.h"
#include "Source.h"
#include "Settings.h"
#include "FrameBuffer.h"
#include "FrameGrabber.h"
#include "SessionCreator.h"
#include "SessionSource.h"
#include "RenderSource.h"
#include "MixingGroup.h"
#include "Log.h"

#include "Session.h"

SessionNote::SessionNote(const std::string &t, bool l, int s): label(std::to_string(BaseToolkit::uniqueId())),
    text(t), large(l), stick(s), pos(glm::vec2(520.f, 30.f)), size(glm::vec2(220.f, 220.f))
{
}

Session::Session() : active_(true), activation_threshold_(MIXING_MIN_THRESHOLD),
    filename_(""), failedSource_(nullptr), thumbnail_(nullptr)
{
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

    snapshots_.xmlDoc_ = new tinyxml2::XMLDocument;
    start_time_ = gst_util_get_timestamp ();
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

    // pre-render all sources
    failedSource_ = nullptr;
    bool ready = true;
    for( SourceList::iterator it = sources_.begin(); it != sources_.end(); ++it){

        // ensure the RenderSource is rendering *this* session
        RenderSource *rs = dynamic_cast<RenderSource *>( *it );
        if ( rs!= nullptr && rs->session() != this )
            rs->setSession(this);

        // discard failed source
        if ( (*it)->failed() ) {
            failedSource_ = (*it);
        }
        // render normally
        else {
            if ( !(*it)->ready() )
                ready = false;
            // update the source
            (*it)->setActive(activation_threshold_);
            (*it)->update(dt);
            // render the source // TODO: verify ok to render after update
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

    // ok, its NOT in the list !
    if (its == sources_.end()) {
        // insert the source in the rendering
        render_.scene.ws()->attach(s->group(View::RENDERING));
        // insert the source to the beginning of the list
        sources_.push_back(s);
        // return the iterator to the source created at the beginning
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

void Session::removeSource(Source *s)
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
        // erase the source from the update list & get next element
        sources_.erase(its);
    }

    // unlock access
    access_.unlock();
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
    // setup the render view: if not specified the default config resulution will be used
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

uint Session::numSource() const
{
    return sources_.size();
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


size_t Session::numPlayGroups() const
{
    return play_groups_.size();
}

void Session::addPlayGroup(const SourceIdList &ids)
{
    play_groups_.push_back( ids );
}

void Session::addToPlayGroup(size_t i, Source *s)
{
    if (i < play_groups_.size() )
    {
        if ( std::find(play_groups_[i].begin(), play_groups_[i].end(), s->id()) == play_groups_[i].end() )
            play_groups_[i].push_back(s->id());
    }
}

void Session::removeFromPlayGroup(size_t i, Source *s)
{
    if (i < play_groups_.size() )
    {
        if ( std::find(play_groups_[i].begin(), play_groups_[i].end(), s->id()) != play_groups_[i].end() )
            play_groups_[i].remove( s->id() );
    }
}

void Session::deletePlayGroup(size_t i)
{
    if (i < play_groups_.size() )
        play_groups_.erase( play_groups_.begin() + i);
}

SourceList Session::playGroup(size_t i) const
{
    SourceList list;

    if (i < play_groups_.size() )
    {
        for (auto sid = play_groups_[i].begin(); sid != play_groups_[i].end(); ++sid){

            SourceList::const_iterator it = std::find_if(sources_.begin(), sources_.end(), Source::hasId( *sid));;
            if ( it != sources_.end())
                list.push_back( *it);

        }
    }

    return list;
}

void Session::lock()
{
    access_.lock();
}

void Session::unlock()
{
    access_.unlock();
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

Session *Session::load(const std::string& filename, uint recursion)
{
    // create session
    SessionCreator creator(recursion);
    creator.load(filename);

    // return created session
    return creator.session();
}

