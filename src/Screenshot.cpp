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

#include <memory.h>
#include <assert.h>
#include <thread>

#include <glad/glad.h>

// standalone image loader
#include <stb_image.h>
#include <stb_image_write.h>

#include "FrameBuffer.h"
#include "Screenshot.h"


Screenshot::Screenshot()
{
    Width = Height = 0;
    bpp = 3;
    VFlip = true;
    Pbo = 0;
    Pbo_size = 0;
    Pbo_full = false;
}

Screenshot::~Screenshot()
{
    if (Pbo > 0)
        glDeleteBuffers(1, &Pbo);
}

bool Screenshot::isFull()
{
    return Pbo_full;
}

void Screenshot::capture()
{
    // create BPO
    if (Pbo == 0)
        glGenBuffers(1, &Pbo);

    // bind
    glBindBuffer(GL_PIXEL_PACK_BUFFER, Pbo);

    // init
    unsigned int size = Width * Height * bpp;
    if (Pbo_size != size) {
        Pbo_size = size;
        glBufferData(GL_PIXEL_PACK_BUFFER, Pbo_size, NULL, GL_STREAM_READ);
    }

    // screenshot to PBO (fast)
    if (bpp > 3)
        glPixelStorei(GL_PACK_ALIGNMENT, 4);
    else
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, Width, Height, bpp > 3 ? GL_RGBA : GL_RGB, GL_UNSIGNED_BYTE, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // done
    Pbo_full = true;
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
}

void Screenshot::captureGL(int w, int h)
{
    bpp = 3; // screen capture of GL in RGB
    Width = w;
    Height = h;
    VFlip = true;

    // do capture
    capture();
}

void Screenshot::captureFramebuffer(FrameBuffer *fb)
{
    if (!fb)
        return;

    bpp = (fb->flags() & FrameBuffer::FrameBuffer_alpha) ? 4 : 3;
    Width = fb->width() * fb->projectionSize().x;
    Height = fb->height() * fb->projectionSize().y;
    VFlip = false;

    // blit the frame buffer into an RBBA copy of cropped size
    FrameBuffer copy(Width, Height, fb->flags());
    fb->blit(&copy);

    // get pixels from copy
    glBindFramebuffer(GL_READ_FRAMEBUFFER, copy.opengl_id());

    // do capture
    capture();

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Screenshot::save(std::string filename)
{
    // is there something to save?
    if (Pbo && Pbo_size > 0 && Pbo_full) {

        // bind buffer
        glBindBuffer(GL_PIXEL_PACK_BUFFER, Pbo);

        // get pixels (quite fast) into a buffer owned by the saving thread
        unsigned char* pixels = nullptr;
        unsigned char* ptr = (unsigned char*) glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
        if (NULL != ptr) {
            pixels = (unsigned char*) malloc(Pbo_size);
            memmove(pixels, ptr, Pbo_size);
            glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
        }

        // unbind buffer
        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

        // initiate saving in thread (slow)
        if (pixels)
            std::thread(storeToFile, pixels, Width, Height, bpp, VFlip, filename).detach();

        // ready for next
        Pbo_full = false;
    }

}

// Thread to perform slow operation of saving to file
// (takes ownership of pixels, which are freed after saving)
void Screenshot::storeToFile(unsigned char *pixels, int w, int h, int bpp, bool vflip, std::string filename)
{
    const size_t stride = (size_t) w * bpp;

    // flip rows in place
    // (avoid stbi_flip_vertically_on_write, which is global and not thread safe)
    if (vflip) {
        unsigned char *row = (unsigned char*) malloc(stride);
        for (int y = 0; y < h / 2; ++y) {
            unsigned char *top = pixels + y * stride;
            unsigned char *bottom = pixels + (h - 1 - y) * stride;
            memcpy(row, top, stride);
            memcpy(top, bottom, stride);
            memcpy(bottom, row, stride);
        }
        free(row);
    }

    // save file
    stbi_write_png(filename.c_str(), w, h, bpp, pixels, (int) stride);

    free(pixels);
}
