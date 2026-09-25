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
#include <cmath>
#include <list>

#include "FrameBuffer.h"
#include "Resource.h"
#include "Settings.h"
#include "Log.h"

#include <glm/gtc/matrix_transform.hpp>

#include <glad/glad.h>
#include <stb_image.h>
#include <stb_image_write.h>

unsigned long FrameBuffer::total_mem_usage = 0;
unsigned long FrameBuffer::memory_usage()
{
    return total_mem_usage;
}

#ifdef FRAMEBUFFER_DEBUG
// list of all frame buffers alive; function-local static to be immune
// to static initialization order (frame buffers can be created early)
static std::list<FrameBuffer *>& registry()
{
    static std::list<FrameBuffer *> list_;
    return list_;
}
#endif

//
// GPU limits & memory estimation
//

glm::vec3 FrameBuffer::maxResolution()
{
    // GL_MAX_TEXTURE_SIZE cannot change during the life of the GL context: query it once
    static GLint max_texture_size = 0;
    if (max_texture_size < 1) {
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture_size);
        // no valid GL context yet: assume the conservative minimum guaranteed by GL 3.3
        if (max_texture_size < 1)
            max_texture_size = 4096;
    }
    return glm::vec3(max_texture_size, max_texture_size, 0.f);
}

unsigned long FrameBuffer::memoryBudget()
{
    static unsigned long budget = 0;
    if (budget == 0) {
        // total RAM of the graphics card, in kBytes (or INT_MAX if unknown)
        const glm::ivec2 RAM = Rendering::getGPUMemoryInformation();
        if (RAM.y > 0 && RAM.y < INT_MAX)
            // a single frame buffer should not take more than a third of the GPU RAM
            budget = ( static_cast<unsigned long>(RAM.y) * 1024UL ) / 3UL;
        else
            // no information available: apply a fixed limit
            budget = FRAMEBUFFER_MAX_MEMORY;
    }
    return budget;
}

unsigned long FrameBuffer::memoryUsage(glm::vec3 resolution, FrameBufferFlags flags)
{
    // reject NaN, infinity and degenerate resolutions
    // (e.g. a division by a null scale when reading a session file)
    if ( !std::isfinite(resolution.x) || !std::isfinite(resolution.y) ||
         resolution.x < float(FRAMEBUFFER_MIN_SIZE) || resolution.y < float(FRAMEBUFFER_MIN_SIZE) ||
         resolution.x > float(INT_MAX) || resolution.y > float(INT_MAX) )
        return 0;

    // NB: all computed in unsigned long to avoid integer overflow on large resolutions
    const unsigned long w = static_cast<unsigned long>(resolution.x);
    const unsigned long h = static_cast<unsigned long>(resolution.y);
    const unsigned long bpp = (flags & FrameBuffer_alpha) ? 4UL : 3UL;

    // the RGB(A) texture
    unsigned long bytes = w * h * bpp;

    // the mipmap levels add about a third
    if (flags & FrameBuffer_mipmap) {
        unsigned long mw = w, mh = h;
        for (int i = 1; i < MIPMAP_LEVEL; ++i) {
            mw = MAX(1UL, mw / 2UL);
            mh = MAX(1UL, mh / 2UL);
            bytes += mw * mh * bpp;
        }
    }

    // the multisample texture holds N samples per pixel
    // (same condition as in init(), which drops the flag if multisampling is disabled)
    if ( (flags & FrameBuffer_multisampling) && Settings::application.render.multisampling > 0 )
        bytes += static_cast<unsigned long>(Settings::application.render.multisampling) * w * h * bpp;

    return bytes;
}

bool FrameBuffer::isValidResolution(glm::vec3 resolution, FrameBufferFlags flags)
{
    // not finite, or smaller than one pixel
    const unsigned long bytes = memoryUsage(resolution, flags);
    if (bytes == 0)
        return false;

    // larger than what the GPU can store in a texture
    const glm::vec3 maxres = maxResolution();
    if (resolution.x > maxres.x || resolution.y > maxres.y)
        return false;

    // more than a single frame buffer is allowed to take
    return bytes <= memoryBudget();
}

FrameBuffer::FrameBuffer(glm::vec3 resolution, FrameBufferFlags flags): flags_(flags),
    textureid_(0), multisampling_textureid_(0), framebufferid_(0), multisampling_framebufferid_(0),
    failed_(false), mem_usage_(0)
{
    attrib_.viewport = glm::ivec2(resolution);
    setProjectionArea(glm::vec4(-1.f, 1.f, 1.f, -1.f));
    attrib_.clear_color = glm::vec4(0.f, 0.f, 0.f, 0.f);
#ifdef FRAMEBUFFER_DEBUG
    registry().push_back(this);
#endif
}

FrameBuffer::FrameBuffer(uint width, uint height, FrameBufferFlags flags): flags_(flags),
    textureid_(0), multisampling_textureid_(0), framebufferid_(0), multisampling_framebufferid_(0),
    failed_(false), mem_usage_(0)
{
    attrib_.viewport = glm::ivec2(width, height);
    setProjectionArea(glm::vec4(-1.f, 1.f, 1.f, -1.f));
    attrib_.clear_color = glm::vec4(0.f, 0.f, 0.f, 0.f);
#ifdef FRAMEBUFFER_DEBUG
    registry().push_back(this);
#endif
}

void FrameBuffer::init()
{
    // a previous attempt already failed: do not try (nor warn) again
    if (failed_)
        return;

    // refuse a resolution the GPU cannot store, or that would take an
    // unreasonable amount of memory (most likely given by mistake)
    if ( !isValidResolution(resolution(), flags_) ) {
        failed_ = true;
        const glm::vec3 maxres = maxResolution();
        Log::Warning("Frame buffer %d x %d not created: it would need %lu MB of GPU memory.\n\n"
                     "The maximum is %d x %d pixels and %lu MB per frame buffer.",
                     attrib_.viewport.x, attrib_.viewport.y,
                     memoryUsage(resolution(), flags_) / 1000000,
                     (int) maxres.x, (int) maxres.y, memoryBudget() / 1000000);
        return;
    }

    // memory needed for this frame buffer (in Bytes)
    const unsigned long usage = memoryUsage(resolution(), flags_);

    // test currently available memory if the buffer is big (more than 20 MByte)
    if ( usage > 20000000UL ) {

        // Obtain RAM usage in GPU (if possible); values in kByte, INT_MAX if unknown
        const glm::ivec2 RAM = Rendering::getGPUMemoryInformation();

        // bad case: not enough RAM available (asking for twice the needed space)
        if ( RAM.x < INT_MAX && static_cast<unsigned long>(RAM.x) * 1024UL < usage * 2UL ) {
            failed_ = true;
            Log::Warning("Frame buffer %d x %d not created: only %d MB of RAM left in the "
                         "graphics card to allocate %lu MB.",
                         attrib_.viewport.x, attrib_.viewport.y, RAM.x / 1000, usage / 1000000);
            if (RAM.y < INT_MAX)
                Log::Warning("Only %.1f %% of %d MB GPU RAM available.",
                             100.f * float(RAM.x) / float(RAM.y), RAM.y / 1000);
            return;
        }
    }

    // forget any error that occured before, to be able to detect ours
    while (glGetError() != GL_NO_ERROR) { /* drain */ }

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


    // common texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_2D, 0);

    // did the driver accept to allocate the texture?
    // NB: glCheckFramebufferStatus does NOT report GL_OUT_OF_MEMORY
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        failed_ = true;
        Log::Warning("Frame buffer %d x %d not created: the graphics card refused to "
                     "allocate the texture (OpenGL error 0x%x).",
                     attrib_.viewport.x, attrib_.viewport.y, err);
        reset();
        return;
    }

    // create a framebuffer object
    glGenFramebuffers(1, &framebufferid_);
    glBindFramebuffer(GL_FRAMEBUFFER, framebufferid_);

#ifdef FRAMEBUFFER_DEBUG
        g_printerr("Framebuffer %d created (%d x %d) - ", framebufferid_, attrib_.viewport.x, attrib_.viewport.y);
#endif

    // no multisampling if application multisampling is level 0 (tested at init)
    if ( Settings::application.render.multisampling < 1 )
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

        // did the driver accept to allocate the multisample texture?
        err = glGetError();
        if (err != GL_NO_ERROR) {
            failed_ = true;
            Log::Warning("Frame buffer %d x %d not created: the graphics card refused to "
                         "allocate the multisampling texture (OpenGL error 0x%x).",
                         attrib_.viewport.x, attrib_.viewport.y, err);
            reset();
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            return;
        }

        // attach the multisampled texture to FBO (framebufferid_  currently binded)
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, multisampling_textureid_, 0);

        // create an intermediate FBO : this is the FBO to use for reading
        glGenFramebuffers(1, &multisampling_framebufferid_);
        glBindFramebuffer(GL_FRAMEBUFFER, multisampling_framebufferid_);

#ifdef FRAMEBUFFER_DEBUG
        g_printerr("multi sampling (%d) - ", Settings::application.render.multisampling);
#endif
    }

    // attach the 2D texture to latest binded FBO
    // (i.e. the multisampling_framebufferid_ FBO if enabled, default framebufferid_ otherwise)
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureid_, 0);

#ifdef FRAMEBUFFER_DEBUG
    if (flags_ & FrameBuffer_mipmap)
        g_printerr("mipmap (%d) - ", MIPMAP_LEVEL);
#endif

    if (  !checkFramebufferStatus() ) {
        failed_ = true;
        reset();
    }
    else {
        // success: account for the memory now used by this frame buffer
        // NB: mem_usage_ is non null if and only if it is counted in total_mem_usage
        mem_usage_ = usage;
        total_mem_usage += mem_usage_;
#ifdef FRAMEBUFFER_DEBUG
         g_printerr("~%lu kB allocated \t(%lu kB total)\n", mem_usage_ / 1000, total_mem_usage / 1000);
#endif
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

FrameBuffer::~FrameBuffer()
{
#ifdef FRAMEBUFFER_DEBUG
    if (framebufferid_)
         g_printerr("Framebuffer %d deleted - ~%lu kB freed (%lu kB total)\n", framebufferid_, mem_usage_ / 1000, (total_mem_usage - mem_usage_) / 1000);
    registry().remove(this);
#endif

    // NB: reset() discounts mem_usage_ from total_mem_usage
    reset();
}

void FrameBuffer::reset()
{
    if (framebufferid_)
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

    // this is the only place where the total memory usage is decreased:
    // mem_usage_ is null unless a successful init() added it to the total
    total_mem_usage -= mem_usage_;
    mem_usage_ = 0;
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
    const glm::ivec2 newviewport = glm::ivec2(res);

    // nothing to do if the resolution is unchanged
    if (attrib_.viewport == newviewport)
        return;

    // de-init (this frees the GL objects and discounts the memory usage)
    reset();

    // change resolution
    attrib_.viewport = newviewport;

    // a new resolution deserves a new attempt at allocation
    failed_ = false;
}

bool FrameBuffer::begin(bool clear)
{
    if (!framebufferid_)
        init();

    // allocation failed: do NOT bind framebuffer 0, this is the screen!
    if (!framebufferid_)
        return false;

    glBindFramebuffer(GL_FRAMEBUFFER, framebufferid_);

    Rendering::manager().pushAttrib(attrib_);

    if (clear)
        glClear(GL_COLOR_BUFFER_BIT);

    return true;
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

    // destination could not be allocated
    if (!destination->framebufferid_)
        return false;

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

bool FrameBuffer::checkFramebufferStatus()
{
    bool ret = false;
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
        // success
        // NB: available GPU memory is tested in init(), before allocation
        ret = true;
        break;
    default:
        Log::Warning(" GL_FRAMEBUFFER is in an UNKNOWN state.");
        break;
    }

    return ret;
}


#ifdef FRAMEBUFFER_DEBUG
void FrameBuffer::dumpRegistry()
{
    g_printerr("\nFramebuffer: %lu buffer(s) still alive, %lu kB not freed\n",
               (unsigned long) registry().size(), total_mem_usage / 1000);

    for (auto fb = registry().cbegin(); fb != registry().cend(); ++fb) {
        g_printerr("   %p id %d \t%d x %d \t%s%s%s \t~%lu kB\n", (void *) *fb,
                   (*fb)->framebufferid_, (*fb)->attrib_.viewport.x, (*fb)->attrib_.viewport.y,
                   ((*fb)->flags_ & FrameBuffer_alpha) ? "RGBA" : "RGB ",
                   ((*fb)->flags_ & FrameBuffer_multisampling) ? " multisampling" : "",
                   ((*fb)->flags_ & FrameBuffer_mipmap) ? " mipmap" : "",
                   (*fb)->mem_usage_ / 1000);
    }
}
#endif

glm::mat4 FrameBuffer::projection() const
{
    return projection_;
}

glm::vec4 FrameBuffer::projectionArea() const
{
    return projection_area_;
}

glm::vec2 FrameBuffer::projectionSize() const
{
    glm::vec2 size;
    size.x = (projection_area_[1] - projection_area_[0]) * 0.5f;
    size.y = (projection_area_[2] - projection_area_[3]) * 0.5f;
    return size;
}

void FrameBuffer::setProjectionArea(glm::vec4 c)
{
    projection_area_ = glm::clamp(c, glm::vec4(-1.f, -1.f, -1.f, -1.f), glm::vec4(1.f, 1.f, 1.f, 1.f));
    projection_ = glm::ortho(projection_area_[0],
                             projection_area_[1],
                             projection_area_[2],
                             projection_area_[3],
                             -1.f,
                             1.f);
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

    // could not be allocated
    if (!framebufferid_)
        return false;

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
