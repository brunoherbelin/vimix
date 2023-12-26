/*
 * This file is part of vimix - video live mixer
 *
 * **Copyright** (C) 2019-2023 Bruno Herbelin <bruno.herbelin@gmail.com>
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

#include <glm/gtc/matrix_transform.hpp>

#include "defines.h"
#include "Log.h"
#include "FrameBuffer.h"
#include "Resource.h"
#include "Decorations.h"
#include "Visitor.h"
#include "Session.h"
#include "SessionCreator.h"

#include "SessionSource.h"

SessionSource::SessionSource(uint64_t id) : Source(id), failed_(false), timer_(0), paused_(false)
{
    session_ = new Session;

    // redo frame for MIXING view
    frames_[View::MIXING]->clear();
    // set Frame in MIXING with an additional border
    Group *group = new Group;
    Frame *frame = new Frame(Frame::ROUND, Frame::THIN, Frame::DROP);
    frame->translation_.z = 0.1;
    frame->color = glm::vec4( COLOR_DEFAULT_SOURCE, 0.95f);
    group->attach(frame);
    frame = new Frame(Frame::ROUND, Frame::THIN, Frame::DROP);
    frame->scale_.x = 1.04;
    frame->scale_.y = 1.07;
    frame->translation_.z = 0.1;
    frame->color = glm::vec4( COLOR_DEFAULT_SOURCE, 0.95f);
    group->attach(frame);
    frames_[View::MIXING]->attach(group);
    group = new Group;
    frame = new Frame(Frame::ROUND, Frame::LARGE, Frame::DROP);
    frame->translation_.z = 0.01;
    frame->color = glm::vec4( COLOR_HIGHLIGHT_SOURCE, 1.f);
    group->attach(frame);
    frame = new Frame(Frame::ROUND, Frame::THIN, Frame::DROP);
    frame->scale_.x = 1.04;
    frame->scale_.y = 1.07;
    frame->translation_.z = 0.01;
    frame->color = glm::vec4( COLOR_HIGHLIGHT_SOURCE, 1.f);
    group->attach(frame);
    frames_[View::MIXING]->attach(group);

    // redo frame for LAYER view
    frames_[View::LAYER]->clear();
    // set Frame in LAYER with an additional border (Group in perspective)
    group = new Group;
    frame = new Frame(Frame::ROUND, Frame::THIN, Frame::PERSPECTIVE);
    frame->translation_.z = 0.1;
    frame->color = glm::vec4( COLOR_DEFAULT_SOURCE, 0.95f);
    group->attach(frame);
    Frame *persp = new Frame(Frame::GROUP, Frame::THIN, Frame::NONE);
    persp->translation_.z = 0.1;
    persp->color = glm::vec4( COLOR_DEFAULT_SOURCE, 0.95f);
    group->attach(persp);
    frames_[View::LAYER]->attach(group);
    group = new Group;
    frame = new Frame(Frame::ROUND, Frame::LARGE, Frame::PERSPECTIVE);
    frame->translation_.z = 0.1;
    frame->color = glm::vec4( COLOR_HIGHLIGHT_SOURCE, 1.f);
    group->attach(frame);
    persp = new Frame(Frame::GROUP, Frame::LARGE, Frame::NONE);
    persp->translation_.z = 0.1;
    persp->color = glm::vec4( COLOR_HIGHLIGHT_SOURCE, 1.f);
    group->attach(persp);
    frames_[View::LAYER]->attach(group);
}

SessionSource::~SessionSource()
{
    // delete session
    if (session_)
        delete session_;
}

Session *SessionSource::detach()
{
    // remember pointer to give away
    Session *giveaway = session_;

    // work on a new session
    session_ = new Session;

    // un-ready
    ready_ = false;

    // ask to delete me
    failed_ = true;

    // lost ref to previous session: to be deleted elsewhere...
    return giveaway;
}

Source::Failure SessionSource::failed() const
{
    return failed_ ? FAIL_CRITICAL : FAIL_NONE;
}

uint SessionSource::texture() const
{
    if (session_ && session_->frame())
        return session_->frame()->texture();
    else
        return Resource::getTextureBlack();
}

void SessionSource::setActive (bool on)
{    
    Source::setActive(on);

    // change status of session (recursive change of internal sources)
    if (session_) {
        session_->setActive(active_);
    }
}

void SessionSource::update(float dt)
{
    Source::update(dt);

    if (session_ == nullptr)
        return;

    // update content
    if (active_ && !paused_) {
        session_->update(dt);
        timer_ += guint64(dt * 1000.f) * GST_USECOND;
    }

    // manage sources which failed
    if ( !session_->failedSources().empty() ) {

        SourceListUnique _failedsources = session_->failedSources();
        for(auto it = _failedsources.begin(); it != _failedsources.end(); ++it)  {

            // only deal with sources that are still attached
            if ( session_->attached( *it ) ) {

                // intervention depends on the severity of the failure
                Source::Failure fail = (*it)->failed();

                // Attempt to repair RETRY failed sources
                // (can be automatically repaired without user intervention)
                if (fail == Source::FAIL_RETRY) {
                    // generated replacement source
                    SessionLoader loader( session_ );
                    Source *replacement = loader.recreateSource( *it );
                    // delete failed source
                    session_->deleteSource( *it );
                    // add replacement source, if successfully created
                    if (replacement)
                        session_->addSource(replacement);
                    else
                        Log::Warning("Source '%s' failed and could not be fixed.", (*it)->name().c_str());

                }
                // Detatch CRITICAL failed sources from the mixer
                // (not deleted in the session; user can replace it)
                else if (fail == Source::FAIL_CRITICAL) {
                    session_->detachSource( *it );
                }
                // Delete FATAL failed sources
                // (nothing can be done by the user)
                else {
                    Log::Warning("Source '%s' failed and was deleted.", (*it)->name().c_str());
                    session_->deleteSource( *it );
                }
            }
        }

        // fail session if all its sources failed
        if ( session_->size() < 1 )
            failed_ = true;

    }

}

void SessionSource::play (bool on)
{
    paused_ = !on;

    if (session_) {
        for( SourceList::iterator it = session_->begin(); it != session_->end(); ++it)
            (*it)->setActive(!paused_);
    }
}

void SessionSource::replay ()
{
    if (session_) {
        for( SourceList::iterator it = session_->begin(); it != session_->end(); ++it)
            (*it)->replay();
        timer_ = 0;
    }
}

bool SessionSource::playable () const
{
    bool p = false;
    if (session_) {
        for( SourceList::iterator it = session_->begin(); it != session_->end(); ++it) {
            if ( (*it)->playable() ){
                p = true;
                break;
            }
        }
    }
    return p;
}

SessionFileSource::SessionFileSource(uint64_t id) : SessionSource(id), path_(""), initialized_(false), wait_for_sources_(false)
{
    // specific node for transition view
    groups_[View::TRANSITION]->visible_ = false;
    groups_[View::TRANSITION]->scale_ = glm::vec3(0.1f, 0.1f, 1.f);
    groups_[View::TRANSITION]->translation_ = glm::vec3(-1.f, 0.f, 0.f);

    frames_[View::TRANSITION] = new Switch;
    Frame *frame = new Frame(Frame::ROUND, Frame::THIN, Frame::DROP);  // visible
    frame->translation_.z = 0.1;
    frame->color = glm::vec4( COLOR_DEFAULT_SOURCE, 0.85f);
    frames_[View::TRANSITION]->attach(frame);
    frame = new Frame(Frame::ROUND, Frame::THIN, Frame::DROP);         // selected
    frame->translation_.z = 0.1;
    frame->color = glm::vec4( COLOR_DEFAULT_SOURCE, 1.f);
    frames_[View::TRANSITION]->attach(frame);
    frame = new Frame(Frame::ROUND, Frame::LARGE, Frame::DROP);        // current
    frame->translation_.z = 0.01;
    frame->color = glm::vec4( COLOR_TRANSITION_SOURCE, 1.f);
    frames_[View::TRANSITION]->attach(frame);
    groups_[View::TRANSITION]->attach(frames_[View::TRANSITION]);

    overlays_[View::TRANSITION] = new Group;
    overlays_[View::TRANSITION]->translation_.z = 0.1;
    overlays_[View::TRANSITION]->visible_ = false;

    Symbol *loader = new Symbol(Symbol::DOTS);
    loader->scale_ = glm::vec3(2.f, 2.f, 1.f);
    loader->update_callbacks_.push_back(new InfiniteGlowCallback);
    overlays_[View::TRANSITION]->attach(loader);
    Symbol *center = new Symbol(Symbol::CIRCLE_POINT, glm::vec3(0.f, -1.05f, 0.1f));
    overlays_[View::TRANSITION]->attach(center);
    groups_[View::TRANSITION]->attach(overlays_[View::TRANSITION]);

    // set symbol
    symbol_ = new Symbol(Symbol::SESSION, glm::vec3(0.75f, 0.75f, 0.01f));
    symbol_->scale_.y = 1.5f;

}

void SessionFileSource::load(const std::string &p, uint level)
{
    path_ = p;

    // delete session
    if (session_)
        delete session_;
    session_ = nullptr;

    // reset renderbuffer_
    if (renderbuffer_)
        delete renderbuffer_;
    renderbuffer_ = nullptr;

    // init session
    if ( path_.empty() ) {
        // empty session
        session_ = new Session;
        Log::Warning("Empty Session filename provided.");
    }
    else {
        // launch a thread to load the session file
        sessionLoader_ = std::async(std::launch::async, Session::load, path_, level);
        Log::Notify("Opening '%s'...", p.c_str());
    }

    // will be ready after init and one frame rendered
    initialized_ = false;
    ready_ = false;
}

void SessionFileSource::reload()
{
    load(path_);
}

void SessionFileSource::init()
{
    // init is first about getting the loaded session
    if (session_ == nullptr) {
        // did the loader finish ?
        if (sessionLoader_.wait_for(std::chrono::milliseconds(4)) == std::future_status::ready) {
            session_ = sessionLoader_.get();
            if (session_ == nullptr)
                failed_ = true;
        }
    }
    else {

        if (wait_for_sources_) {

            // force update of of all sources
            active_ = true;
            touch();

            // deep update to make sure reordering of sources
            ++View::need_deep_update_;

            // update to draw framebuffer
            session_->update(dt_);

            // if all sources are ready, done with initialization!
            auto unintitializedsource = std::find_if_not(session_->begin(), session_->end(), Source::isInitialized);
            if (unintitializedsource == session_->end()) {
                // done init
                wait_for_sources_ = false;
                initialized_ = true;
                Log::Info("Source '%s' linked to Session %s.", name().c_str(), std::to_string(session_->id()).c_str());
            }
        }
        else if ( !failed_ ) {

            // set resolution
            session_->setResolution( session_->config(View::RENDERING)->scale_ );

            // update to draw framebuffer
            session_->update(dt_);

            // get the texture index from framebuffer of session, apply it to the surface
            texturesurface_->setTextureIndex( session_->frame()->texture() );

            // create Frame buffer matching size of session
            FrameBuffer *renderbuffer = new FrameBuffer( session_->frame()->resolution() );

            // set the renderbuffer of the source and attach rendering nodes
            attach(renderbuffer);

            // wait for all sources to init
            if (session_->size() > 0)
                wait_for_sources_ = true;
            else {
                initialized_ = true;
                Log::Info("Source '%s' linked to new Session %s.", name().c_str(), std::to_string(session_->id()).c_str());
            }
        }
    }

    if (initialized_)
    {
        // remove the loading icon
        Node *loader = overlays_[View::TRANSITION]->back();
        overlays_[View::TRANSITION]->detach(loader);
        delete loader;
        // request deep update to reorder session_
        ++View::need_deep_update_;
        // run update to redraw framebuffer (after reorder)
        session_->update(dt_);
    }
}

void SessionFileSource::render()
{
    if ( !initialized_ )
        init();
    else {
        // render the media player into frame buffer
        renderbuffer_->begin();
        texturesurface_->draw(glm::identity<glm::mat4>(), renderbuffer_->projection());
        renderbuffer_->end();
        ready_ = true;
    }
}

void SessionFileSource::accept(Visitor& v)
{
    Source::accept(v);
    v.visit(*this);
}

glm::ivec2 SessionFileSource::icon() const
{
    return glm::ivec2(ICON_SOURCE_SESSION);
}

std::string SessionFileSource::info() const
{
    return "Child Session";
}


SessionGroupSource::SessionGroupSource(uint64_t id) : SessionSource(id), resolution_(glm::vec3(0.f))
{
    // set symbol
    symbol_ = new Symbol(Symbol::GROUP, glm::vec3(0.75f, 0.75f, 0.01f));
    symbol_->scale_.y = 1.5f;
}

void SessionGroupSource::init()
{
    if ( resolution_.x > 0.f && resolution_.y > 0.f ) {

        // valid resolution given to create render view
        session_->setResolution( resolution_, true );

        // deep update to make sure reordering of sources
        ++View::need_deep_update_;

        //  update to draw framebuffer
        session_->update( dt_ );

        // get the texture index from framebuffer of session, apply it to the surface
        texturesurface_->setTextureIndex( session_->frame()->texture() );

        // create Frame buffer matching size of session
        FrameBuffer *renderbuffer = new FrameBuffer( session_->frame()->resolution(), FrameBuffer::FrameBuffer_alpha );

        // set the renderbuffer of the source and attach rendering nodes
        attach(renderbuffer);

        // render the session frame into frame buffer immediately (avoids 1 frame blank)
        renderbuffer_->begin();
        texturesurface_->draw(glm::identity<glm::mat4>(), renderbuffer_->projection());
        renderbuffer_->end();

        // done init
        uint N = session_->size();
        std::string numsource = std::to_string(N) + " source" + (N>1 ? "s" : "");
        Log::Info("Bundle Session %s reading %s (%d x %d).", std::to_string(session_->id()).c_str(), numsource.c_str(),
                  int(renderbuffer->resolution().x), int(renderbuffer->resolution().y) );
        Log::Info("Source '%s' linked to Bundle Session %s.", name().c_str(), std::to_string(session_->id()).c_str());
    }
}

void SessionGroupSource::setSession (Session *s)
{
    if (s) {
        if ( session_ )
            delete session_;
        session_ = s;
        resolution_ = s->frame()->resolution();
    }
}

bool SessionGroupSource::import(Source *source)
{
    bool ret = false;

    if ( session_ )
    {
        SourceList::iterator its = session_->addSource(source);
        if (its != session_->end())
            ret = true;
    }

    return ret;
}

void SessionGroupSource::accept(Visitor& v)
{
    Source::accept(v);
    v.visit(*this);
}

glm::ivec2 SessionGroupSource::icon() const
{
    return glm::ivec2(ICON_SOURCE_GROUP);
}

std::string SessionGroupSource::info() const
{
    return "Bundle Session";
}
