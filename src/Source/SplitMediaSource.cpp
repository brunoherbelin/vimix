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

#include "defines.h"
#include "Visitor/Visitor.h"
#include "Player/SplitMediaPlayer.h"
#include "IconsVimixImage.h"
#include "Scene/Decorations.h"

#include "SplitMediaSource.h"

SplitMediaSource::SplitMediaSource(uint64_t id) : MediaSource(id, new SplitMediaPlayer)
{
    // split media symbol (instead of default video symbol of MediaSource::init)
    symbol_ = new Symbol(Symbol::VIDEOSPLIT, glm::vec3(0.75f, 0.75f, 0.01f));
    symbol_->scale_.y = 1.5f;
}

SplitMediaPlayer *SplitMediaSource::splitmediaplayer() const
{
    return static_cast<SplitMediaPlayer *>(mediaplayer_);
}

void SplitMediaSource::setFiles(const std::list<std::string> &list_files)
{
    // a media player can be opened only once
    if ( mediaplayer_->isOpen() )
        return;

    // path of the source is the first file
    path_ = list_files.empty() ? "" : list_files.front();

    // prepare audio flag before openning
    mediaplayer_->setAudioEnabled( audio_flags_ & Source::Audio_enabled );

    // open gstreamer
    splitmediaplayer()->open(list_files);

    // will be ready after init and one frame rendered
    ready_ = false;
}

std::list<std::string> SplitMediaSource::files() const
{
    return splitmediaplayer()->files();
}

glm::ivec2 SplitMediaSource::icon() const
{
    return glm::ivec2(ICON_VI_SOURCE_VIDEOSPLIT);
}

std::string_view SplitMediaSource::type() const
{
    return SOURCE_TYPE_SPLIT_MEDIA;
}

std::string SplitMediaSource::info() const
{
    return "Video Sequence ( " + std::to_string(files().size()) + " )";
}

void SplitMediaSource::accept(Visitor& v)
{
    MediaSource::accept(v);
    v.visit(*this);
}
