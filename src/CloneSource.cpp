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

#include <gst/gst.h>

#include <glm/gtc/matrix_access.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Log.h"
#include "defines.h"
#include "Resource.h"
#include "Decorations.h"
#include "Visitor.h"
#include "FrameBuffer.h"
#include "FrameBufferFilter.h"
#include "DelayFilter.h"
#include "ImageFilter.h"

#include "CloneSource.h"

CloneSource::CloneSource(Source *origin, uint64_t id) : Source(id), origin_(origin), paused_(false), filter_(nullptr)
{
    // initial name copies the origin name: diplucates are namanged in session
    name_ = origin->name();

    // set symbol
    symbol_ = new Symbol(Symbol::CLONE, glm::vec3(0.75f, 0.75f, 0.01f));
    symbol_->scale_.y = 1.5f;

    // init connecting line
    connection_ = new DotLine;
    connection_->color = glm::vec4(COLOR_HIGHLIGHT_SOURCE, 0.6f);
    connection_->target = origin->groups_[View::MIXING]->translation_;
    groups_[View::MIXING]->attach(connection_);

    // default to pass-through filter
    filter_ = new PassthroughFilter;
}

CloneSource::~CloneSource()
{
    if (origin_)
        origin_->clones_.remove(this);

    delete filter_;
}

void CloneSource::detach()
{
    Log::Info("Source '%s' detached from '%s'.", name().c_str(), origin_->name().c_str() );
    origin_ = nullptr;
}

void CloneSource::init()
{
    if (origin_ && origin_->ready_ && origin_->mode_ > Source::UNINITIALIZED && origin_->renderbuffer_) {

        // create render Frame buffer matching size of images
        FrameBuffer *renderbuffer = new FrameBuffer( origin_->frame()->resolution(), origin_->frame()->flags() );

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

void CloneSource::render()
{
    if ( renderbuffer_ == nullptr )
        init();
    else {
        // render filter image
        filter_->draw( origin_->frame() );

        // ensure correct output texture is displayed (could have changed if filter changed)
        texturesurface_->setTextureIndex( filter_->texture() );

        // detect resampling (change of resolution in filter)
        if ( renderbuffer_->resolution() != filter_->resolution() ) {
            renderbuffer_->resize( filter_->resolution() );
//            FrameBuffer *renderbuffer = new FrameBuffer( filter_->resolution(), origin_->frame()->flags() );
//            attach(renderbuffer);
        }

        // render textured surface into frame buffer
        // NB: this also applies the color correction shader
        renderbuffer_->begin();
        texturesurface_->draw(glm::identity<glm::mat4>(), renderbuffer_->projection());
        renderbuffer_->end();
        ready_ = true;
    }
}

void CloneSource::setActive (bool on)
{
    bool was_active = active_;

    // try to activate (may fail if source is cloned)
    Source::setActive(on);

    // enable / disable filtering
    if ( active_ != was_active )
        filter_->setEnabled( active_ );

}

void CloneSource::update(float dt)
{
    Source::update(dt);

    if (origin_) {

        if (!paused_ && active_)
            filter_->update(dt);

        // update connection line target to position of origin source
        connection_->target = glm::inverse( GlmToolkit::transform(groups_[View::MIXING]->translation_, glm::vec3(0), groups_[View::MIXING]->scale_) ) *
                glm::vec4(origin_->groups_[View::MIXING]->translation_, 1.f);
    }
}

void CloneSource::setFilter(FrameBufferFilter::Type T)
{
    if (filter_)
        delete filter_;

    switch (T)
    {
    case FrameBufferFilter::FILTER_DELAY:
        filter_ = new DelayFilter;
        break;
    case FrameBufferFilter::FILTER_RESAMPLE:
        filter_ = new ResampleFilter;
        break;
    case FrameBufferFilter::FILTER_BLUR:
        filter_ = new BlurFilter;
        break;
    case FrameBufferFilter::FILTER_SHARPEN:
        filter_ = new SharpenFilter;
        break;
    case FrameBufferFilter::FILTER_SMOOTH:
        filter_ = new SmoothFilter;
        break;
    case FrameBufferFilter::FILTER_EDGE:
        filter_ = new EdgeFilter;
        break;
    case FrameBufferFilter::FILTER_ALPHA:
        filter_ = new AlphaFilter;
        break;
    case FrameBufferFilter::FILTER_IMAGE:
        filter_ = new ImageFilter;
        break;
    default:
    case FrameBufferFilter::FILTER_PASSTHROUGH:
        filter_ = new PassthroughFilter;
        break;
    }
}

void CloneSource::play (bool on)
{
    // if a different state is asked
    if (paused_ == on) {

        // play / pause filter to suspend clone
        filter_->setEnabled( on );

        // restart delay if was paused
        if (paused_ && filter_->type() == FrameBufferFilter::FILTER_DELAY)
            replay();

        // toggle state
        paused_ = !on;
    }
}

bool CloneSource::playable () const
{
    return filter_->type() != FrameBufferFilter::FILTER_PASSTHROUGH;
}

void CloneSource::replay()
{
    // reset Filter
    filter_->reset();
}

guint64 CloneSource::playtime () const
{
    if (filter_->type() != FrameBufferFilter::FILTER_PASSTHROUGH)
        return guint64( filter_->updateTime() * GST_SECOND ) ;
    if (origin_)
        return origin_->playtime();
    return 0;
}


uint CloneSource::texture() const
{
    if (origin_)
        return origin_->frame()->texture();
    return Resource::getTextureBlack();
}

Source::Failure CloneSource::failed() const
{
    return (origin_ == nullptr || origin_->failed()) ? FAIL_FATAL : FAIL_NONE;
}

void CloneSource::accept(Visitor& v)
{
    Source::accept(v);
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
