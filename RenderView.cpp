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

#include <thread>

// Opengl
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtx/vector_angle.hpp>

#include "defines.h"
#include "Settings.h"
#include "Decorations.h"

#include "RenderView.h"


RenderView::RenderView() : View(RENDERING), frame_buffer_(nullptr), fading_overlay_(nullptr)
{
}

RenderView::~RenderView()
{
    if (frame_buffer_)
        delete frame_buffer_;
    if (fading_overlay_)
        delete fading_overlay_;
}

void RenderView::setFading(float f)
{
    if (fading_overlay_ == nullptr)
        fading_overlay_ = new Surface;

    fading_overlay_->shader()->color.a = CLAMP( f < EPSILON ? 0.f : f, 0.f, 1.f);
}

float RenderView::fading() const
{
    if (fading_overlay_)
        return fading_overlay_->shader()->color.a;
    else
        return 0.f;
}

void RenderView::setResolution(glm::vec3 resolution, bool useAlpha)
{
    // use default resolution if invalid resolution is given (default behavior)
    if (resolution.x < 2.f || resolution.y < 2.f)
        resolution = FrameBuffer::getResolutionFromParameters(Settings::application.render.ratio, Settings::application.render.res);

    // do we need to change resolution ?
    if (frame_buffer_ && frame_buffer_->resolution() != resolution)  {

        // new frame buffer
        delete frame_buffer_;
        frame_buffer_ = nullptr;
    }

    if (!frame_buffer_)
        // output frame is an RBG Multisamples FrameBuffer
        frame_buffer_ = new FrameBuffer(resolution, useAlpha, true);

    // reset fading
    setFading();
}

void RenderView::draw()
{
    static glm::mat4 projection = glm::ortho(-1.f, 1.f, 1.f, -1.f, -SCENE_DEPTH, 1.f);

    if (frame_buffer_) {
        // draw in frame buffer
        glm::mat4 P  = glm::scale( projection, glm::vec3(1.f / frame_buffer_->aspectRatio(), 1.f, 1.f));

        // render the scene normally (pre-multiplied alpha in RGB)
        frame_buffer_->begin();
        scene.root()->draw(glm::identity<glm::mat4>(), P);
        fading_overlay_->draw(glm::identity<glm::mat4>(), projection);
        frame_buffer_->end();
    }
}

void RenderView::drawThumbnail()
{
    if (frame_buffer_) {
        // if a thumbnailer is pending
        if (thumbnailer_.size() > 0) {

            try {
                // new thumbnailing framebuffer
                FrameBuffer *frame_thumbnail = new FrameBuffer( glm::vec3(SESSION_THUMBNAIL_HEIGHT * frame_buffer_->aspectRatio(), SESSION_THUMBNAIL_HEIGHT, 1.f) );

                // render
                if (Settings::application.render.blit) {
                    if ( !frame_buffer_->blit(frame_thumbnail) )
                        throw std::runtime_error("no blit");
                }
                else {
                    FrameBufferSurface *thumb = new FrameBufferSurface(frame_buffer_);
                    frame_thumbnail->begin();
                    thumb->draw(glm::identity<glm::mat4>(), frame_thumbnail->projection());
                    frame_thumbnail->end();
                    delete thumb;
                }

                // return valid thumbnail promise
                thumbnailer_.back().set_value( frame_thumbnail->image() );

                // done with thumbnailing framebuffer
                delete frame_thumbnail;
            }
            catch(...) {
                // return failed thumbnail promise
                thumbnailer_.back().set_exception(std::current_exception());
            }

            // done with this promise
            thumbnailer_.pop_back();
        }
    }
}

FrameBufferImage *RenderView::thumbnail ()
{
    // by default null image
    FrameBufferImage *img = nullptr;

    // this function is always called from a parallel thread
    // So we wait for a few frames of rendering before trying to capture a thumbnail
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // create and store a promise for a FrameBufferImage
    thumbnailer_.emplace_back( std::promise<FrameBufferImage *>() );

    // future will return the promised FrameBufferImage
    std::future<FrameBufferImage *> ft = thumbnailer_.back().get_future();

    try {
        // wait for a valid return value from promise
        img = ft.get();
    }
    // catch any failed promise
    catch (const std::exception&){
    }

    return img;
}
