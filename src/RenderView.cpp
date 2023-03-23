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

#include <thread>

// Opengl
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtx/vector_angle.hpp>

#include "defines.h"
#include "Settings.h"
#include "Primitives.h"

#include "RenderView.h"

const char* RenderView::ratio_preset_name[6] = { "4:3", "3:2", "16:10", "16:9", "21:9", "Custom" };
glm::vec2 RenderView::ratio_preset_value[6] = { glm::vec2(4.f,3.f), glm::vec2(3.f,2.f), glm::vec2(16.f,10.f), glm::vec2(16.f,9.f), glm::vec2(21.f,9.f) , glm::vec2(1.f,2.f) };
const char* RenderView::height_preset_name[5] = { "720", "1080", "1200", "1440", "2160" };
float RenderView::height_preset_value[5] = { 720.f, 1080.f, 1200.f, 1440.f, 2160.f };


glm::vec3 RenderView::resolutionFromPreset(int ar, int h)
{
    float width = ratio_preset_value[ar].x * height_preset_value[h] / ratio_preset_value[ar].y;
    width -= (int)width % 2;
    return glm::vec3( width, height_preset_value[h] , 0.f);
}

glm::ivec2 RenderView::presetFromResolution(glm::vec3 resolution)
{
    static int num_height = ((int)(sizeof(RenderView::height_preset_value) / sizeof(*RenderView::height_preset_value)));
    static int num_ar = ((int)(sizeof(RenderView::ratio_preset_value) / sizeof(*RenderView::ratio_preset_value)));

    glm::ivec2 ret = glm::ivec2(AspectRatio_Custom, -1);

    // get height parameter
    for(int h = 0; h < num_height; h++) {
        if ( ABS_DIFF(resolution.y, RenderView::height_preset_value[h]) < 1.f){
            ret.y = h;
            break;
        }
    }
    // found a preset height: find if it is a preset ratio
    if (ret.y > -1) {
        // get aspect ratio parameter
        float myratio = resolution.x / resolution.y;
        for(int ar = 0; ar < num_ar; ar++) {
            if ( ABS_DIFF( myratio, RenderView::ratio_preset_value[ar].x / RenderView::ratio_preset_value[ar].y ) < 0.01f){
                ret.x = ar;
                break;
            }
        }
    }
    // not a preset height (NB : ratio is custom)
    else {
        // find closest height preset
        float diff = MAXFLOAT;
        for(int h = 0; h < num_height; h++) {
            if ( ABS_DIFF(resolution.y, RenderView::height_preset_value[h]) < diff){
                diff = ABS_DIFF(resolution.y, RenderView::height_preset_value[h]);
                ret.y = h;
            }
        }
    }

    return ret;
}

RenderView::RenderView() : View(RENDERING), frame_buffer_(nullptr), fading_overlay_(nullptr), frame_thumbnail_(nullptr)
{
}

RenderView::~RenderView()
{
    if (frame_buffer_)
        delete frame_buffer_;
    if (fading_overlay_)
        delete fading_overlay_;
    if (frame_thumbnail_)
        delete frame_thumbnail_;
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
    // read settings if invalid resolution is given
    if (resolution.x < 2.f || resolution.y < 2.f) {
        if (Settings::application.render.ratio < AspectRatio_Custom)
            resolution = resolutionFromPreset(Settings::application.render.ratio, Settings::application.render.res);
        else
            resolution = glm::vec3(Settings::application.render.custom_width, Settings::application.render.custom_height, 0.f);
    }

    // do we need to change resolution ?
    if (frame_buffer_ && frame_buffer_->resolution() != resolution)  {

        // new frame buffer
        delete frame_buffer_;
        frame_buffer_ = nullptr;
    }

    if (!frame_buffer_) {
        // output frame is an RBG Multisamples FrameBuffer
        FrameBuffer::FrameBufferFlags flag = FrameBuffer::FrameBuffer_multisampling;
        if (useAlpha)
            flag |= FrameBuffer::FrameBuffer_alpha;
        frame_buffer_ = new FrameBuffer(resolution, flag);
    }

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
                // set resolution of the thumbnail
                glm::vec3 res_thumbnail( round(SESSION_THUMBNAIL_HEIGHT * frame_buffer_->aspectRatio()), SESSION_THUMBNAIL_HEIGHT, 0.f);

                // try to reuse the stored frame_thumbnail_ FBO
                if ( frame_thumbnail_ != nullptr && frame_thumbnail_->resolution() != res_thumbnail ) {
                    delete frame_thumbnail_;
                    frame_thumbnail_ = nullptr;
                }

                // new thumbnailing framebuffer if necessary
                if (frame_thumbnail_ == nullptr)
                    frame_thumbnail_ = new FrameBuffer( res_thumbnail );

                // render
                if ( !frame_buffer_->blit(frame_thumbnail_) ){
                    // render anyway if blit failed
                    FrameBufferSurface *thumb = new FrameBufferSurface(frame_buffer_);
                    frame_thumbnail_->begin();
                    thumb->draw(glm::identity<glm::mat4>(), frame_thumbnail_->projection());
                    frame_thumbnail_->end();
                    delete thumb;
                }

                // return valid thumbnail promise
                thumbnailer_.back().set_value( frame_thumbnail_->image() );

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
