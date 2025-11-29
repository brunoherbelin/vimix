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

#include "Log.h"
#include "Toolkit/GstToolkit.h"
#include "Toolkit/BaseToolkit.h"
#include "FrameBuffer.h"

#include "FrameGrabber.h"



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

}



FrameGrabber::FrameGrabber(): finished_(false), initialized_(false), active_(false),
    endofstream_(false), accept_buffer_(false), buffering_full_(false), pause_(false),
    pipeline_(nullptr), src_(nullptr), caps_(nullptr), timer_(nullptr), timer_firstframe_(0),
    timer_pauseframe_(0), timestamp_(0), duration_(0), pause_duration_(0), frame_count_(0),
    keyframe_count_(0), buffering_size_(MIN_BUFFER_SIZE), buffering_count_(0), timestamp_on_clock_(true)
{
    // unique id
    id_ = BaseToolkit::uniqueId();
    // configure default parameter
    frame_duration_ = gst_util_uint64_scale_int (1, GST_SECOND, DEFAULT_GRABBER_FPS);  // 25 FPS by default
}

FrameGrabber::~FrameGrabber()
{
    if (src_ != nullptr)
        gst_object_unref (src_);
    if (caps_ != nullptr)
        gst_caps_unref (caps_);
    if (timer_)
        gst_object_unref (timer_);

    if (pipeline_ != nullptr) {
        // ignore errors on ending
        gst_bus_set_flushing(gst_element_get_bus(pipeline_), true);
        // force pipeline stop
        GstState state = GST_STATE_NULL;
        gst_element_set_state (pipeline_, state);
        gst_element_get_state (pipeline_, &state, NULL, GST_CLOCK_TIME_NONE);
        gst_object_unref (pipeline_);
    }
}

bool FrameGrabber::finished() const
{
    return finished_;
}

bool FrameGrabber::busy() const
{
    if (active_)
        return accept_buffer_ ? true : false;
    else
        return false;
}

bool FrameGrabber::paused() const
{
    return pause_;
}

void FrameGrabber::setPaused(bool pause)
{
    // can pause only if already active
    if (active_) {

        // keep time of switch from not-paused to paused
        if (pause && !pause_)
            timer_pauseframe_ = gst_clock_get_time(timer_);

        // set to paused
        pause_ = pause;

        // pause pipeline
        gst_element_set_state (pipeline_, pause_ ? GST_STATE_PAUSED : GST_STATE_PLAYING);
    }
}

uint64_t FrameGrabber::duration() const
{
    return GST_TIME_AS_MSECONDS(duration_);
}

void FrameGrabber::stop ()
{
    // TODO if not initialized wait for initializer

    // stop recording
    active_ = false;

    // send end of stream
    gst_element_send_event (pipeline_, gst_event_new_eos ());

}

std::string FrameGrabber::info(bool extended) const
{
    if (extended) {
        std::string typestring;
        switch ( type() )
        {
        case GRABBER_PNG :
        typestring = "Portable Network Graphics frame grabber";
            break;
        case GRABBER_VIDEO :
        typestring = "Video file frame grabber";
            break;
        case GRABBER_P2P :
        typestring = "Peer-to-Peer stream frame grabber";
            break;
        case GRABBER_BROADCAST :
        typestring = "SRT Broarcast frame grabber";
            break;
        case GRABBER_SHM :
        typestring = "Shared Memory frame grabber";
            break;
        case GRABBER_LOOPBACK :
        typestring = "Loopback frame grabber";
            break;
        default:
        typestring = "Generic frame grabber";
            break;
        }
        return typestring;
    }

    if (!initialized_)
        return "Initializing";
    if (active_)
        return GstToolkit::time_to_string(duration_);
    else
        return "Inactive";
}

// appsrc needs data and we should start sending
void FrameGrabber::callback_need_data (GstAppSrc *, guint , gpointer p)
{
    FrameGrabber *grabber = static_cast<FrameGrabber *>(p);
    if (grabber)
        grabber->accept_buffer_ = true;
}

// appsrc has enough data and we can stop sending
void FrameGrabber::callback_enough_data (GstAppSrc *, gpointer p)
{
    FrameGrabber *grabber = static_cast<FrameGrabber *>(p);
    if (grabber) {
        grabber->accept_buffer_ = false;
#ifndef NDEBUG
        Log::Info("Frame capture : Buffer full");
#endif
    }
}


GstBusSyncReply FrameGrabber::signal_handler(GstBus *, GstMessage *msg, gpointer ptr)
{
    // only handle error messages
    if (ptr != nullptr && GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ERROR) {
        // inform user
        GError *error;
        gst_message_parse_error(msg, &error, NULL);
        FrameGrabber *fg = reinterpret_cast<FrameGrabber *>(ptr);
        if (fg) {
            Log::Warning("FrameGrabber Error %s : %s",
                        std::to_string(fg->id()).c_str(),
                        error->message);
            fg->endofstream_=true;
        }
        else 
            Log::Warning("FrameGrabber Error : %s", error->message);
        g_error_free(error);
//    } else {
//        g_printerr("FrameGrabber msg %s \n", GST_MESSAGE_TYPE_NAME(msg));
    }

    // drop all messages to avoid filling up the stack
    return GST_BUS_DROP;
}


GstPadProbeReturn FrameGrabber::callback_event_probe(GstPad *, GstPadProbeInfo * info, gpointer p)
{
  GstEvent *event = GST_PAD_PROBE_INFO_EVENT(info);
  if (GST_EVENT_TYPE (event) == GST_EVENT_EOS)
  {
      FrameGrabber *grabber = static_cast<FrameGrabber *>(p);
      if (grabber)
          grabber->endofstream_ = true;
  }

  return GST_PAD_PROBE_OK;
}


std::string FrameGrabber::initialize(FrameGrabber *rec, GstCaps *caps)
{
    return rec->init(caps);
}

void FrameGrabber::addFrame (GstBuffer *buffer, GstCaps *caps)
{
    // ignore
    if (buffer == nullptr)
        return;

    // initializer ongoing in separate thread
    if (initializer_.valid()) {
        // try to get info from initializer
        if (initializer_.wait_for( std::chrono::milliseconds(4) ) == std::future_status::ready )
        {
            // done initialization
            std::string msg = initializer_.get();

            // if initialization succeeded
            if (initialized_) {

#ifdef IGNORE_GST_BUS_MESSAGE
                //        avoid filling up bus with messages
                gst_bus_set_flushing(gst_element_get_bus(pipeline_), true);
#else
                // set message handler for the pipeline's bus
                gst_bus_set_sync_handler(gst_element_get_bus(pipeline_),
                                         FrameGrabber::signal_handler, this, NULL);
#endif
                // attach EOS detector
                GstPad *pad = gst_element_get_static_pad (gst_bin_get_by_name (GST_BIN (pipeline_), "sink"), "sink");
                gst_pad_add_probe (pad, GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM, FrameGrabber::callback_event_probe, this, NULL);
                gst_object_unref (pad);
                // start recording
                active_ = true;
                // inform
                Log::Info("%s", msg.c_str());
            }
            // else show warning
            else {
                // end frame grabber
                finished_ = true;
                // inform
                Log::Warning("%s", msg.c_str());
            }
        }
    }
    // first time initialization
    else if (pipeline_ == nullptr) {
        initializer_ = std::async( FrameGrabber::initialize, this, caps);
    }

    // stop if an incompatilble frame buffer given after initialization
    if (initialized_ && !gst_caps_is_subset( caps_, caps ))
    {
        stop();
        Log::Warning("Frame capture interrupted because the resolution changed.");
    }

    // store a frame if recording is active and if the encoder accepts data
    if (active_)
    {
        // how much buffer is used
        buffering_count_ = gst_app_src_get_current_level_bytes(src_);

        if (accept_buffer_ && !pause_) {
            GstClockTime t = 0;

            // initialize timer on first occurence
            if (timer_ == nullptr) {
                timer_ = gst_pipeline_get_clock ( GST_PIPELINE(pipeline_) );
                timer_firstframe_ = gst_clock_get_time(timer_);
            }
            else {
                // if returning from pause, time of pause was stored
                if (timer_pauseframe_ > 0) {
                    // compute duration of the pausing time and add to total pause duration
                    pause_duration_ += gst_clock_get_time(timer_) - timer_pauseframe_;

                    // sync audio packets
                    GstElement *audiosync = GST_ELEMENT_CAST(gst_bin_get_by_name(GST_BIN(pipeline_), "audiosync"));
                    if (audiosync)
                        g_object_set(G_OBJECT(audiosync), "ts-offset", -timer_pauseframe_, NULL);

                    // reset pause frame time
                    timer_pauseframe_ = 0;
                }
                // time since timer starts (first frame registered)
                // minus pause duration
                t = gst_clock_get_time(timer_) - timer_firstframe_ - pause_duration_;
            }

            // if time is zero (first frame) or if delta time is passed one frame duration (with a margin)
            if ( t == 0 || (t - duration_) > (frame_duration_ - 3000) ) {

                // add a key frame every second (if keyframecount is valid)
                if (keyframe_count_ > 1 && frame_count_ % keyframe_count_ < 1) {
                    GstEvent *event
                        = gst_video_event_new_downstream_force_key_unit(timestamp_,
                                                                        GST_CLOCK_TIME_NONE,
                                                                        GST_CLOCK_TIME_NONE,
                                                                        FALSE,
                                                                        frame_count_ / keyframe_count_);
                    gst_element_send_event(GST_ELEMENT(src_), event);
                }

                if (timestamp_on_clock_)
                    // automatic frame presentation time stamp (DURATION PRIORITY)
                    // Pipeline set to "do-timestamp"=TRUE
                    // set timestamp to actual time
                    timestamp_ = duration_;
                else {
                    // force frame presentation timestamp (FRAMERATE PRIORITY)
                    // Pipeline set to "do-timestamp"=FALSE
                    GST_BUFFER_DTS(buffer) = GST_BUFFER_PTS(buffer) = timestamp_;
                    // set frame duration
                    buffer->duration = frame_duration_;
                    // monotonic timestamp increment to keep fixed FPS
                    // Pipeline set to "do-timestamp"=FALSE
                    timestamp_ += frame_duration_;
                }

                // when buffering is (almost) full, refuse buffer 1 frame over 2
                if (buffering_full_)
                    accept_buffer_ = frame_count_%2;
                else
                {
                    // enter buffering_full_ mode if the space left in buffering is for only few frames
                    // (this prevents filling the buffer entirely)
                    if ( buffering_size_ - buffering_count_ < MIN_BUFFER_SIZE ) {
#ifndef NDEBUG
                        Log::Info("Frame capture : Using %s of %s Buffer.",
                                  BaseToolkit::byte_to_string(buffering_count_).c_str(),
                                  BaseToolkit::byte_to_string(buffering_size_).c_str());
#endif
                        buffering_full_ = true;
                    }
                }

                // increment ref counter to make sure the frame remains available
                gst_buffer_ref(buffer);

                // push frame
                gst_app_src_push_buffer (src_, buffer);
                // NB: buffer will be unrefed by the appsrc

                // count frames
                frame_count_++;

                // update duration to an exact multiples of frame duration
                duration_ = ( t / frame_duration_) * frame_duration_;
            }
        }
    }

    // if we received and end of stream (from callback_event_probe)
    if (endofstream_)
    {
        // try to stop properly when interrupted
        if (active_) {
            // de-activate and re-send EOS
            stop();
            // inform
            Log::Info("Frame capture : Unnexpected EOF signal (no space left on drive? File deleted?)");
            Log::Warning("Frame capture : Failed after %s.", GstToolkit::time_to_string(duration_, GstToolkit::TIME_STRING_READABLE).c_str());
        }
        // terminate properly if finished
        else
        {
            terminate();
            finished_ = true;
        }

    }

}


uint FrameGrabber::buffering() const
{
    guint64 p = (100 * buffering_count_) / buffering_size_;
    return (uint) p;
}

guint64 FrameGrabber::frames() const
{
    return frame_count_;
}


void Broadcast::start(FrameGrabber *ptr)
{
    stop( ptr->type() );
    FrameGrabbing::manager().add(ptr);
}

bool Broadcast::busy(FrameGrabber::Type T)
{
    FrameGrabber *ptr = FrameGrabbing::manager().get( T );
    if (ptr != nullptr)        
        return ptr->busy();
    
    return false;
}

std::string Broadcast::info(FrameGrabber::Type T, bool extended)
{
    FrameGrabber *ptr = FrameGrabbing::manager().get( T );
    if (ptr != nullptr)        
        return ptr->info(extended);
    
    return "Disabled";
}

bool Broadcast::enabled(FrameGrabber::Type T)
{
    return ( FrameGrabbing::manager().get( T ) != nullptr);
}

void Broadcast::stop(FrameGrabber::Type T)
{
    FrameGrabber *prev = FrameGrabbing::manager().get( T );
    if (prev != nullptr)        
        prev->stop();
}
