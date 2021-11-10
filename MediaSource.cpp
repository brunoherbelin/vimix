/*
 * This file is part of vimix - video live mixer
 *
 * **Copyright** (C) 2020-2021 Bruno Herbelin <bruno.herbelin@gmail.com>
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

#include <glm/gtc/matrix_transform.hpp>

#include "defines.h"
#include "ImageShader.h"
#include "ImageProcessingShader.h"
#include "Resource.h"
#include "Decorations.h"
#include "MediaPlayer.h"
#include "Visitor.h"
#include "Log.h"

#include "MediaSource.h"

MediaSource::MediaSource(uint64_t id) : Source(id), path_("")
{
    // create media player
    mediaplayer_ = new MediaPlayer;
}

MediaSource::~MediaSource()
{
    // delete media player
    delete mediaplayer_;
}

void MediaSource::setPath(const std::string &p)
{
    path_ = p;
    Log::Notify("Creating Source with media '%s'", path_.c_str());

    // open gstreamer
    mediaplayer_->open(path_);
    mediaplayer_->play(true);

    // will be ready after init and one frame rendered
    ready_ = false;
}

std::string MediaSource::path() const
{
    return path_;
}

MediaPlayer *MediaSource::mediaplayer() const
{
    return mediaplayer_;
}

glm::ivec2 MediaSource::icon() const
{
    if (mediaplayer_->isImage())
        return glm::ivec2(4, 9);
    else
        return glm::ivec2(18, 13);
}

bool MediaSource::failed() const
{
    return mediaplayer_->failed();
}

uint MediaSource::texture() const
{
    return mediaplayer_->texture();
}

void MediaSource::init()
{
    if ( mediaplayer_->isOpen() ) {

        // update video
        mediaplayer_->update();

        // once the texture of media player is created
        if (mediaplayer_->texture() != Resource::getTextureBlack()) {

            // get the texture index from media player, apply it to the media surface
            texturesurface_->setTextureIndex( mediaplayer_->texture() );

            // create Frame buffer matching size of media player
            float height = float(mediaplayer_->width()) / mediaplayer_->aspectRatio();
            FrameBuffer *renderbuffer = new FrameBuffer(mediaplayer_->width(), (uint)height, true);

            // icon in mixing view
            if (mediaplayer_->isImage())
                symbol_ = new Symbol(Symbol::IMAGE, glm::vec3(0.75f, 0.75f, 0.01f));
            else
                symbol_ = new Symbol(Symbol::VIDEO, glm::vec3(0.75f, 0.75f, 0.01f));
            symbol_->scale_.y = 1.5f;

            // set the renderbuffer of the source and attach rendering nodes
            attach(renderbuffer);

            // force update of activation mode
            active_ = true;

            // deep update to reorder
            ++View::need_deep_update_;

            // done init
            Log::Info("Source '%s' linked to Media %s.", name().c_str(), std::to_string(mediaplayer_->id()).c_str());
        }
    }

}

void MediaSource::setActive (bool on)
{
    bool was_active = active_;

    // try to activate (may fail if source is cloned)
    Source::setActive(on);

    // change status of media player (only if status changed)
    if ( active_ != was_active )
        mediaplayer_->enable(active_);

    // change visibility of active surface (show preview of media when inactive)
    if (activesurface_) {
        if (active_)
            activesurface_->setTextureIndex(Resource::getTextureTransparent());
        else
            activesurface_->setTextureIndex(mediaplayer_->texture());
    }
}


bool MediaSource::playing () const
{
    return mediaplayer_->isPlaying();
}

void MediaSource::play (bool on)
{
    mediaplayer_->play(on);
}

bool MediaSource::playable () const
{
    return !mediaplayer_->isImage();
}

void MediaSource::replay ()
{
    mediaplayer_->rewind();
}

guint64 MediaSource::playtime () const
{
    return mediaplayer_->position();
}

void MediaSource::update(float dt)
{
    Source::update(dt);

    // update video
    mediaplayer_->update();
}

void MediaSource::render()
{
    if ( renderbuffer_ == nullptr )
        init();
    else {
        // render the media player into frame buffer
        renderbuffer_->begin();
        // apply fading
        texturesurface_->shader()->color = glm::vec4( glm::vec3(mediaplayer_->currentTimelineFading()), 1.f);
        texturesurface_->draw(glm::identity<glm::mat4>(), renderbuffer_->projection());
        renderbuffer_->end();
        ready_ = true;
    }
}

void MediaSource::accept(Visitor& v)
{
    Source::accept(v);
    v.visit(*this);
}
