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

#include <glib.h>
#include <glm/ext/matrix_transform.hpp>
#include <glm/fwd.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/matrix.hpp>
#include <gtk/gtk.h>

#include "defines.h"
#include "Log.h"
#include "FrameBuffer.h"
#include "Scene/Decorations.h"
#include "Resource.h"
#include "Visitor/Visitor.h"
#include "Visitor/DrawVisitor.h"
#include "Canvas.h"

#include "CanvasSource.h"

CanvasSurface::CanvasSurface(uint64_t id) : Source(id), paused_(false)
{
    // set symbol
    symbol_ = new Symbol(Symbol::SCREEN, glm::vec3(0.75f, 0.75f, 0.01f));
    symbol_->scale_.y = 1.0f;

    // default visible 
    groups_[View::MIXING]->translation_ = glm::vec3( 0.f );
    groups_[View::LAYER]->translation_.z = 1.f;
    groups_[View::RENDERING]->visible_ = true;

    label = new Group;
    label_0_ = new Character(4);
    label_0_->setChar('C');
    label_0_->translation_ = glm::vec3(-0.015f, 0.f, 0.f);
    label_0_->scale_.y = 0.05f;
    label->attach(label_0_);
    label_1_ = new Character(4);
    label_1_->setChar('A');
    label_1_->translation_ = glm::vec3(0.015f, 0.f, 0.f);
    label_1_->scale_.y = 0.05f;
    label->attach(label_1_);
    overlays_[View::GEOMETRY]->attach(label);

    // colorize all frames and handles in GEOMETRY view
    ColorVisitor color(glm::vec4(COLOR_FRAME, 0.95f));
    groups_[View::GEOMETRY]->accept(color);
    
}

CanvasSurface::~CanvasSurface()
{
    Log::Info("Canvas '%s' deleted.", name().c_str());
}

Source::Failure CanvasSurface::failed() const
{
    return FAIL_NONE;
}

uint CanvasSurface::texture() const
{
    if (renderbuffer_ != nullptr)
        return renderbuffer_->texture();
    else if (Canvas::manager().framebuffer_)
        return Canvas::manager().framebuffer_->texture();
    else
        return Resource::getTextureBlack(); // getTextureTransparent ?
}

void CanvasSurface::init()
{
    if (Canvas::manager().framebuffer_ 
        && Canvas::manager().framebuffer_->texture() != Resource::getTextureBlack()) {

        // create Frame buffer matching size of output session
        FrameBuffer *renderbuffer = new FrameBuffer( Canvas::manager().framebuffer_->resolution() );

        // set the renderbuffer of the source and attach rendering nodes
        attach(renderbuffer);

        // set output texture
        texturesurface_->setTextureIndex( Canvas::manager().framebuffer_->texture() );

        // done init
        Log::Info("Canvas '%s' linked to output (%d x %d).", name().c_str(), int(renderbuffer->resolution().x), int(renderbuffer->resolution().y) );
    }
}

void CanvasSurface::render ()
{    
    static glm::mat4 projection = glm::ortho(-1.f, 1.f, 1.f, -1.f, -SCENE_DEPTH, 1.f);

    if ( renderbuffer_ == nullptr )
        init();
    else {

        // the scene root transform to the inverse of the source transform
        glm::vec3 rotation = groups_[View::GEOMETRY]->rotation_;
        glm::vec3 scale = groups_[View::GEOMETRY]->scale_;
        scale.z = 1.f;
        glm::vec3 translation = groups_[View::GEOMETRY]->translation_;
        translation.z = 0.f;
        translation.x /= Canvas::manager().framebuffer_->aspectRatio();

        // // ensure correct output texture is displayed (could have changed if canvas framebuffer changed)
        // texturesurface_->setTextureIndex( Canvas::manager().framebuffer_->texture() );

        // render 
        renderbuffer_->begin();
        texturesurface_->draw(glm::inverse( GlmToolkit::transform(translation, rotation, scale) ), projection);
        renderbuffer_->end();

        ready_ = true;
    }
}

void CanvasSurface::update(float dt)
{
    Source::update(dt);

    // resize labels according to scale
    glm::vec3 scale = this->groups_[View::GEOMETRY]->scale_;
    label_0_->scale_.y = 0.05f / scale.y;
    label_1_->scale_.y = 0.05f / scale.y;
    label_0_->translation_.x = -0.015f / scale.x;
    label_1_->translation_.x = 0.015f / scale.x;

}

void CanvasSurface::play (bool on)
{
}

void CanvasSurface::reload ()
{
    // reset renderbuffer_
    if (renderbuffer_)
        delete renderbuffer_;
    renderbuffer_ = nullptr;
}

glm::vec3 CanvasSurface::resolution() const
{
    if (renderbuffer_ != nullptr)
        return renderbuffer_->resolution();
    else if (Canvas::manager().framebuffer_)
        return Canvas::manager().framebuffer_->resolution();
    else
        return glm::vec3(0.f);
}



CanvasSource::CanvasSource(uint64_t id) : Source(id), canvas_index_(0),  rendered_output_(nullptr),
    paused_(false), reset_(true)
{
    // set symbol TODO CANVAS SYMBOL
    symbol_ = new Symbol(Symbol::RECEIVE, glm::vec3(0.75f, 0.75f, 0.01f));
    symbol_->scale_.y = 1.5f;

    // default visible 
    groups_[View::MIXING]->translation_ = glm::vec3( 0.f );
    groups_[View::LAYER]->translation_.z = 1.f;

    // colorize all frames and handles in GEOMETRY view
    ColorVisitor color(glm::vec4(COLOR_WINDOW, 0.95f));
    groups_[View::GEOMETRY]->accept(color);
}

CanvasSource::~CanvasSource()
{
    if (rendered_output_ != nullptr)
        delete rendered_output_;
}

Source::Failure CanvasSource::failed() const
{
    CanvasSurface *canvas = Canvas::manager().at(canvas_index_);

    // the canvas surface does not exist anymore
    if (canvas == nullptr)
        return FAIL_FATAL;
    // the canvas surface exists but resolution changed
    else if ( rendered_output_ != nullptr && renderbuffer_ != nullptr ) {
        if ( rendered_output_->resolution() != canvas->frame()->resolution()
             // or alpha channel changed (e.g. inside SessionSource)
             || ( ( rendered_output_->flags() & FrameBuffer::FrameBuffer_alpha ) != ( canvas->frame()->flags() & FrameBuffer::FrameBuffer_alpha ) )
             ) {
            return FAIL_RETRY;
        }
    }

    return FAIL_NONE;
}

uint CanvasSource::texture() const
{
    if (rendered_output_ != nullptr)
        return rendered_output_->texture();
    else
        return Resource::getTextureBlack(); // getTextureTransparent ?
}

void CanvasSource::init()
{
    CanvasSurface *canvas = Canvas::manager().at(canvas_index_);

    if (canvas && canvas->frame() && canvas->frame()->texture() != Resource::getTextureBlack()) {

        FrameBuffer *fb = canvas->frame();
        
        // use same flags than canvas frame, without multisampling
        FrameBuffer::FrameBufferFlags flag = fb->flags();
        flag &= ~FrameBuffer::FrameBuffer_multisampling;

        // create the frame buffer displayed by the source (all modes)
        if (rendered_output_ != nullptr)
            delete rendered_output_;
        rendered_output_ = new FrameBuffer( fb->resolution(), flag );

        // needs a first initialization (to get texture)
        fb->blit(rendered_output_);

        // set the texture index from internal framebuffer, apply it to the source texture surface
        texturesurface_->setTextureIndex( rendered_output_->texture() );

        // create Frame buffer matching size of output session
        FrameBuffer *renderbuffer = new FrameBuffer( fb->resolution() );

        // set the renderbuffer of the source and attach rendering nodes
        attach(renderbuffer);

        // adjust aspect ratio of source to match canvas
        groups_[View::GEOMETRY]->scale_.x = canvas->group(View::GEOMETRY)->scale_.x;

        // deep update to reorder
        ++View::need_deep_update_;

        // done init
        Log::Info("Source '%s' linked to output (%d x %d).", name().c_str(), int(fb->resolution().x), int(fb->resolution().y) );
    }
}

void CanvasSource::update(float dt)
{
    static glm::mat4 projection = glm::ortho(-1.f, 1.f, 1.f, -1.f, -SCENE_DEPTH, 1.f);

    Source::update(dt);

    CanvasSurface *canvas = Canvas::manager().at(canvas_index_);
    if (canvas && canvas->frame() && rendered_output_) {

        if ((active_ && !paused_) || reset_) {

            canvas->frame()->blit(rendered_output_);
        }
    }
}

void CanvasSource::play (bool on)
{
    // toggle state
    paused_ = !on;
}

void CanvasSource::replay ()
{
    // request next frame to reset
    reset_ = true;
}

void CanvasSource::reload ()
{
    // reset renderbuffer_
    if (renderbuffer_)
        delete renderbuffer_;
    renderbuffer_ = nullptr;

    // request next frame to reset
    reset_ = true;
}

glm::vec3 CanvasSource::resolution() const
{
    if (rendered_output_ != nullptr)
        return rendered_output_->resolution();
    else
        return glm::vec3(0.f);
}

void CanvasSource::accept(Visitor& v)
{
    Source::accept(v);
    v.visit(*this);
}

glm::ivec2 CanvasSource::icon() const
{
    return glm::ivec2(ICON_SOURCE); // TODO NEW ICON SOURCE_CANVAS
}

std::string CanvasSource::info() const
{
    return "Canvas source";
}
