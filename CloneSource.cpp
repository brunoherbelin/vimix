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
#include "Resource.h"
#include "Visitor.h"
#include "FrameBuffer.h"
#include "Decorations.h"

#include "CloneSource.h"

const char* CloneSource::cloning_provenance_label[2] = { "Original texture", "Post-processed image" };


CloneSource::CloneSource(Source *origin, uint64_t id) : Source(id), origin_(origin), cloningsurface_(nullptr),
    read_index_(0), write_index_(0), delay_(0.0), paused_(false), provenance_(CLONE_TEXTURE)
{
    // initial name copies the origin name: diplucates are namanged in session
    name_ = origin->name();

    // set symbol
    symbol_ = new Symbol(Symbol::CLONE, glm::vec3(0.75f, 0.75f, 0.01f));
    symbol_->scale_.y = 1.5f;

    // init array
    stack_.fill(nullptr);
    elapsed_stack_.fill(0.0);
    timestamps_.fill(0);

    timer_ = g_timer_new ();
}

CloneSource::~CloneSource()
{
    if (origin_)
        origin_->clones_.remove(this);

    // delete all frame buffers
    for (size_t i = 0; i < stack_.size(); ++i){
        if ( stack_[i] != nullptr )
            delete stack_[i];
    }

    if (cloningsurface_)
        delete cloningsurface_;

    g_free(timer_);
}

CloneSource *CloneSource::clone(uint64_t id)
{
    // do not clone a clone : clone the original instead
    if (origin_)
        return origin_->clone(id);
    else
        return nullptr;
}

void CloneSource::init()
{
    if (origin_ && origin_->mode_ > Source::UNINITIALIZED) {

        // internal surface to draw the original texture
        cloningsurface_ = new Surface;
        cloningsurface_->setTextureIndex( origin()->texture() );

        // frame buffers where to draw frames from the origin source
        glm::vec3 res = origin_->frame()->resolution();
        for (size_t i = 0; i < stack_.size(); ++i){
            stack_[i] = new FrameBuffer( res, origin_->frame()->use_alpha() );
        }

        // set initial texture surface
        texturesurface_->setTextureIndex( stack_[read_index_]->texture() );

        // activate elapsed-timer
        g_timer_start(timer_);

        // create render Frame buffer matching size of images
        FrameBuffer *renderbuffer = new FrameBuffer( res, true);

        // set the renderbuffer of the source and attach rendering nodes
        attach(renderbuffer);

        // force update of activation mode
        active_ = true;

        // deep update to reorder
        ++View::need_deep_update_;

        // done init
        Log::Info("Source %s cloning source %s.", name().c_str(), origin_->name().c_str() );
    }
}

void CloneSource::setActive (bool on)
{
    // request update
    need_update_ |= active_ != on;

    active_ = on;

    groups_[View::RENDERING]->visible_ = active_;
    groups_[View::GEOMETRY]->visible_ = active_;
    groups_[View::LAYER]->visible_ = active_;

    if (origin_) {
        if ( mode_ > Source::UNINITIALIZED)
            origin_->touch();

        // change visibility of active surface (show preview of origin when inactive)
        if (activesurface_) {
            if (active_)
                activesurface_->setTextureIndex(Resource::getTextureTransparent());
            else
                activesurface_->setTextureIndex(stack_[read_index_]->texture());
        }
    }
}

void CloneSource::update(float dt)
{
    if (active_ && !paused_ &&  origin_ && cloningsurface_ != nullptr) {

        double now = g_timer_elapsed (timer_, NULL) ;

        // increment enplacement of write index
        write_index_ = (write_index_+1)%(stack_.size());
        elapsed_stack_[write_index_] = now;
        timestamps_[write_index_] = origin_->playtime();

        // CLONE_RENDER : blit rendered framebuffer in the stack
        if (provenance_ == CLONE_RENDER)
            origin_->frame()->blit(stack_[write_index_]);
        // CLONE_TEXTURE : render origin texture in the stack
        else {
            stack_[write_index_]->begin();
            cloningsurface_->draw(glm::identity<glm::mat4>(), stack_[write_index_]->projection());
            stack_[write_index_]->end();
        }

        // define emplacement of read index
        if (delay_ < 0.001)
            // minimal difference if no delay
            read_index_ = write_index_;
        else
        {
            // starting where we are at, get the next index that satisfies the delay
            size_t previous_index = read_index_;
            while ( now - elapsed_stack_[read_index_] > delay_)  {
                // usually, one frame increment suffice
                read_index_ = (read_index_ + 1 )%(stack_.size());
                // break the loop if running infinite (never happens)
                if (previous_index == read_index_)
                    break;
            }
        }

        // update the source surface to be rendered
        texturesurface_->setTextureIndex( stack_[read_index_]->texture() );

    }

    Source::update(dt);
}


void CloneSource::setDelay(double second)
{
    delay_ = CLAMP(second, 0.0, 1.0);
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

void CloneSource::replay()
{
    g_timer_reset(timer_);
    elapsed_stack_.fill(0.0);
    write_index_ = 0;
    read_index_ = 1;
}

guint64 CloneSource::playtime () const
{
    return timestamps_[read_index_];
}


uint CloneSource::texture() const
{
    if (cloningsurface_ != nullptr)
        return stack_[read_index_]->texture();
    else
        return Resource::getTextureTransparent();
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
    return std::string("clone of '") + origin_->name() + "'";
}
