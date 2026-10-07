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
#include "ImageShader.h"
#include "FrameBuffer.h"
#include "Scene/Decorations.h"
#include "Scene/Primitives.h"
#include "Visitor/Visitor.h"
#include "Interpolator.h"

#include "DraftSource.h"

DraftSource::DraftSource(Source *origin, uint64_t id) : CloneSource(origin, id), shared_(false)
{
    // registered as depending on its origin (keeps origin active)
    origin->clones_.push_back(this);

    // no line connecting to origin
    connection_->visible_ = false;

    // same symbol as origin
    if (origin->symbol_) {
        delete symbol_;
        symbol_ = new Symbol(origin->symbol_->type(), origin->symbol_->translation_);
        symbol_->scale_ = origin->symbol_->scale_;
    }
}

void DraftSource::init()
{
    if (origin_ && origin_->ready_ && origin_->mode_ > Source::UNINITIALIZED && origin_->renderbuffer_) {

        // create render Frame buffer matching size of origin
        // NB: GPU memory is allocated only when rendering for the first time
        FrameBuffer *renderbuffer = new FrameBuffer( origin_->renderbuffer_->resolution(),
                                                     origin_->renderbuffer_->flags() );

        // set the renderbuffer of the source and attach rendering nodes
        attach(renderbuffer);

        // force update of activation mode
        active_ = true;

        // deep update to reorder
        ++View::need_deep_update_;
    }
}

void DraftSource::render()
{
    // initialize, and render immediately if possible
    if ( renderbuffer_ == nullptr ) {
        init();
        if ( renderbuffer_ == nullptr )
            return;
    }

    if ( origin_ == nullptr || origin_->renderbuffer_ == nullptr)
        return;

    // share content of origin if content properties are the same
    shared_ = imageProcessingEnabled() == origin_->imageProcessingEnabled()
              && SourceCoreField::diff(*this, *origin_, SourceCoreField::CONTENT) == 0;

    // show the frame of origin, or the content rendered by this source
    FrameBuffer *fb = shared_ ? origin_->renderbuffer_ : renderbuffer_;
    if (rendersurface_->frameBuffer() != fb) {
        rendersurface_->setFrameBuffer(fb);
        mixingsurface_->setFrameBuffer(fb);
    }

    if (!shared_) {
        // follow resolution of origin
        if ( renderbuffer_->resolution() != origin_->renderbuffer_->resolution() )
            renderbuffer_->resize( origin_->renderbuffer_->resolution() );

        // render the content of origin with the properties of this source
        // NB: this also applies the color correction shader
        texturesurface_->setTextureIndex( origin_->texturesurface_->textureIndex() );
        texturesurface_->shader()->color = origin_->texturesurface_->shader()->color;
        ImageShader *os = dynamic_cast<ImageShader *>(origin_->texturesurface_->shader());
        ImageShader *ds = dynamic_cast<ImageShader *>(texturesurface_->shader());
        if (os && ds)
            ds->premultiply = os->premultiply;
        if ( renderbuffer_->begin() ) {
            texturesurface_->draw(glm::identity<glm::mat4>(), renderbuffer_->projection());
            renderbuffer_->end();
        }
    }

    ready_ = true;
}

FrameBuffer *DraftSource::frame() const
{
    if (shared_ && origin_)
        return origin_->frame();

    return Source::frame();
}

void DraftSource::update(float dt)
{
    // no filtering
    Source::update(dt);
}

void DraftSource::setActive (bool on)
{
    Source::setActive(on);
}

bool DraftSource::playing () const
{
    return origin_ ? origin_->playing() : false;
}

void DraftSource::play (bool on)
{
}

bool DraftSource::playable () const
{
    return false;
}

void DraftSource::replay ()
{
}

void DraftSource::reload ()
{
}

guint64 DraftSource::playtime () const
{
    return origin_ ? origin_->playtime() : 0;
}

uint DraftSource::texture() const
{
    return origin_ ? origin_->texture() : Resource::getTextureBlack();
}

Source::Failure DraftSource::failed() const
{
    return origin_ == nullptr ? FAIL_CRITICAL : FAIL_NONE;
}

bool DraftSource::texturePostProcessed() const
{
    return origin_ ? origin_->texturePostProcessed() : false;
}

void DraftSource::accept(Visitor& v)
{
    Source::accept(v);
    v.visit(*this);
}

glm::ivec2 DraftSource::icon() const
{
    return origin_ ? origin_->icon() : glm::ivec2(ICON_VI_SOURCE_CLONE);
}

std::string DraftSource::info() const
{
    return origin_ ? origin_->info() : "Draft";
}
