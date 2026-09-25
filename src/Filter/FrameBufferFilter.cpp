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

#include "IconsVimixImage.h"
#include "FrameBuffer.h"
#include "Resource.h"
#include "Visitor/Visitor.h"

#include "FrameBufferFilter.h"

std::vector< std::tuple<int, int, std::string> > FrameBufferFilter::Types = {
    { ICON_VI_FILTER_NONE, std::string("None") },
    { ICON_VI_FILTER_DELAY, std::string("Delay") },
    { ICON_VI_FILTER_RESAMPLE, std::string("Resample") },
    { ICON_VI_FILTER_BLUR, std::string("Blur") },
    { ICON_VI_FILTER_SHARPEN, std::string("Sharpen") },
    { ICON_VI_FILTER_SMOOTH, std::string("Smooth & Noise") },
    { ICON_VI_FILTER_EDGE, std::string("Edge") },
    { ICON_VI_FILTER_ALPHA, std::string("Alpha") },
    { ICON_VI_FILTER_IMAGE, std::string("Custom shader") }
};

FrameBufferFilter::FrameBufferFilter() : enabled_(true), input_(nullptr)
{

}

void FrameBufferFilter::draw (FrameBuffer *input)
{
    if (input && ( enabled_ || input_ == nullptr ) )
        input_ = input;
}

void FrameBufferFilter::accept(Visitor& v)
{
    if (input_)
        v.visit(*this);
}

PassthroughFilter::PassthroughFilter() : FrameBufferFilter()
{

}

uint PassthroughFilter::texture() const
{
    if (input_)
        return input_->texture();
    else
        return Resource::getTextureBlack();
}

glm::vec3 PassthroughFilter::resolution() const
{
    if (input_)
        return input_->resolution();
    else
        return glm::vec3(1,1,0);
}
