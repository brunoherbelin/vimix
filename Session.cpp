#include <algorithm>

#include "defines.h"
#include "Settings.h"
#include "FrameBuffer.h"
#include "Session.h"
#include "FrameGrabber.h"
#include "SessionCreator.h"
#include "SessionSource.h"

#include "Log.h"

Session::Session() : failedSource_(nullptr), active_(true), fading_target_(0.f)
{
    filename_ = "";

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
}


Session::~Session()
{
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
}

void Session::setActive (bool on)
{
    if (active_ != on) {
        active_ = on;
        for(auto it = sources_.begin(); it != sources_.end(); it++) {
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

    // pre-render of all sources
    failedSource_ = nullptr;
    for( SourceList::iterator it = sources_.begin(); it != sources_.end(); it++){

        // ensure the RenderSource is rendering this session
        RenderSource *s = dynamic_cast<RenderSource *>( *it );
        if ( s!= nullptr && s->session() != this )
                s->setSession(this);

        if ( (*it)->failed() ) {
            failedSource_ = (*it);
        }
        else {
            // render the source
            (*it)->render();
            // update the source
            (*it)->update(dt);
        }
    }

    // apply fading (smooth dicotomic reaching)
    float f = render_.fading();
    if ( ABS_DIFF(f, fading_target_) > EPSILON) {
        render_.setFading( f + ( fading_target_ - f ) / 2.f);
    }

    // update the scene tree
    render_.update(dt);

    // draw render view in Frame Buffer
    render_.draw();

}


SourceList::iterator Session::addSource(Source *s)
{
    SourceList::iterator its = sources_.end();

    // lock before change
    access_.lock();

    // find the source
    its = find(s);

    // ok, its NOT in the list !
    if (its == sources_.end()) {
        // insert the source in the rendering
        render_.scene.ws()->attach(s->group(View::RENDERING));
        // insert the source to the beginning of the list
        sources_.push_front(s);
        // return the iterator to the source created at the beginning
        its = sources_.begin();
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

void Session::setResolution(glm::vec3 resolution, bool useAlpha)
{
    // setup the render view: if not specified the default config resulution will be used
    render_.setResolution( resolution, useAlpha );
    // store the actual resolution set in the render view
    config_[View::RENDERING]->scale_ = render_.resolution();
}

void Session::setFading(float f, bool forcenow)
{
    if (forcenow)
        render_.setFading( f );

    fading_target_ = CLAMP(f, 0.f, 1.f);
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

uint Session::numSource() const
{
    return sources_.size();
}

std::list<uint64_t> Session::getIdList() const
{
    std::list<uint64_t> idlist;

    for( auto sit = sources_.begin(); sit != sources_.end(); sit++)
        idlist.push_back( (*sit)->id() );

    // make sure no duplicate
    idlist.unique();

    return idlist;
}

bool Session::empty() const
{
    return sources_.empty();
}

SourceList::iterator Session::at(int index)
{
    if (index<0)
        return sources_.end();

    int i = 0;
    SourceList::iterator it = sources_.begin();
    while ( i < index && it != sources_.end() ){
        i++;
        it++;
    }
    return it;
}

int Session::index(SourceList::iterator it) const
{
    int index = -1;
    int count = 0;
    for(auto i = sources_.begin(); i != sources_.end(); i++, count++) {
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
        to++;

    Source *s = (*from);
    sources_.erase(from);
    sources_.insert(to, s);
}

void Session::lock()
{
    access_.lock();
}

void Session::unlock()
{
    access_.unlock();
}


Session *Session::load(const std::string& filename, uint recursion)
{
    SessionCreator creator(recursion);
    creator.load(filename);

    return creator.session();
}

