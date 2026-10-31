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
#include "Scene/Decorations.h"
#include "Visitor/Visitor.h"

#include "MultiFileSource.h"
#include "IconsVimixImage.h"

MultiFileSource::MultiFileSource (uint64_t id) : MediaSource(id, new MultiFilePlayer)
{
    // set symbol
    symbol_ = new Symbol(Symbol::SEQUENCE, glm::vec3(0.75f, 0.75f, 0.01f));
    symbol_->scale_.y = 1.5f;
}

MultiFilePlayer *MultiFileSource::multifileplayer () const
{
    return static_cast<MultiFilePlayer *>(mediaplayer_);
}

void MultiFileSource::setFiles (const std::list<std::string> &list_files, uint framerate)
{
    setSequence(MultiFileSequence(list_files), framerate);
}

void MultiFileSource::setSequence (const MultiFileSequence &sequence, uint framerate)
{
    // a media player can be opened only once
    if ( mediaplayer_->isOpen() )
        return;

    // path of the source is the location pattern of the sequence
    path_ = sequence.location;

    // open gstreamer
    multifileplayer()->open( sequence, CLAMP( framerate, 1, 30) );

    // will be ready after init and one frame rendered
    ready_ = false;
}

MultiFileSequence MultiFileSource::sequence () const
{
    return multifileplayer()->sequence();
}

void MultiFileSource::setFramerate (uint framerate)
{
    multifileplayer()->setFramerate( CLAMP( framerate, 1, 30) );
}

uint MultiFileSource::framerate () const
{
    return multifileplayer()->framerate();
}

void MultiFileSource::accept (Visitor& v)
{
    MediaSource::accept(v);
    v.visit(*this);
}

glm::ivec2 MultiFileSource::icon () const
{
    return glm::ivec2(ICON_VI_SOURCE_SEQUENCE);
}

std::string_view MultiFileSource::type() const
{
    return SOURCE_TYPE_MULTI_FILE;
}

std::string MultiFileSource::info() const
{
    return "Image Sequence ( " + std::to_string(sequence().max - sequence().min) + " )";
}
