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

#include "Source/CanvasSource.h"    
#include "Scene/Primitives.h"
#include "View/GeometryView.h"

#include "Canvas.h"

Canvas::Canvas() : canvas_surface_(nullptr)
{
    canvas_scene_ = new Group;
    // create first canvas
    addCanvas();
}

void Canvas::setFrameBuffer(FrameBuffer *fb)
{
    // create surface to render canvases
    if ( canvas_surface_ == nullptr) 
        canvas_surface_ = new FrameBufferSurface(fb);
    
    // update canvas surface framebuffer
    canvas_surface_->setFrameBuffer(fb);
    canvas_surface_->scale_.x = fb->aspectRatio();
}

void Canvas::update()
{
    // Render canvases
    for (auto cit = canvasBegin(); cit != canvasEnd(); ++cit) {
        // render each canvas with their section of the background surface
        (*cit)->update(0.f);
        (*cit)->render();
    }
}

void Canvas::addCanvas()
{
    // create a new canvas source
    CanvasSource *canvas = new CanvasSource;
    canvas->setName( "Canvas " + std::to_string( canvases_.size() + 1) );

    // add to list of canvases
    canvases_.push_back( canvas );

    // activate it
    canvas->setActive( true );

    // attach to geometry view scene 
    canvas_scene_->attach( canvas->groups_[View::GEOMETRY] );
    canvas_scene_->attach( canvas->frames_[View::GEOMETRY] );
    
}

void Canvas::removeCanvas()
{
    if ( canvases_.size() > 1 ) {

        canvas_scene_->detach( canvases_.back()->groups_[View::GEOMETRY] );
        canvas_scene_->detach( canvases_.back()->frames_[View::GEOMETRY] );

        // remove last canvas
        canvases_.pop_back();
    }
}

SourceList::iterator Canvas::canvasBegin ()
{
    return canvases_.begin();
}

SourceList::iterator Canvas::canvasEnd ()
{
    return canvases_.end();
}