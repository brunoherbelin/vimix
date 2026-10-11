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

// gstreamer
#include <gst/gst.h>
#include <gst/app/gstappsrc.h>
#include <gst/video/video.h>

#include "FrameBuffer.h"
#include "Log.h"
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

bool Screenshot::save(std::string filename, GstToolkit::Image format)
{
    // is there something to save?
    if (Pbo && Pbo_size > 0 && Pbo_full) {

        // ready for next
        Pbo_full = false;

        // gstreamer encoder for this image format
        if ( GstToolkit::getImageEncodingPipeline(format).empty() ) {
            Log::Warning("Cannot save %s: encoder is not available.", filename.c_str());
            return false;
        }
        if ( !GstToolkit::supportsImageResolution(format, Width, Height) ) {
            Log::Warning("Cannot save %s: %s", filename.c_str(),
                         GstToolkit::unsupportedImageResolution(format, Width, Height).c_str());
            return false;
        }

        // video frame description of pixels
        // (gstreamer rows are aligned on 4 bytes, PBO rows are packed)
        GstVideoInfo info;
        gst_video_info_set_format(&info, bpp > 3 ? GST_VIDEO_FORMAT_RGBA : GST_VIDEO_FORMAT_RGB, Width, Height);
        const size_t row = (size_t) Width * bpp;
        const size_t stride = GST_VIDEO_INFO_PLANE_STRIDE(&info, 0);
        GstBuffer *buffer = gst_buffer_new_allocate(NULL, GST_VIDEO_INFO_SIZE(&info), NULL);

        // bind buffer
        glBindBuffer(GL_PIXEL_PACK_BUFFER, Pbo);

        // get pixels (quite fast) into a buffer owned by the saving thread
        bool done = false;
        unsigned char* ptr = (unsigned char*) glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
        if (NULL != ptr) {
            GstMapInfo map;
            if ( gst_buffer_map(buffer, &map, GST_MAP_WRITE) ) {
                // copy row by row, flipping vertically if needed
                for (int y = 0; y < Height; ++y)
                    memcpy(map.data + (VFlip ? Height - 1 - y : y) * stride, ptr + y * row, row);
                gst_buffer_unmap(buffer, &map);
                done = true;
            }
            glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
        }

        // unbind buffer
        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

        // initiate encoding and saving in thread (slow)
        if (done) {
            std::thread(storeToFile, buffer, gst_video_info_to_caps(&info), format, filename).detach();
            return true;
        }
        gst_buffer_unref(buffer);
    }

    return false;
}

// Thread to perform slow operation of encoding and saving to file
// (takes ownership of buffer and caps)
void Screenshot::storeToFile(GstBuffer *buffer, GstCaps *caps, GstToolkit::Image format, std::string filename)
{
    // create a gstreamer pipeline to encode one frame
    std::string description = "appsrc name=src ! videoconvert ! ";
    description += GstToolkit::getImageEncodingPipeline(format);
    description += "filesink name=sink";

    GError *error = NULL;
    GstElement *pipeline = gst_parse_launch(description.c_str(), &error);
    if (error != NULL) {
        Log::Warning("Cannot save %s: %s", filename.c_str(), error->message);
        g_clear_error(&error);
        if (pipeline)
            gst_object_unref(pipeline);
        gst_buffer_unref(buffer);
        gst_caps_unref(caps);
        return;
    }

    // setup file sink
    GstElement *sink = gst_bin_get_by_name(GST_BIN(pipeline), "sink");
    g_object_set(G_OBJECT(sink), "location", filename.c_str(), "sync", FALSE, NULL);
    gst_object_unref(sink);

    // setup app source with caps of the frame
    GstElement *src = gst_bin_get_by_name(GST_BIN(pipeline), "src");
    g_object_set(G_OBJECT(src), "format", GST_FORMAT_TIME, NULL);
    gst_app_src_set_caps(GST_APP_SRC(src), caps);
    gst_caps_unref(caps);

    // push the single frame, then end of stream
    GST_BUFFER_PTS(buffer) = 0;
    gst_element_set_state(pipeline, GST_STATE_PLAYING);
    gst_app_src_push_buffer(GST_APP_SRC(src), buffer);
    gst_app_src_end_of_stream(GST_APP_SRC(src));
    gst_object_unref(src);

    // wait for the end of encoding
    GstBus *bus = gst_element_get_bus(pipeline);
    GstMessage *msg = gst_bus_timed_pop_filtered(bus, 10 * GST_SECOND,
                                                 (GstMessageType) (GST_MESSAGE_EOS | GST_MESSAGE_ERROR));
    if (msg == NULL)
        Log::Warning("Cannot save %s: timeout.", filename.c_str());
    else {
        if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ERROR) {
            gst_message_parse_error(msg, &error, NULL);
            Log::Warning("Cannot save %s: %s", filename.c_str(), error->message);
            g_clear_error(&error);
        }
        gst_message_unref(msg);
    }
    gst_object_unref(bus);

    // done
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
}
