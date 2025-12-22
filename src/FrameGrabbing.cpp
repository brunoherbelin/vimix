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

#include <algorithm>

//  Desktop OpenGL function loader
#include <glad/glad.h>

// gstreamer
#include <gst/gstformat.h>
#include <gst/video/video.h>

#include "FrameBuffer.h"
#include "FrameGrabbing.h"

#include "GPUVideoRecorder.h"

#ifdef USE_GST_OPENGL_SYNC_HANDLER
void FrameGrabbing::add(GPUVideoRecorder *rec)
{
    if (gpu_grabber_ == nullptr) {
        gpu_grabber_ = rec;
    }
}

#endif  // USE_GST_OPENGL_SYNC_HANDLER

FrameGrabbing::FrameGrabbing(): pbo_index_(0), pbo_next_index_(0), size_(0),
    width_(0), height_(0), use_alpha_(0), caps_(NULL), gpu_grabber_(nullptr)
{
    pbo_[0] = 0;
    pbo_[1] = 0;
}

FrameGrabbing::~FrameGrabbing()
{
    // stop and delete all frame grabbers
    clearAll();

    // cleanup
    if (caps_)
        gst_caps_unref (caps_);

//    if (pbo_[0] > 0) // automatically deleted at shutdown
//        glDeleteBuffers(2, pbo_);
}

void FrameGrabbing::add(FrameGrabber *rec)
{
    if (rec != nullptr)
        grabbers_.push_back(rec);
}

void FrameGrabbing::chain(FrameGrabber *rec, FrameGrabber *next_rec)
{
    if (rec != nullptr && next_rec != nullptr)
    {
        // add grabber if not yet
        if ( std::find(grabbers_.begin(), grabbers_.end(), rec) == grabbers_.end() )
            grabbers_.push_back(rec);

        grabbers_chain_[next_rec] = rec;
    }
}

void FrameGrabbing::verify(FrameGrabber **rec)
{
    if ( std::find(grabbers_.begin(), grabbers_.end(), *rec) == grabbers_.end() &&
         grabbers_chain_.find(*rec) == grabbers_chain_.end()  )
        *rec = nullptr;
}

bool FrameGrabbing::busy() const
{
    return !grabbers_.empty();
}

struct fgId
{
    inline bool operator()(const FrameGrabber* elem) const {
       return (elem && elem->id() == _id);
    }
    explicit fgId(uint64_t id) : _id(id) { }
private:
    uint64_t _id;
};

FrameGrabber *FrameGrabbing::get(uint64_t id)
{
    if (id > 0 && grabbers_.size() > 0 )
    {
        std::list<FrameGrabber *>::iterator iter = std::find_if(grabbers_.begin(), grabbers_.end(), fgId(id));
        if (iter != grabbers_.end())
            return (*iter);
    }

    return nullptr;
}

struct fgType
{
    inline bool operator()(const FrameGrabber* elem) const {
       return (elem && elem->type() == _t);
    }
    explicit fgType(FrameGrabber::Type t) : _t(t) { }
private:
    FrameGrabber::Type _t;
};

FrameGrabber *FrameGrabbing::get(FrameGrabber::Type t)
{
    if (grabbers_.size() > 0 )
    {
        std::list<FrameGrabber *>::iterator iter = std::find_if(grabbers_.begin(), grabbers_.end(), fgType(t));
        if (iter != grabbers_.end())
            return (*iter);
    }

    return nullptr;
}

void FrameGrabbing::stopAll()
{
    std::list<FrameGrabber *>::iterator iter;
    for (iter=grabbers_.begin(); iter != grabbers_.end(); ++iter )
        (*iter)->stop();
}

void FrameGrabbing::clearAll()
{
    std::list<FrameGrabber *>::iterator iter;
    for (iter=grabbers_.begin(); iter != grabbers_.end(); )
    {
        FrameGrabber *rec = *iter;
        rec->stop();
        if (rec->finished()) {
            iter = grabbers_.erase(iter);
            delete rec;
        }
        else
            ++iter;
    }
}


void FrameGrabbing::grabFrame(FrameBuffer *frame_buffer)
{
    if (frame_buffer == nullptr)
        return;

    // if different frame buffer from previous frame
    if ( frame_buffer->width() != width_ ||
         frame_buffer->height() != height_ ||
         (frame_buffer->flags() & FrameBuffer::FrameBuffer_alpha) != use_alpha_) {

        // define stream properties
        width_ = frame_buffer->width();
        height_ = frame_buffer->height();
        use_alpha_ = (frame_buffer->flags() & FrameBuffer::FrameBuffer_alpha);
        size_ = width_ * height_ * (use_alpha_ ? 4 : 3);

        // first time initialization
        if ( pbo_[0] == 0 )
            glGenBuffers(2, pbo_);

        // re-affect pixel buffer object
        glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo_[1]);
        glBufferData(GL_PIXEL_PACK_BUFFER, size_, NULL, GL_STREAM_READ);
        glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo_[0]);
        glBufferData(GL_PIXEL_PACK_BUFFER, size_, NULL, GL_STREAM_READ);

        // reset indices
        pbo_index_ = 0;
        pbo_next_index_ = 0;

        // new caps
        if (caps_)
            gst_caps_unref (caps_);
        caps_ = gst_caps_new_simple ("video/x-raw",
                                     "format", G_TYPE_STRING, use_alpha_ ? "RGBA" : "RGB",
                                     "width",  G_TYPE_INT, width_,
                                     "height", G_TYPE_INT, height_,
                                     NULL);
    }

    // fill a frame in buffer
    if (!grabbers_.empty() && size_ > 0) {

        GstBuffer *buffer = nullptr;

        // set buffer target for writing in a new frame
        glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo_[pbo_index_]);

#ifdef USE_GLREADPIXEL
        // get frame
        frame_buffer->readPixels();
#else
        glBindTexture(GL_TEXTURE_2D, frame_buffer->texture());
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        glBindTexture(GL_TEXTURE_2D, 0);
#endif

        // update case ; alternating indices
        if ( pbo_next_index_ != pbo_index_ ) {

            // set buffer target for saving the frame
            glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo_[pbo_next_index_]);

            // new buffer
            buffer = gst_buffer_new_and_alloc (size_);

            // map gst buffer into a memory  WRITE target
            GstMapInfo map;
            gst_buffer_map (buffer, &map, GST_MAP_WRITE);

            // map PBO pixels into a memory READ pointer
            unsigned char* ptr = (unsigned char*) glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);

            // transfer pixels from PBO memory to buffer memory
            if (NULL != ptr)
                memmove(map.data, ptr, size_);

            // un-map
            glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
            gst_buffer_unmap (buffer, &map);
        }

        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

        // alternate indices
        pbo_next_index_ = pbo_index_;
        pbo_index_ = (pbo_index_ + 1) % 2;

        // a frame was successfully grabbed
        if ( buffer != nullptr && gst_buffer_get_size(buffer) > 0) {

            // give the frame to all recorders
            std::list<FrameGrabber *>::iterator iter = grabbers_.begin();
            while (iter != grabbers_.end())
            {
                FrameGrabber *rec = *iter;
                rec->addFrame(buffer, caps_);

                // remove finished recorders
                if (rec->finished()) {
                    iter = grabbers_.erase(iter);
                    delete rec;
                }
                else
                    ++iter;
            }

            // manage the list of chainned recorder
            std::map<FrameGrabber *, FrameGrabber *>::iterator chain = grabbers_chain_.begin();
            while (chain != grabbers_chain_.end())
            {
                // update frame grabber of chain list
                chain->first->addFrame(buffer, caps_);

                // if the chained recorder is now active
                if (chain->first->active_ && chain->first->accept_buffer_){
                    // add it to main grabbers,
                    grabbers_.push_back(chain->first);
                    // stop the replaced grabber
                    chain->second->stop();
                    // loop in chain list: done with this chain
                    chain = grabbers_chain_.erase(chain);
                }
                else
                    // loop in chain list
                    ++chain;
            }

            // unref / free the frame
            gst_buffer_unref(buffer);
        }
    }

#ifdef USE_GST_OPENGL_SYNC_HANDLER
    // TEST OF GPU FRAME GRABBER
    if (gpu_grabber_ != nullptr) {

        gpu_grabber_->addFrame(frame_buffer->texture(), caps_);
        
        if (gpu_grabber_->finished()) {
            delete gpu_grabber_;
            gpu_grabber_ = nullptr;
        }

    }
#endif  // USE_GST_OPENGL_SYNC_HANDLER

}


void Outputs::start(FrameGrabber *ptr)
{
    stop( ptr->type() );
    FrameGrabbing::manager().add(ptr);
}

bool Outputs::busy(FrameGrabber::Type T)
{
    FrameGrabber *ptr = FrameGrabbing::manager().get( T );
    if (ptr != nullptr)
        return ptr->busy();

    return false;
}

std::string Outputs::info(FrameGrabber::Type T, bool extended)
{
    FrameGrabber *ptr = FrameGrabbing::manager().get( T );
    if (ptr != nullptr)
        return ptr->info(extended);

    return "Disabled";
}

bool Outputs::enabled(FrameGrabber::Type T)
{
    return ( FrameGrabbing::manager().get( T ) != nullptr);
}

void Outputs::stop(FrameGrabber::Type T)
{
    FrameGrabber *prev = FrameGrabbing::manager().get( T );
    if (prev != nullptr)
        prev->stop();
}
