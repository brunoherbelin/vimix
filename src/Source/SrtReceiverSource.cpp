/*
 * This file is part of vimix - video live mixer
 *
 * **Copyright** (C) 2019-2024 Bruno Herbelin <bruno.herbelin@gmail.com>
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

#include "Log.h"
#include "Scene/Decorations.h"
#include "Visitor/Visitor.h"

#include "SrtReceiverSource.h"


// gst-launch-1.0 rtspsrc location=rtsp://192.168.0.19:5500/camera ! decodebin !  videoconvert  ! autovideosink
// gst-launch-1.0 ristsrc address=0.0.0.0 port=7072 ! rtpmp2tdepay ! tsdemux ! decodebin !  videoconvert  ! autovideosink

// gst-launch-1.0 uridecodebin uri=rist://127.0.0.1:7072 ! videoconvert  ! autovideosink
// gst-launch-1.0 uridecodebin uri=http://192.168.0.19:8080/video ! queue ! videoconvert  ! autovideosink
// gst-launch-1.0 uridecodebin uri=rtsp://192.168.0.19:5500/camera ! queue ! videoconvert  ! autovideosink


SrtReceiverSource::SrtReceiverSource(uint64_t id) : StreamSource(id)
{
    // create stream
    stream_ = new Stream;

    // set symbol
    symbol_ = new Symbol(Symbol::RECEIVE, glm::vec3(0.75f, 0.75f, 0.01f));
    symbol_->scale_.y = 1.5f;
}

Source::Failure SrtReceiverSource::failed() const
{
    return (stream_ != nullptr &&  stream_->failed()) ? FAIL_RETRY : FAIL_NONE;
}


void SrtReceiverSource::setConnection(const std::string &ip, const std::string &port)
{
    // TODO add check on wellformed IP and PORT
    ip_ = ip;
    port_ = port;
    Log::Notify("Creating Source SRT receiving from '%s'", uri().c_str());

    std::string description = "srtsrc uri=" + uri() + " ! queue ! decodebin ! videoconvert";

    // open gstreamer
    stream_->open(description);
    stream_->play(true);

    // delete and reset render buffer to enforce re-init of StreamSource
    if (renderbuffer_)
        delete renderbuffer_;
    renderbuffer_ = nullptr;

    // will be ready after init and one frame rendered
    ready_ = false;
}

std::string SrtReceiverSource::uri() const
{
    return std::string("srt://") + ip_ + ":" + port_;
}

void SrtReceiverSource::accept(Visitor& v)
{
    StreamSource::accept(v);
    v.visit(*this);
}

glm::ivec2 SrtReceiverSource::icon() const
{
    return glm::ivec2(ICON_SOURCE_SRT);
}

std::string SrtReceiverSource::info() const
{
    return "SRT receiver";
}
