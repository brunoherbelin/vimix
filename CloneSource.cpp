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

#include <glm/gtc/matrix_access.hpp>

#include "Log.h"
#include "Resource.h"
#include "Visitor.h"
#include "Decorations.h"

#include "CloneSource.h"


CloneSource *Source::clone(uint64_t id)
{
    CloneSource *s = new CloneSource(this, id);

    clones_.push_back(s);

    return s;
}


CloneSource::CloneSource(Source *origin, uint64_t id) : Source(id), origin_(origin)
{
    name_ = origin->name();
    // set symbol
    symbol_ = new Symbol(Symbol::CLONE, glm::vec3(0.75f, 0.75f, 0.01f));
    symbol_->scale_.y = 1.5f;
}

CloneSource::~CloneSource()
{
    if (origin_)
        origin_->clones_.remove(this);
}

CloneSource *CloneSource::clone(uint64_t id)
{
    // do not clone a clone : clone the original instead
    if (origin_)
        return origin_->clone(id);
    else
        return nullptr;
}

void CloneSource::init()
{
    if (origin_ && origin_->mode_ > Source::UNINITIALIZED) {

        // get the texture index from framebuffer of view, apply it to the surface
        texturesurface_->setTextureIndex( origin_->texture() );

        // create Frame buffer matching size of session
        FrameBuffer *renderbuffer = new FrameBuffer( origin_->frame()->resolution(), true);

        // set the renderbuffer of the source and attach rendering nodes
        attach(renderbuffer);

        // deep update to reorder
        ++View::need_deep_update_;

        // done init
        Log::Info("Source %s cloning source %s.", name().c_str(), origin_->name().c_str() );
    }
}

void CloneSource::setActive (bool on)
{
    active_ = on;

    groups_[View::RENDERING]->visible_ = active_;
    groups_[View::GEOMETRY]->visible_ = active_;
    groups_[View::LAYER]->visible_ = active_;

    if (origin_) {
        if ( mode_ > Source::UNINITIALIZED)
            origin_->touch();

        // change visibility of active surface (show preview of origin when inactive)
        if (activesurface_) {
            if (active_)
                activesurface_->setTextureIndex(Resource::getTextureTransparent());
            else
                activesurface_->setTextureIndex(origin_->texture());
        }
    }
}

uint CloneSource::texture() const
{
    if (origin_ != nullptr)
        return origin_->texture();
    else
        return Resource::getTextureBlack();
}

void CloneSource::accept(Visitor& v)
{
    Source::accept(v);
    if (!failed())
        v.visit(*this);
}

glm::ivec2 CloneSource::icon() const
{
    return glm::ivec2(ICON_SOURCE_CLONE);
}

std::string CloneSource::info() const
{
    return std::string("clone of '") + origin_->name() + "'";
}
