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

#include <glm/gtc/matrix_transform.hpp>

#include "defines.h"
#include "Log.h"
#include "FrameBuffer.h"
#include "Decorations.h"
#include "Resource.h"
#include "Visitor.h"
#include "Session.h"

#include "RenderSource.h"

const char* RenderSource::rendering_provenance_label[2] = { "Loopback", "Non-recursive" };

RenderSource::RenderSource(uint64_t id) : Source(id), session_(nullptr), runtime_(0), rendered_output_(nullptr), rendered_surface_(nullptr),
    paused_(false), provenance_(RENDER_TEXTURE)
{
    // set symbol
    symbol_ = new Symbol(Symbol::RENDER, glm::vec3(0.75f, 0.75f, 0.01f));
    symbol_->scale_.y = 1.5f;
}

RenderSource::~RenderSource()
{
    if (rendered_output_ != nullptr)
        delete rendered_output_;
}

bool RenderSource::failed() const
{
    if ( rendered_output_ != nullptr && session_ != nullptr )
        return rendered_output_->resolution() != session_->frame()->resolution();

    return false;
}

uint RenderSource::texture() const
{
    if (rendered_output_ != nullptr)
        return rendered_output_->texture();
    else if (session_ && session_->frame())
        return session_->frame()->texture();
    else
        return Resource::getTextureBlack(); // getTextureTransparent ?
}

//void RenderSource::setActive (bool on)
//{

//}

void RenderSource::init()
{
    if (session_ && session_->frame() && session_->frame()->texture() != Resource::getTextureBlack()) {

        FrameBuffer *fb = session_->frame();

        // get the texture index from framebuffer of view for RENDER_TEXTURE mode
//        rendered_surface_ = new Surface;
//        rendered_surface_->setTextureIndex( fb->texture() );

        // use same flags than session frame, without multisampling
        FrameBuffer::FrameBufferFlags flag = fb->flags();
        flag &= ~FrameBuffer::FrameBuffer_multisampling;

        // create the frame buffer displayed by the source (all modes)
        rendered_output_ = new FrameBuffer( fb->resolution(), flag );

        // needs a first initialization (to get texture)
        fb->blit(rendered_output_);

        // set the texture index from internal framebuffer, apply it to the source texture surface
        texturesurface_->setTextureIndex( rendered_output_->texture() );

        // create Frame buffer matching size of output session
        FrameBuffer *renderbuffer = new FrameBuffer( fb->resolution() );

        // set the renderbuffer of the source and attach rendering nodes
        attach(renderbuffer);

        // deep update to reorder
        ++View::need_deep_update_;

        // done init
        Log::Info("Source '%s' linked to output (%d x %d).", name().c_str(), int(fb->resolution().x), int(fb->resolution().y) );
    }
}

void RenderSource::update(float dt)
{
    Source::update(dt);

    if (!paused_ && session_ && rendered_output_) {

        if (provenance_ == RENDER_EXCLUSIVE) {
            // temporarily exclude this RenderSource from the rendering
            groups_[View::RENDERING]->visible_ = false;
            // simulate a rendering of the session in a framebuffer
            static glm::mat4 projection = glm::ortho(-1.f, 1.f, 1.f, -1.f, -SCENE_DEPTH, 1.f);
            glm::mat4 P  = glm::scale( projection, glm::vec3(1.f / rendered_output_->aspectRatio(), 1.f, 1.f));
            rendered_output_->begin();
            // access to private RenderView in the session to call draw on the root of the scene
            session_->render_.scene.root()->draw(glm::identity<glm::mat4>(), P);
            rendered_output_->end();
            // restore this RenderSource visibility
            groups_[View::RENDERING]->visible_ = true;
        }
        else
            session_->frame()->blit(rendered_output_);

        runtime_ = session_->runtime();

        //        rendered_output_->begin(true); // if not blit
        //        rendered_surface_->draw(glm::identity<glm::mat4>(), rendered_output_->projection());
        //        rendered_output_->end();
    }

}

void RenderSource::play (bool on)
{
    // toggle state
    paused_ = !on;
}

glm::vec3 RenderSource::resolution() const
{
    if (rendered_output_ != nullptr)
        return rendered_output_->resolution();
    else if (session_ && session_->frame())
        return session_->frame()->resolution();
    else
        return glm::vec3(0.f);
}

void RenderSource::accept(Visitor& v)
{
    Source::accept(v);
    if (!failed())
        v.visit(*this);
}

glm::ivec2 RenderSource::icon() const
{
    return glm::ivec2(ICON_SOURCE_RENDER);
}

std::string RenderSource::info() const
{
    return "Rendering Output";
}
