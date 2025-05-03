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
#include "Decorations.h"
#include "Visitor.h"
#include "FrameBuffer.h"
#include "FrameBufferFilter.h"
#include "DelayFilter.h"

#include "ShaderSource.h"

ShaderSource::ShaderSource(uint64_t id) : Source(id), paused_(false), filter_(nullptr)
{
    // set symbol
    symbol_ = new Symbol(Symbol::CODE, glm::vec3(0.75f, 0.75f, 0.01f));
    symbol_->scale_.y = 1.5f;

    // default
    filter_ = new ImageFilter;
    filter_->setProgram(FilteringProgram::example_patterns.front());
    background_ = new FrameBuffer(64, 64);
}

ShaderSource::~ShaderSource()
{
    delete filter_;
    delete background_;
}


void ShaderSource::setResolution(glm::vec3 resolution)
{
    if (background_)
        delete background_;

    background_ = new FrameBuffer(resolution);
}

void ShaderSource::setProgram(const FilteringProgram &f)
{
    filter_->setProgram(f);
}

void ShaderSource::init()
{
    if (background_) {

        // create render Frame buffer matching size of images
        FrameBuffer *renderbuffer = new FrameBuffer( background_->resolution() );

        // set the renderbuffer of the source and attach rendering nodes
        attach(renderbuffer);

        // force update of activation mode
        active_ = true;

        // deep update to reorder
        ++View::need_deep_update_;

        // done init
        Log::Info("Source '%s' shader.", name().c_str());
    }
}

void ShaderSource::render()
{
    if ( renderbuffer_ == nullptr )
        init();
    else {
        // render filter image
        filter_->draw( background_ );

        // ensure correct output texture is displayed (could have changed if filter changed)
        texturesurface_->setTextureIndex( filter_->texture() );

        // detect resampling (change of resolution in filter)
        if ( renderbuffer_->resolution() != filter_->resolution() ) {
            renderbuffer_->resize( filter_->resolution() );
        }

        // render textured surface into frame buffer
        // NB: this also applies the color correction shader
        renderbuffer_->begin();
        texturesurface_->draw(glm::identity<glm::mat4>(), renderbuffer_->projection());
        renderbuffer_->end();
        ready_ = true;
    }
}

void ShaderSource::setActive (bool on)
{
    bool was_active = active_;

    // try to activate (may fail if source is cloned)
    Source::setActive(on);

    // enable / disable filtering
    if ( active_ != was_active )
        filter_->setEnabled( active_ );

}

void ShaderSource::update(float dt)
{
    Source::update(dt);

    if (!paused_ && active_)
        filter_->update(dt);

}

void ShaderSource::play (bool on)
{
    // if a different state is asked
    if (paused_ == on) {

        // play / pause filter to suspend clone
        filter_->setEnabled( on );

        // restart delay if was paused
        if (paused_)
            replay();

        // toggle state
        paused_ = !on;
    }
}

bool ShaderSource::playable () const
{
    return true;
}

void ShaderSource::replay()
{
    // reset Filter
    filter_->reset();
}

void ShaderSource::reload()
{
    replay();
}

guint64 ShaderSource::playtime () const
{
    if (filter_)
        return guint64( filter_->updateTime() * GST_SECOND ) ;

    return 0;
}

uint ShaderSource::texture() const
{
    return filter_->texture();
}

Source::Failure ShaderSource::failed() const
{
    return FAIL_NONE;
}

void ShaderSource::accept(Visitor& v)
{
    Source::accept(v);
    v.visit(*this);
}

glm::ivec2 ShaderSource::icon() const
{
    return glm::ivec2(ICON_SOURCE_SHADER);
}

std::string ShaderSource::info() const
{
    return "Shader";
}
