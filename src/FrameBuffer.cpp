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

#include <sstream>

#include "FrameBuffer.h"
#include "Resource.h"
#include "Settings.h"
#include "Log.h"

#include <glm/gtc/matrix_transform.hpp>

#include <glad/glad.h>
#include <stb_image.h>
#include <stb_image_write.h>

#ifndef NDEBUG
#define FRAMEBUFFER_DEBUG
#endif


FrameBuffer::FrameBuffer(glm::vec3 resolution, FrameBufferFlags flags): flags_(flags),
    textureid_(0), multisampling_textureid_(0), framebufferid_(0), multisampling_framebufferid_(0), mem_usage_(0)
{
    attrib_.viewport = glm::ivec2(resolution);
    setProjectionArea(glm::vec2(1.f, 1.f));
    attrib_.clear_color = glm::vec4(0.f, 0.f, 0.f, 0.f);
}

FrameBuffer::FrameBuffer(uint width, uint height, FrameBufferFlags flags): flags_(flags),
    textureid_(0), multisampling_textureid_(0), framebufferid_(0), multisampling_framebufferid_(0), mem_usage_(0)
{
    attrib_.viewport = glm::ivec2(width, height);
    setProjectionArea(glm::vec2(1.f, 1.f));
    attrib_.clear_color = glm::vec4(0.f, 0.f, 0.f, 0.f);
}

void FrameBuffer::init()
{
    mem_usage_ = 0;

    // generate texture
    glGenTextures(1, &textureid_);
    glBindTexture(GL_TEXTURE_2D, textureid_);

    // create texture with Mipmapping with multiple levels
    if (flags_ & FrameBuffer_mipmap) {
        glTexStorage2D(GL_TEXTURE_2D, MIPMAP_LEVEL + 1, (flags_ & FrameBuffer_alpha) ? GL_RGBA8 : GL_RGB8, attrib_.viewport.x, attrib_.viewport.y);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    }
    // default : create simple texture for RGB(A)
    else  {
        glTexStorage2D(GL_TEXTURE_2D, 1, (flags_ & FrameBuffer_alpha) ? GL_RGBA8 : GL_RGB8, attrib_.viewport.x, attrib_.viewport.y);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    }

    // calculate GPU memory usage (for debug only)
    mem_usage_ += ( attrib_.viewport.x * attrib_.viewport.y * (flags_ & FrameBuffer_alpha?4:3) ) / 1024;

    // common texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_2D, 0);

    // create a framebuffer object
    glGenFramebuffers(1, &framebufferid_);
    glBindFramebuffer(GL_FRAMEBUFFER, framebufferid_);

#ifdef FRAMEBUFFER_DEBUG
        g_printerr("Framebuffer %d created (%d x %d) - ", framebufferid_, attrib_.viewport.x, attrib_.viewport.y);
#endif

// Always disable multisampling under Mac OSX (unsupported OpenGL feature)
#ifndef TARGET_OS_OSX
    // or take settings into account: no multisampling if application multisampling is level 0
    if ( Settings::application.render.multisampling < 1 )
#endif
    flags_ &= ~FrameBuffer_multisampling;

    if (flags_ & FrameBuffer_multisampling){

        // create a multisample texture
        glGenTextures(1, &multisampling_textureid_);
        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, multisampling_textureid_);
        glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, Settings::application.render.multisampling,
                                (flags_ & FrameBuffer_alpha) ? GL_RGBA8 : GL_RGB8, attrib_.viewport.x, attrib_.viewport.y, GL_TRUE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0);

        // attach the multisampled texture to FBO (framebufferid_  currently binded)
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, multisampling_textureid_, 0);

        // create an intermediate FBO : this is the FBO to use for reading
        glGenFramebuffers(1, &multisampling_framebufferid_);
        glBindFramebuffer(GL_FRAMEBUFFER, multisampling_framebufferid_);

        // calculate GPU memory usage
        mem_usage_ += ( Settings::application.render.multisampling * attrib_.viewport.x * attrib_.viewport.y * (flags_ & FrameBuffer_alpha?4:3) ) / 1024;

#ifdef FRAMEBUFFER_DEBUG
        g_printerr("multi sampling (%d) - ", Settings::application.render.multisampling);
#endif
    }

    // attach the 2D texture to latest binded FBO
    // (i.e. the multisampling_framebufferid_ FBO if enabled, default framebufferid_ otherwise)
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureid_, 0);

    // attach multiple FBOs to the mipmaped texture
    if (flags_ & FrameBuffer_mipmap) {

        // calculate GPU memory usage
        int width = attrib_.viewport.x;
        int height = attrib_.viewport.y;
        for(int i=1; i < MIPMAP_LEVEL; ++i) {
            width = MAX(1, (width / 2));
            height = MAX(1, (height / 2));
            mem_usage_ += ( width * height * (flags_ & FrameBuffer_alpha?4:3) ) / 1024;
        }
#ifdef FRAMEBUFFER_DEBUG
        g_printerr("mipmap (%d) - ", MIPMAP_LEVEL);
#endif
    }

    checkFramebufferStatus();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

#ifdef FRAMEBUFFER_DEBUG
    g_printerr("~%d kB allocated\n", mem_usage_);
#endif
}

FrameBuffer::~FrameBuffer()
{
#ifdef FRAMEBUFFER_DEBUG
    g_printerr("Framebuffer %d deleted - ~%d kB freed\n", framebufferid_, mem_usage_);
#endif

    if (framebufferid_)
        glDeleteFramebuffers(1, &framebufferid_);
    if (multisampling_framebufferid_)
        glDeleteFramebuffers(1, &multisampling_framebufferid_);
    if (textureid_)
        glDeleteTextures(1, &textureid_);
    if (multisampling_textureid_)
        glDeleteTextures(1, &multisampling_textureid_);
}

uint FrameBuffer::texture() const
{
    if (framebufferid_ == 0)
        return Resource::getTextureBlack();

    return textureid_;
}

float FrameBuffer::aspectRatio() const
{
    return static_cast<float>(attrib_.viewport.x) / static_cast<float>(attrib_.viewport.y);
}


std::string FrameBuffer::info() const
{
    std::ostringstream info;
    info << attrib_.viewport.x << "x" << attrib_.viewport.y << "px";

    return info.str();
}

glm::vec3 FrameBuffer::resolution() const
{
    return glm::vec3(attrib_.viewport.x, attrib_.viewport.y, 0.f);
}

void FrameBuffer::resize(glm::vec3 res)
{
    if (framebufferid_) {
        if (attrib_.viewport.x != res.x || attrib_.viewport.y != res.y)
        {
            // de-init
            glDeleteFramebuffers(1, &framebufferid_);
            framebufferid_ = 0;

            if (multisampling_framebufferid_)
                glDeleteFramebuffers(1, &multisampling_framebufferid_);
            multisampling_framebufferid_ = 0;

            if (textureid_)
                glDeleteTextures(1, &textureid_);
            textureid_ = 0;

            if (multisampling_textureid_)
                glDeleteTextures(1, &multisampling_textureid_);
            multisampling_textureid_ = 0;

            // change resolution
            attrib_.viewport = glm::ivec2(res);
            mem_usage_ = 0;
        }
    }
}

void FrameBuffer::begin(bool clear)
{
    if (!framebufferid_)
        init();

    glBindFramebuffer(GL_FRAMEBUFFER, framebufferid_);

    Rendering::manager().pushAttrib(attrib_);

    if (clear)
        glClear(GL_COLOR_BUFFER_BIT);
}

void FrameBuffer::end()
{    
    // if multisampling frame buffer
    if (flags_ & FrameBuffer_multisampling) {
        // blit the multisample FBO into unisample FBO to generate 2D texture
        // Doing this blit will automatically resolve the multisampled FBO.
        glBindFramebuffer(GL_READ_FRAMEBUFFER, framebufferid_);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, multisampling_framebufferid_);
        glBlitFramebuffer(0, 0, attrib_.viewport.x, attrib_.viewport.y,
                          0, 0, attrib_.viewport.x, attrib_.viewport.y, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    }

    FrameBuffer::release();

    if (flags_ & FrameBuffer_mipmap) {
        glBindTexture(GL_TEXTURE_2D, textureid_);
        glGenerateMipmap(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    Rendering::manager().popAttrib();
}

void FrameBuffer::release()
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void FrameBuffer::readPixels()
{
    if (!framebufferid_) {
#ifdef FRAMEBUFFER_DEBUG
        g_printerr("FrameBuffer readPixels failed\n");
#endif
        return;
    }

    if (flags_ & FrameBuffer_multisampling)
        glBindFramebuffer(GL_READ_FRAMEBUFFER, multisampling_framebufferid_);
    else
        glBindFramebuffer(GL_READ_FRAMEBUFFER, framebufferid_);

    if (flags_ & FrameBuffer_alpha)
        glPixelStorei(GL_PACK_ALIGNMENT, 4);
    else
        glPixelStorei(GL_PACK_ALIGNMENT, 1);

    glReadPixels(0, 0, attrib_.viewport.x, attrib_.viewport.y, ((flags_ & FrameBuffer_alpha)? GL_RGBA : GL_RGB), GL_UNSIGNED_BYTE, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

bool FrameBuffer::blit(FrameBuffer *destination)
{
    if (!framebufferid_ || !destination || (flags_ & FrameBuffer_alpha) != (destination->flags_ & FrameBuffer_alpha) ){
#ifdef FRAMEBUFFER_DEBUG
        g_printerr("FrameBuffer blit failed\n");
#endif
        return false;
    }

    if (!destination->framebufferid_)
        destination->init();

    if (flags_ & FrameBuffer_multisampling)
        glBindFramebuffer(GL_READ_FRAMEBUFFER, multisampling_framebufferid_);
    else
        glBindFramebuffer(GL_READ_FRAMEBUFFER, framebufferid_);

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, destination->framebufferid_);
    // blit to the frame buffer object
    glBlitFramebuffer(0, 0, attrib_.viewport.x, attrib_.viewport.y,
                      0, 0, destination->width(), destination->height(), GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return true;
}

void FrameBuffer::checkFramebufferStatus()
{
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    switch (status){
    case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
        Log::Warning("GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT​ is returned if any of the framebuffer attachment points are framebuffer incomplete.");
        break;
    case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
        Log::Warning("GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT​ is returned if the framebuffer does not have at least one image attached to it.");
        break;
    case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
        Log::Warning("GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER​ is returned if the value of GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE​ is GL_NONE​ for any color "
                     "attachment point(s) named by GL_DRAWBUFFERi​.");
        break;
    case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
        Log::Warning("GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER​ is returned if GL_READ_BUFFER​ is not GL_NONE​ and the value of "
                     "GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE​ is GL_NONE​ for the color attachment point named by GL_READ_BUFFER.");
        break;
    case GL_FRAMEBUFFER_UNSUPPORTED:
        Log::Warning("GL_FRAMEBUFFER_UNSUPPORTED​ is returned if the combination of internal formats of the attached images violates an "
                     "implementation-dependent set of restrictions.");
        break;
    case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
        Log::Warning("GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE​ is returned if the value of GL_RENDERBUFFER_SAMPLES​ is not the same for all attached renderbuffers; "
                     "if the value of GL_TEXTURE_SAMPLES​ is the not same for all attached textures; or, if the attached images are a mix of renderbuffers and textures, the value of "
                     "GL_RENDERBUFFER_SAMPLES​ does not match the value of GL_TEXTURE_SAMPLES.\nGL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE​ is also returned if the value of "
                     "GL_TEXTURE_FIXED_SAMPLE_LOCATIONS​ is not the same for all attached textures; or, if the attached images are a mix of renderbuffers and textures, "
                     "the value of GL_TEXTURE_FIXED_SAMPLE_LOCATIONS​ is not GL_TRUE​ for all attached textures.");
        break;
    case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS:
        Log::Warning("GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS​ is returned if any framebuffer attachment is layered, and any populated attachment is not layered,"
                     " or if all populated color attachments are not from textures of the same target.");
        break;
    case GL_FRAMEBUFFER_UNDEFINED:
        Log::Warning(" GL_FRAMEBUFFER_UNDEFINED​ is returned if target​ is the default framebuffer, but the default framebuffer does not exist.");
        break;
    case GL_FRAMEBUFFER_COMPLETE:
        {

        // test available memory if created buffer is big (more than 8MB)
        if ( mem_usage_ > 8000 ) {

            // Obtain RAM usage in GPU (if possible)
            glm::ivec2 RAM = Rendering::getGPUMemoryInformation();

            // bad case: not enough RAM, we should warn the user
            if ( uint(RAM.x) < mem_usage_ * 3 ) {
                Log::Warning("Critical allocation of frame buffer: only %d kB RAM remaining in graphics card.", RAM.x );
                if (RAM.y < INT_MAX)
                    Log::Warning("Only %.1f %% of %d kB available.", 100.f*float(RAM.x)/float(RAM.y), RAM.y);
            }
        }

        }
        break;
    }

}


glm::mat4 FrameBuffer::projection() const
{
    return projection_;
}

glm::vec2 FrameBuffer::projectionArea() const
{
    return projection_area_;
}

void FrameBuffer::setProjectionArea(glm::vec2 c)
{
    projection_area_.x = CLAMP(c.x, 0.1f, 1.f);
    projection_area_.y = CLAMP(c.y, 0.1f, 1.f);
    projection_ = glm::ortho(-projection_area_.x, projection_area_.x, projection_area_.y, -projection_area_.y, -1.f, 1.f);
}


FrameBufferImage::FrameBufferImage(int w, int h) :
    rgb(nullptr), width(w), height(h), is_stbi(false)
{
    if (width>0 && height>0)
        rgb = new uint8_t[width*height*3];
}

FrameBufferImage::FrameBufferImage(jpegBuffer jpgimg) :
    rgb(nullptr), width(0), height(0), is_stbi(true)
{
    int c = 0;
    if (jpgimg.buffer != nullptr && jpgimg.len >0)
        rgb = stbi_load_from_memory(jpgimg.buffer, jpgimg.len, &width, &height, &c, 3);
}

FrameBufferImage::FrameBufferImage(const std::string &filename) :
    rgb(nullptr), width(0), height(0), is_stbi(true)
{
    int c = 0;
    if (!filename.empty())
        rgb = stbi_load(filename.c_str(), &width, &height, &c, 3);
}

FrameBufferImage::~FrameBufferImage()
{
    if (rgb!=nullptr) {
        if (is_stbi)
            stbi_image_free(rgb);
        else
            delete [] rgb;
    }
}

FrameBufferImage::jpegBuffer FrameBufferImage::getJpeg() const
{
    jpegBuffer jpgimg;

    // if we hold a valid image
    if (rgb!=nullptr && width>0 && height>0) {

        // dynamically allocate JPEG buffer
        stbi_write_jpg_to_func( [](void *context, void *data, int size)
        {
            uint pos = ((FrameBufferImage::jpegBuffer*)context)->len;
            ((FrameBufferImage::jpegBuffer*)context)->len += size;
            ((FrameBufferImage::jpegBuffer*)context)->buffer = (unsigned char *) realloc(((FrameBufferImage::jpegBuffer*)context)->buffer, ((FrameBufferImage::jpegBuffer*)context)->len);
            memmove(((FrameBufferImage::jpegBuffer*)context)->buffer + pos, data, size);
        }
        ,&jpgimg, width, height, 3, rgb, FBI_JPEG_QUALITY);
    }

    return jpgimg;
}

FrameBufferImage *FrameBuffer::image(){

    FrameBufferImage *img = nullptr;

    // not ready
    if (!framebufferid_)
        return img;

    if (flags_ & FrameBuffer_multisampling)
        return img;

    // allocate image
    img = new FrameBufferImage(attrib_.viewport.x, attrib_.viewport.y);

    // get pixels into image
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0); // set buffer target readpixel

    // read pixels (not multisampling)
    glBindFramebuffer(GL_READ_FRAMEBUFFER, framebufferid_);

    // for reading RGB (even on RGBA)
    glPixelStorei(GL_PACK_ALIGNMENT, 1);

    // read RGB (no alpha)
    glReadPixels(0, 0, attrib_.viewport.x, attrib_.viewport.y, GL_RGB, GL_UNSIGNED_BYTE, img->rgb);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return img;
}

bool FrameBuffer::fill(FrameBufferImage *image)
{
    if (!framebufferid_)
        init();

    // only compatible for RGB FrameBuffers
    if (flags_ & FrameBuffer_alpha || flags_ & FrameBuffer_multisampling)
        return false;

    // invalid image
    if ( image == nullptr ||
         image->rgb==nullptr ||
         image->width < 1 ||
         image->height < 1 )
        return false;

    // is it same size ?
    if (image->width == attrib_.viewport.x && image->height == attrib_.viewport.y ) {
        // directly fill texture with image
        glBindTexture(GL_TEXTURE_2D, textureid_);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, image->width, image->height,
                        GL_RGB, GL_UNSIGNED_BYTE, image->rgb);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    else {
        uint textureimage, framebufferimage;
        // generate texture
        glGenTextures(1, &textureimage);
        glBindTexture(GL_TEXTURE_2D, textureimage);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, image->width, image->height, 0, GL_RGB, GL_UNSIGNED_BYTE, image->rgb);
        glBindTexture(GL_TEXTURE_2D, 0);

        // create a framebuffer object
        glGenFramebuffers(1, &framebufferimage);
        glBindFramebuffer(GL_FRAMEBUFFER, framebufferimage);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureimage, 0);

        // blit to the frame buffer object with interpolation
        glBindFramebuffer(GL_READ_FRAMEBUFFER, framebufferimage);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebufferid_);
        glBlitFramebuffer(0, 0, image->width, image->height,
                          0, 0, attrib_.viewport.x, attrib_.viewport.y, GL_COLOR_BUFFER_BIT, GL_LINEAR);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // cleanup
        glDeleteFramebuffers(1, &framebufferimage);
        glDeleteTextures(1, &textureimage);
    }

    return true;
}


//void FrameBuffer::writePNG(const std::string &filename)
//{
//    // not ready
//    if (!framebufferid_)
//        return;

//    // create a temporary RGBA frame buffer at the resolution of cropped area
//    int w = attrib_.viewport.x * projection_area_.x;
//    int h = attrib_.viewport.y * projection_area_.y;
//    FrameBuffer copy(w,  h,  FrameBuffer_alpha);

//    // create temporary RAM buffer to store the cropped RGBA
//    uint8_t *buffer = new uint8_t[w * h * 4];

//    // blit the frame buffer into the copy
//    blit(&copy);

//    // get pixels of the copy into buffer
//    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0); // set buffer target readpixel
//    copy.readPixels(buffer);

//    // save to file
//    stbi_write_png(filename.c_str(), w, h, 4, buffer, w * 4);

//    // delete (copy is also deleted)
//    delete[] buffer;
//}
