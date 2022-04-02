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

#include <string>
#include <sstream>
#include <glm/gtc/matrix_transform.hpp>

#include "defines.h"
#include "ImageShader.h"
#include "Resource.h"
#include "Decorations.h"
#include "Stream.h"
#include "Visitor.h"
#include "Log.h"
#include "BaseToolkit.h"

#include "StreamSource.h"


GenericStreamSource::GenericStreamSource(uint64_t id) : StreamSource(id)
{
    // create stream
    stream_ = new Stream;

    // set symbol
    symbol_ = new Symbol(Symbol::EMPTY, glm::vec3(0.75f, 0.75f, 0.01f));
    symbol_->scale_.y = 1.5f;
}

void GenericStreamSource::setDescription(const std::string &desc)
{
    gst_description_ = desc;
    gst_elements_ = BaseToolkit::splitted(desc, '!');
    Log::Notify("Creating Source with Stream description '%s'", gst_description_.c_str());

    // open gstreamer
    stream_->open(gst_description_ + " ! queue max-size-buffers=10 ! videoconvert" );
    stream_->play(true);

    // will be ready after init and one frame rendered
    ready_ = false;
}


std::string GenericStreamSource::description() const
{
    return gst_description_;
}

std::list<std::string> GenericStreamSource::gstElements() const
{
    return gst_elements_;
}

void GenericStreamSource::accept(Visitor& v)
{
    Source::accept(v);
    if (!failed())
        v.visit(*this);
}

glm::ivec2 GenericStreamSource::icon() const
{
    return glm::ivec2(ICON_SOURCE_GSTREAMER);
}

std::string GenericStreamSource::info() const
{
    return "Custom gstreamer";
}

StreamSource::StreamSource(uint64_t id) : Source(id), stream_(nullptr)
{
}

StreamSource::~StreamSource()
{
    // delete stream
    if (stream_)
        delete stream_;
}

bool StreamSource::failed() const
{
    return (stream_ != nullptr &&  stream_->failed() );
}

uint StreamSource::texture() const
{
    if (stream_ == nullptr)
        return Resource::getTextureBlack();
    else
        return stream_->texture();
}

void StreamSource::init()
{
    if ( stream_ && stream_->isOpen() ) {

        // update video
        stream_->update();

        // once the texture of media player is created
        if (stream_->texture() != Resource::getTextureBlack()) {

            // get the texture index from media player, apply it to the media surface
            texturesurface_->setTextureIndex( stream_->texture() );

            // create Frame buffer matching size of media player
            float height = float(stream_->width()) / stream_->aspectRatio();
            FrameBuffer *renderbuffer = new FrameBuffer(stream_->width(), (uint)height, true);

            // set the renderbuffer of the source and attach rendering nodes
            attach(renderbuffer);

            // force update of activation mode
            active_ = true;

            // deep update to reorder
            ++View::need_deep_update_;

            // done init
            Log::Info("Source '%s' linked to Stream %s", name().c_str(), std::to_string(stream_->id()).c_str());
        }
    }

}

void StreamSource::setActive (bool on)
{
    bool was_active = active_;

    // try to activate (may fail if source is cloned)
    Source::setActive(on);

    if (stream_) {
        // change status of stream (only if status changed)
        if (active_ != was_active)
            stream_->enable(active_);

        // change visibility of active surface (show preview of stream when inactive)
        if (activesurface_) {
            if (active_)
                activesurface_->setTextureIndex(Resource::getTextureTransparent());
            else
                activesurface_->setTextureIndex(stream_->texture());
        }
    }
}


bool StreamSource::playing () const
{
    if ( stream_ )
        return stream_->isPlaying();
    return false;
}

void StreamSource::play (bool on)
{
    if ( stream_ )
        stream_->play(on);
}


bool StreamSource::playable () const
{
    if ( stream_ )
        return !stream_->singleFrame();
    return false;
}

void StreamSource::replay ()
{
    if ( stream_ )
        stream_->rewind();
}

guint64 StreamSource::playtime () const
{
    if ( stream_ )
        return stream_->position();
    else
        return 0;
}

void StreamSource::update(float dt)
{
    Source::update(dt);

    // update stream
    if (stream_)
        stream_->update();
}
