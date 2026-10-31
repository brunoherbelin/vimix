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

#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>

#include "defines.h"
#include "FrameBuffer.h"
#include "Resource.h"
#include "Scene/Decorations.h"
#include "Scene/Primitives.h"
#include "Toolkit/SystemToolkit.h"

#include "SessionThumbnailSource.h"

SessionThumbnailSource::SessionThumbnailSource(const std::string &path) : Source(), path_(path),
    missing_(!SystemToolkit::file_exists(path)), texture_(0), rendered_(false), image_(nullptr)
{
    setName( SystemToolkit::base_filename(path_) );

    // read session information (and thumbnail) in a thread
    infoLoader_ = std::async(std::launch::async, SessionCreator::info, path_, true);

    createFrames();
}

SessionThumbnailSource::SessionThumbnailSource(FrameBufferImage *image, const std::string &name) : Source(),
    missing_(false), texture_(0), rendered_(false), image_(image)
{
    setName( name );

    createFrames();
}

void SessionThumbnailSource::createFrames()
{
    // specific node for presentation view
    frames_[View::PRESENTATION] = new Switch;
    Frame *frame = new Frame(Frame::ROUND, Frame::THIN, Frame::DROP);  // visible
    frame->translation_.z = 0.1;
    frame->color = glm::vec4( COLOR_DEFAULT_SOURCE, 0.85f);
    frames_[View::PRESENTATION]->attach(frame);
    frame = new Frame(Frame::ROUND, Frame::THIN, Frame::DROP);         // selected
    frame->translation_.z = 0.1;
    frame->color = glm::vec4( COLOR_DEFAULT_SOURCE, 1.f);
    frames_[View::PRESENTATION]->attach(frame);
    frame = new Frame(Frame::ROUND, Frame::LARGE, Frame::DROP);        // current
    frame->translation_.z = 0.01;
    frame->color = glm::vec4( COLOR_TRANSITION_SOURCE, 1.f);
    frames_[View::PRESENTATION]->attach(frame);
    groups_[View::PRESENTATION]->attach(frames_[View::PRESENTATION]);

    // default aspect ratio until thumbnail is loaded
    frames_[View::PRESENTATION]->scale_.x = 16.f / 9.f;

    // dimmed frames for missing file
    if (missing_) {
        for (uint i = 0; i < frames_[View::PRESENTATION]->numChildren(); ++i)
            static_cast<Frame *>(frames_[View::PRESENTATION]->child(i))->color.a = 0.3f;
    }
}

SessionThumbnailSource::~SessionThumbnailSource()
{
    // wait for end of thread, and free the thumbnail if any
    if (infoLoader_.valid()) {
        SessionInformation info = infoLoader_.get();
        if (info.thumbnail)
            delete info.thumbnail;
    }
    if (image_)
        delete image_;

    if (texture_)
        glDeleteTextures(1, &texture_);
}

uint SessionThumbnailSource::texture() const
{
    return texture_ ? texture_ : Resource::getTextureBlack();
}

void SessionThumbnailSource::init()
{
    FrameBufferImage *thumbnail = image_;
    image_ = nullptr;

    // wait for session information to be loaded
    if ( !path_.empty() ) {
        if ( !infoLoader_.valid() ||
             infoLoader_.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready )
            return;
        thumbnail = infoLoader_.get().thumbnail;
    }

    // default size of image if no thumbnail (16:9)
    int h = static_cast<int>(SESSION_THUMBNAIL_HEIGHT);
    int w = 16 * h / 9;

    // create texture from thumbnail
    if (thumbnail) {
        w = thumbnail->width;
        h = thumbnail->height;
        glGenTextures(1, &texture_);
        glBindTexture( GL_TEXTURE_2D, texture_);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGB8, w, h);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, thumbnail->rgb);
        glBindTexture(GL_TEXTURE_2D, 0);
        delete thumbnail;
    }

    // set the texture to draw in the frame buffer
    texturesurface_->setTextureIndex( texture() );

    // set the renderbuffer of the source and attach rendering nodes
    attach( new FrameBuffer(w, h) );

    // dedicated surface to show the frame buffer in presentation view
    FrameBufferSurface *surface = new FrameBufferSurface(renderbuffer_);
    groups_[View::PRESENTATION]->attach(surface);

    // scale all nodes to match aspect ratio
    const float AR = renderbuffer_->aspectRatio();
    for (NodeSet::iterator node = groups_[View::PRESENTATION]->begin();
         node != groups_[View::PRESENTATION]->end(); ++node) {
        (*node)->scale_.x = AR;
    }

    // request deep update to reorder
    ++View::need_deep_update_;
}

void SessionThumbnailSource::render()
{
    if ( renderbuffer_ == nullptr )
        init();
    // render the static image only once into frame buffer
    else if ( !rendered_ ) {
        if ( renderbuffer_->begin() ) {
            texturesurface_->draw(glm::identity<glm::mat4>(), renderbuffer_->projection());
            renderbuffer_->end();
            rendered_ = true;
        }
        ready_ = true;
    }
}
