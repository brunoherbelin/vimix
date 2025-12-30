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

CanvasSource::CanvasSource(uint64_t id) : Source(id), 
    rendered_output_(nullptr), rendered_surface_(nullptr), paused_(false), reset_(true)
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

CanvasSource::~CanvasSource()
{
    // detatch from canvas rendering scene
    // so that it is not deleted each time a Canvas is deleted
    canvas_rendering_scene_.ws()->detach(Canvas::manager().canvas_surface_);

    if (rendered_output_ != nullptr)
        delete rendered_output_;
}

Source::Failure CanvasSource::failed() const
{
    // if ( rendered_output_ != nullptr && renderview_ != nullptr ) {
    //     // the rendering output was created, but resolution changed
    //     if ( rendered_output_->resolution() != renderview_->frame()->resolution()
    //          // or alpha channel changed (e.g. inside SessionSource)
    //          || ( ( rendered_output_->flags() & FrameBuffer::FrameBuffer_alpha ) != ( renderview_->frame()->flags() & FrameBuffer::FrameBuffer_alpha ) )
    //          ) {
    //         return FAIL_RETRY;
    //     }
    // }

    return FAIL_NONE;
}

uint CanvasSource::texture() const
{
    if (rendered_output_ != nullptr)
        return rendered_output_->texture();
    else if (Canvas::manager().canvas_surface_ && Canvas::manager().canvas_surface_->frameBuffer())
        return Canvas::manager().canvas_surface_->frameBuffer()->texture();
    else
        return Resource::getTextureBlack(); // getTextureTransparent ?
}

void CanvasSource::init()
{
    if (Canvas::manager().canvas_surface_ 
        && Canvas::manager().canvas_surface_->frameBuffer() 
        && Canvas::manager().canvas_surface_->frameBuffer()->texture() != Resource::getTextureBlack()) {

        // attach canvas surface to rendering scene (drawn by this source in update)
        canvas_rendering_scene_.ws()->detach(Canvas::manager().canvas_surface_); // avoid double attach
        canvas_rendering_scene_.ws()->attach(Canvas::manager().canvas_surface_);
        canvas_rendering_scene_.update(0.f);

        FrameBuffer *fb = Canvas::manager().canvas_surface_->frameBuffer();
        // use same flags than rendering frame, without multisampling
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

        // deep update to reorder
        ++View::need_deep_update_;

        // done init
        Log::Info("Canvas '%s' linked to output (%d x %d).", name().c_str(), int(fb->resolution().x), int(fb->resolution().y) );
    }
}

void CanvasSource::update(float dt)
{
    static glm::mat4 projection = glm::ortho(-1.f, 1.f, 1.f, -1.f, -SCENE_DEPTH, 1.f);

    Source::update(dt);

    if (Canvas::manager().canvas_surface_ 
        && Canvas::manager().canvas_surface_->frameBuffer() 
        && rendered_output_ && renderbuffer_) {
    
        if ((active_ && !paused_) || reset_) {
            
            //  the scene root transform to the inverse of the source transform
            glm::vec3 rotation = this->groups_[View::GEOMETRY]->rotation_;
            glm::vec3 scale = this->groups_[View::GEOMETRY]->scale_;
            scale.z = 1.f;
            glm::vec3 translation =  this->groups_[View::GEOMETRY]->translation_;
            translation.z = 0.f;

            // change projection to account for CROP (inverse transform)
            glm::vec3 _c_s = glm::vec3(groups_[View::GEOMETRY]->crop_[0] - groups_[View::GEOMETRY]->crop_[1],
                    groups_[View::GEOMETRY]->crop_[2] - groups_[View::GEOMETRY]->crop_[3],
                    2.f) * 0.5f ;
            glm::vec3 _t((_c_s.x + groups_[View::GEOMETRY]->crop_[1]),
                            (_c_s.y + groups_[View::GEOMETRY]->crop_[3]), 0.f);
            
            glm::mat4 P = glm::scale(glm::translate(projection, _t), _c_s * 
                glm::vec3(-1.f / Canvas::manager().canvas_surface_->frameBuffer()->aspectRatio(), 1.f, 1.f));

            rendered_output_->begin();
            // access to private RenderView in the session to call draw on the root of the scene
            canvas_rendering_scene_.root()->draw(glm::inverse( GlmToolkit::transform(translation, rotation, scale) ), P);
            rendered_output_->end();

            // resize labels according to scale
            label_0_->scale_.y = 0.05f / scale.y;
            label_1_->scale_.y = 0.05f / scale.y;
            label_0_->translation_.x = -0.015f / scale.x;
            label_1_->translation_.x = 0.015f / scale.x;

            // done reset
            reset_ = false;
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
    else if (Canvas::manager().canvas_surface_ && Canvas::manager().canvas_surface_->frameBuffer())
        return Canvas::manager().canvas_surface_->frameBuffer()->resolution();
    else
        return glm::vec3(0.f);
}

