/*
 * This file is part of vimix - video live mixer
 *
 * **Copyright** (C) 2019-2026 Bruno Herbelin <bruno.herbelin@gmail.com>
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

#include "Visitor/Visitor.h"
#include "AudioPlayer.h"
#include "Resource.h"
#include "IconsVimixImage.h"
#include "Scene/Decorations.h"

#include "AudioSource.h"

AudioSource::AudioSource(uint64_t id) : MediaSource(id, new AudioPlayer)
{
    // audio is always enabled
    audio_flags_ = Source::Audio_available | Source::Audio_enabled;

    // volume follows alpha in mixing view and timeline fading
    audio_volume_mix_ = Source::Volume_mult_alpha | Source::Volume_mult_opacity
                      | Source::Volume_mult_parent | Source::Volume_mult_session;
    audio_volume_[VOLUME_BASE] = 0.5f;

    // icon in mixing view (instead of default video symbol of MediaSource::init)
    // TODO: dedicated audio symbol
    symbol_ = new Symbol(Symbol::MUSIC, glm::vec3(0.75f, 0.75f, 0.01f));
    symbol_->scale_.y = 1.5f;
}

AudioPlayer *AudioSource::audioplayer() const
{
    return static_cast<AudioPlayer *>(mediaplayer_);
}

uint AudioSource::texture() const
{
    // no image to give to the mix
    // (the visualization of the audio is in the frame buffer)
    return Resource::getTextureTransparent();
}

void AudioSource::updateAudio()
{
    // audio cannot be disabled (volume can be 0)
    audio_flags_ |= Source::Audio_available | Source::Audio_enabled;

    MediaSource::updateAudio();
}

glm::ivec2 AudioSource::icon() const
{
    return glm::ivec2(ICON_VI_SOURCE_AUDIO);
}

std::string AudioSource::info() const
{
    return "Audio File";
}

void AudioSource::accept(Visitor& v)
{
    MediaSource::accept(v);
    v.visit(*this);
}
