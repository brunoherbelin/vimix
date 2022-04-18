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

#include <gst/gst.h>

#include <glm/gtc/matrix_access.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Log.h"
#include "defines.h"
#include "Resource.h"
#include "Visitor.h"
#include "FrameBuffer.h"
#include "Decorations.h"

#include "CloneSource.h"


CloneSource::CloneSource(Source *origin, uint64_t id) : Source(id), origin_(origin), cloningsurface_(nullptr),
    garbage_image_(nullptr), timer_reset_(false), delay_(0.0), paused_(false)
{
    // initial name copies the origin name: diplucates are namanged in session
    name_ = origin->name();

    // set symbol
    symbol_ = new Symbol(Symbol::CLONE, glm::vec3(0.75f, 0.75f, 0.01f));
    symbol_->scale_.y = 1.5f;

    // init connecting line
    connection_ = new DotLine;
    connection_->color = glm::vec4(COLOR_DEFAULT_SOURCE, 0.5f);
    connection_->target = origin->groups_[View::MIXING]->translation_;
    groups_[View::MIXING]->attach(connection_);

    // init timer
    timer_ = g_timer_new ();
}

CloneSource::~CloneSource()
{
    if (origin_)
        origin_->clones_.remove(this);

    // delete all frame buffers
    while (!images_.empty()) {
        if (images_.front() != nullptr)
            delete images_.front();
        images_.pop();
    }

    if (cloningsurface_)
        delete cloningsurface_;

    g_free(timer_);
}

void CloneSource::init()
{
    if (origin_ && origin_->ready_ && origin_->mode_ > Source::UNINITIALIZED && origin_->renderbuffer_) {

        // frame buffers where to draw frames from the origin source
        glm::vec3 res = origin_->frame()->resolution();
        images_.push( new FrameBuffer( res, origin_->frame()->use_alpha() ) );
        timestamps_.push( origin_->playtime() );
        elapsed_.push( 0. );

        // ask to reset elapsed-timer
        timer_reset_ = true;

        // set initial texture surface
        origin_->frame()->blit( images_.front() );
        texturesurface_->setTextureIndex( images_.front()->texture() );

        // create render Frame buffer matching size of images
        FrameBuffer *renderbuffer = new FrameBuffer( res, true );

        // set the renderbuffer of the source and attach rendering nodes
        attach(renderbuffer);

        // force update of activation mode
        active_ = true;

        // deep update to reorder
        ++View::need_deep_update_;

        // done init
        Log::Info("Source '%s' cloning source '%s'.", name().c_str(), origin_->name().c_str() );
    }
}


void CloneSource::setActive (bool on)
{
    // try to activate (may fail if source is cloned)
    Source::setActive(on);

    if (origin_) {
        if ( mode_ > Source::UNINITIALIZED)
            origin_->touch();

        // change visibility of active surface (show preview of origin when inactive)
        if (activesurface_) {
            if (active_ || images_.empty())
                activesurface_->setTextureIndex(Resource::getTextureTransparent());
            else
                activesurface_->setTextureIndex(renderbuffer_->texture());
        }
    }
}

void CloneSource::update(float dt)
{
    Source::update(dt);

    if (origin_ && !images_.empty()) {

        if (!paused_ && active_)
        {
            // if temporary FBO was pending to be deleted, delete it now
            if (garbage_image_ != nullptr) {
                delete garbage_image_;
                garbage_image_ = nullptr;
            }

            // Reset elapsed timer on request (init or replay)
            if ( timer_reset_ ) {
                g_timer_start(timer_);
                timer_reset_ = false;
            }
            // What time is it?
            double now = g_timer_elapsed (timer_, NULL);

            // is the total buffer of images longer than delay ?
            if ( now - elapsed_.front() > delay_ )
            {
                // remember FBO to be reused if needed (see below) or deleted later
                garbage_image_ = images_.front();
                // remove element from queue (front)
                images_.pop();
                elapsed_.pop();
                timestamps_.pop();
            }

            // add image to queue to accumulate buffer images until delay reached
            if ( images_.empty() || now - elapsed_.front() < delay_ + (dt * 0.001) )
            {
                // create a FBO if none can be reused (from above) and test for RAM in GPU
                if (garbage_image_ == nullptr && ( images_.empty() || Rendering::shouldHaveEnoughMemory(origin_->frame()->resolution(), origin_->frame()->use_alpha()) ) )
                    garbage_image_ = new FrameBuffer( origin_->frame()->resolution(), origin_->frame()->use_alpha() );
                // image available
                if (garbage_image_ != nullptr) {
                    // add element to queue (back)
                    images_.push( garbage_image_ );
                    elapsed_.push( now );
                    timestamps_.push( origin_->playtime() );
                    // garbage_image_ FBO is now used, it should not be deleted
                    garbage_image_ = nullptr;
                }
                else {
                    delay_ = now - elapsed_.front() - (dt * 0.001);
                    Log::Warning("Cannot satisfy delay for Clone %s: not enough RAM in graphics card.", name_.c_str());
                }
            }

            // make sure the queue is not empty (in case of failure above)
            if ( !images_.empty() ) {
                // blit rendered framebuffer in the newest image (back)
                origin_->frame()->blit( images_.back() );

                // update the source surface to be rendered with the oldest image (front)
                texturesurface_->setTextureIndex( images_.front()->texture() );
            }
        }

        // update connection line target to position of origin source
        connection_->target = glm::inverse( GlmToolkit::transform(groups_[View::MIXING]->translation_, glm::vec3(0), groups_[View::MIXING]->scale_) ) *
                glm::vec4(origin_->groups_[View::MIXING]->translation_, 1.f);
    }
}


void CloneSource::setDelay(double second)
{
    delay_ = CLAMP(second, 0.0, 2.0);
}

void CloneSource::play (bool on)
{
    // if a different state is asked
    if (paused_ == on) {

        // restart clean if was paused
        if (paused_)
            replay();
        // toggle state
        paused_ = !on;
    }
}

bool CloneSource::playable () const
{
    if (origin_)
        return origin_->playable();
    return true;
}

void CloneSource::replay()
{
    // clear to_delete_ FBO if pending
    if (garbage_image_ != nullptr) {
        delete garbage_image_;
        garbage_image_ = nullptr;
    }

    // remove all images except the one in the back (newest)
    while (images_.size() > 1) {
        // do not delete immediately the (oldest) front image (the FBO is currently displayed)
        if (garbage_image_ == nullptr)
            garbage_image_ = images_.front();
        // delete other FBO (unused)
        else if (images_.front() != nullptr)
            delete images_.front();
        images_.pop();
    }

    // remove all timing
    while (!elapsed_.empty())
        elapsed_.pop();
    // reset elapsed timer to 0
    timer_reset_ = true;
    elapsed_.push(0.);

    // remove all timestamps
    while (!timestamps_.empty())
        timestamps_.pop();
    timestamps_.push(0);
}

guint64 CloneSource::playtime () const
{
    return timestamps_.front();
}


uint CloneSource::texture() const
{
    if (!images_.empty())
        return images_.front()->texture();
    else
        return Resource::getTextureBlack();
}

void CloneSource::accept(Visitor& v)
{
    Source::accept(v);
    if (!failed())
        v.visit(*this);
}

glm::ivec2 CloneSource::icon() const
{
    return glm::ivec2(ICON_SOURCE_CLONE);
}

std::string CloneSource::info() const
{
    return "Clone";
}
