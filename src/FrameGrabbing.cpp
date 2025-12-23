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

#include <cstdint>
#include <glib.h>
#include <thread>
#include <algorithm>

//  Desktop OpenGL function loader
#include <glad/glad.h>

// gstreamer
#include <gst/gstformat.h>
#include <gst/video/video.h>

#include "FrameBuffer.h"
#include "FrameGrabbing.h"


FrameGrabbing::FrameGrabbing(): pbo_index_(0), pbo_next_index_(0), size_(0),
    width_(0), height_(0), use_alpha_(0), caps_(NULL)
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

void FrameGrabbing::add(FrameGrabber *rec, uint64_t duration)
{
    if (rec != nullptr) {
        grabbers_.push_back(rec);
        grabbers_duration_[rec] = duration;
    }
}

void FrameGrabbing::chain(FrameGrabber *rec, FrameGrabber *next_rec)
{
    if (rec != nullptr && next_rec != nullptr)
    {
        // add to grabbers
        grabbers_.push_back(next_rec);
        // chain them
        grabbers_chain_[next_rec] = rec;
    }
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
    grabbers_duration_.clear();
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
    grabbers_chain_.clear();
    grabbers_duration_.clear();
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

    if (grabbers_.empty() || size_ <= 0)
        return;

    // separate CPU and GPU grabbers
    std::list<FrameGrabber *> cpu_grabbers_;
    std::list<FrameGrabber *> gpu_grabbers_;
    std::partition_copy(
        grabbers_.begin(), grabbers_.end(),
        std::back_inserter(gpu_grabbers_),
        std::back_inserter(cpu_grabbers_),
        fgType(FrameGrabber::GRABBER_GPU)
    );

    // feed CPU grabbers with frame_buffer texture index
    if (!cpu_grabbers_.empty()) {

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
            std::list<FrameGrabber *>::iterator iter = cpu_grabbers_.begin();
            while (iter != cpu_grabbers_.end())
            {
                FrameGrabber *rec = *iter;

                uint64_t max_duration = grabbers_duration_.count(rec) ? grabbers_duration_[rec] : 0;
                if (max_duration > 0 && rec->duration() >= max_duration - rec->frameDuration() * 2) 
                    rec->stop();
                
                rec->addFrame(buffer, caps_);

                // remove finished recorders
                if (rec->finished()) {
                    // terminate and remove from main grabbers list
                    rec->terminate();
                    grabbers_.remove(rec);
                    grabbers_duration_.erase(rec);
                    // remove from local list and iterate
                    iter = cpu_grabbers_.erase(iter);
                    delete rec;
                }
                else
                    ++iter;
            }

            // unref / free the frame
            gst_buffer_unref(buffer);
        }
    }

    // feed GPU grabbers with frame_buffer texture index
    if (!gpu_grabbers_.empty()) {

        // give the texture to all recorders
        std::list<FrameGrabber *>::iterator iter = gpu_grabbers_.begin();
        while (iter != gpu_grabbers_.end())
        {
            FrameGrabber *rec = *iter;

            uint64_t max_duration = grabbers_duration_.count(rec) ? grabbers_duration_[rec] : 0;
            if (max_duration > 0 && rec->duration() >= max_duration - rec->frameDuration()) 
                rec->stop();
                
            rec->addFrame(frame_buffer->texture(), caps_);

            // remove finished recorders
            if (rec->finished()) {
                // terminate and remove from main grabbers list
                rec->terminate();
                grabbers_.remove(rec);
                grabbers_duration_.erase(rec);
                // remove from local list and iterate
                iter = gpu_grabbers_.erase(iter);
                delete rec;
            }
            else
                ++iter;
        }

    }

    // manage the list of chainned recorder
    std::map<FrameGrabber *, FrameGrabber *>::iterator chain = grabbers_chain_.begin();
    while (chain != grabbers_chain_.end())
    {
        // if the chained recorder is now active
        if (chain->first->active_ && chain->first->accept_buffer_){

            // stop the replaced grabber
            chain->second->stop();

            // manage duration : switch remaining time to new grabber
            uint64_t max_duration = grabbers_duration_.count(chain->second) ? grabbers_duration_[chain->second] : 0;
            grabbers_duration_[chain->first] = max_duration ? max_duration - chain->second->duration() + chain->second->frameDuration() * 2 : 0;
            grabbers_duration_.erase(chain->second);

            // loop in chain list: done with this chain
            chain = grabbers_chain_.erase(chain);
        }
        else
            // loop in chain list
            ++chain;
    }

}


void Outputs::start(FrameGrabber *ptr, 
                    std::chrono::seconds delay,
                    uint64_t timeout)
{
    // delayed start
    if (delay > std::chrono::seconds(0)) {
        // start after delay in a separate thread
        std::thread([ptr, delay, timeout]() {
            Outputs::manager().delayed[ptr->type()] = true;
            std::this_thread::sleep_for(delay);
            // interrupted during delayed start
            if (Outputs::manager().delayed[ptr->type()] != false) {
                Outputs::manager().stop( ptr->type() );
                FrameGrabbing::manager().add(ptr, timeout);
            }
        }).detach();
        return;
    } 

    // immediate start
    stop( ptr->type() );
    FrameGrabbing::manager().add(ptr, timeout);        
}
    
void Outputs::chain(FrameGrabber *new_rec)  
{
    FrameGrabber *rec = FrameGrabbing::manager().get( new_rec->type() );
    if (rec != nullptr)
    {
        FrameGrabbing::manager().chain(rec, new_rec);
    }
}

bool Outputs::pending(FrameGrabber::Type T)
{
    return delayed[T];
}

bool Outputs::busy(FrameGrabber::Type T)
{
    FrameGrabber *ptr = FrameGrabbing::manager().get( T );
    if (ptr != nullptr)
        return ptr->busy();

    return false;
}

std::string Outputs::info(bool extended, FrameGrabber::Type T)
{
    if (delayed[T]) 
        return "Recording will start shortly...";
    
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
    // interrupt any delayed start for this type
    delayed[T] = false;
    
    FrameGrabber *ptr = FrameGrabbing::manager().get( T );
    if (ptr != nullptr)
        ptr->stop();
}

bool Outputs::paused(FrameGrabber::Type T)
{
    FrameGrabber *ptr = FrameGrabbing::manager().get( T );
    if (ptr != nullptr)
        return ptr->paused();

    return false;
}

void Outputs::pause(FrameGrabber::Type T)
{
    FrameGrabber *ptr = FrameGrabbing::manager().get( T );
    if (ptr != nullptr)
        ptr->setPaused(true);
}

void Outputs::unpause(FrameGrabber::Type T)
{
    FrameGrabber *ptr = FrameGrabbing::manager().get( T );
    if (ptr != nullptr)
        ptr->setPaused(false);
}
