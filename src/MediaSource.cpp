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


#include <glm/gtc/matrix_transform.hpp>

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

    // prepare audio flag before openning
    mediaplayer_->setAudioEnabled( audio_flags_ & Source::Audio_enabled );

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
        return glm::ivec2(ICON_SOURCE_IMAGE);
    else
        return glm::ivec2(ICON_SOURCE_VIDEO);
}

std::string MediaSource::info() const
{
    if (mediaplayer_->isImage())
        return "Image File";
    else
        return "Video File";
}

Source::Failure MediaSource::failed() const
{
    return mediaplayer_->failed() ? FAIL_CRITICAL : FAIL_NONE;
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
            FrameBuffer *renderbuffer = new FrameBuffer(mediaplayer_->width(), (uint)height, FrameBuffer::FrameBuffer_alpha);

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

            // deep update to reorder (two frames to give time to insert)
            View::need_deep_update_ += 2;

            // test audio is available
            if (mediaplayer_->audioAvailable())
                audio_flags_ |= Source::Audio_available;

            // done init
            Log::Info("Source '%s' linked to MediaPlayer %s.", name().c_str(), std::to_string(mediaplayer_->id()).c_str());
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
    return !mediaplayer_->singleFrame();
}

void MediaSource::replay ()
{
    mediaplayer_->rewind();
}

void MediaSource::reload ()
{
    mediaplayer_->reopen();
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

void MediaSource::updateAudio()
{
    // update enable/ disable status of audio of media player (do nothing if no change)
    mediaplayer_->setAudioEnabled( audio_flags_ & Source::Audio_enabled );

    // update audio volume if enabled
    if (audio_flags_ & Source::Audio_enabled) {

        // base volume
        gdouble new_vol = (gdouble) (audio_volume_[VOLUME_BASE]);

        // apply factors
        if (audio_volume_mix_ & Source::Volume_mult_alpha)
            new_vol *= (gdouble) (alpha());
        if (audio_volume_mix_ & Source::Volume_mult_opacity)
            new_vol *= (gdouble) (mediaplayer_->currentTimelineFading());
        if (audio_volume_mix_ & Source::Volume_mult_parent)
            new_vol *= (gdouble) (audio_volume_[VOLUME_PARENT]);
        if (audio_volume_mix_ & Source::Volume_mult_session)
            new_vol *= (gdouble) (audio_volume_[VOLUME_SESSION]);

        // implementation for media player gstreamer pipeline
        mediaplayer_->setAudioVolume(new_vol);
    }
}

void MediaSource::render()
{
    if ( renderbuffer_ == nullptr )
        init();
    else {
        // render the media player into frame buffer
        // NB: this also applies the color correction shader
        renderbuffer_->begin();
        // apply fading
        float __f = mediaplayer_->currentTimelineFading();
        setAudioVolumeFactor(Source::VOLUME_OPACITY, __f);
        if (mediaplayer_->timelineFadingMode() != MediaPlayer::FADING_ALPHA) {
            // color fading
            texturesurface_->shader()->color = glm::vec4( glm::vec3(mediaplayer_->currentTimelineFading()), 1.f);
        }
        else {
            // alpha fading
            texturesurface_->shader()->color = glm::vec4( glm::vec3(1.f), mediaplayer_->currentTimelineFading());
        }
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

bool MediaSource::texturePostProcessed() const
{
    if (Source::texturePostProcessed())
        return true;

    return !mediaplayer_->timeline()->fadingIsClear() ||
           mediaplayer_->loopStatus() == MediaPlayer::LOOP_STATUS_BLACKOUT;
}

