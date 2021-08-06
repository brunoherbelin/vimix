#include <algorithm>

//  Desktop OpenGL function loader
#include <glad/glad.h>

// gstreamer
#include <gst/gstformat.h>
#include <gst/video/video.h>

#include "defines.h"
#include "Log.h"
#include "GstToolkit.h"
#include "BaseToolkit.h"
#include "FrameBuffer.h"

#include "FrameGrabber.h"



FrameGrabbing::FrameGrabbing(): pbo_index_(0), pbo_next_index_(0), size_(0), width_(0), height_(0), use_alpha_(0), caps_(NULL)
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

void FrameGrabbing::verify(FrameGrabber **rec)
{
    if ( std::find(grabbers_.begin(), grabbers_.end(), *rec) == grabbers_.end() )
        *rec = nullptr;
}

FrameGrabber *FrameGrabbing::front()
{
    if (grabbers_.empty())
        return nullptr;

    return grabbers_.front();
}

struct fgId: public std::unary_function<FrameGrabber*, bool>
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
        iter = grabbers_.erase(iter);
        delete rec;
    }
}


void FrameGrabbing::grabFrame(FrameBuffer *frame_buffer, float dt)
{
    if (frame_buffer == nullptr)
        return;

    // if different frame buffer from previous frame
    if ( frame_buffer->width() != width_ ||
         frame_buffer->height() != height_ ||
         frame_buffer->use_alpha() != use_alpha_) {

        // define stream properties
        width_ = frame_buffer->width();
        height_ = frame_buffer->height();
        use_alpha_ = frame_buffer->use_alpha();
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
                                     "framerate", GST_TYPE_FRACTION, 30, 1,
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
        if (buffer != nullptr) {

            // give the frame to all recorders
            std::list<FrameGrabber *>::iterator iter = grabbers_.begin();
            while (iter != grabbers_.end())
            {
                FrameGrabber *rec = *iter;
                rec->addFrame(buffer, caps_, dt);

                if (rec->finished()) {
                    iter = grabbers_.erase(iter);
                    delete rec;
                }
                else
                    ++iter;
            }

            // unref / free the frame
            gst_buffer_unref(buffer);
        }

    }

}



FrameGrabber::FrameGrabber(): finished_(false), expecting_finished_(false), active_(false), accept_buffer_(false),
    pipeline_(nullptr), src_(nullptr), caps_(nullptr), timer_(nullptr), timestamp_(0)
{
    // unique id
    id_ = BaseToolkit::uniqueId();
    // configure fix parameter
    frame_duration_ = gst_util_uint64_scale_int (1, GST_SECOND, 30);  // 30 FPS
}

FrameGrabber::~FrameGrabber()
{
    if (src_ != nullptr)
        gst_object_unref (src_);
    if (caps_ != nullptr)
        gst_caps_unref (caps_);
    if (pipeline_ != nullptr) {
        gst_element_set_state (pipeline_, GST_STATE_NULL);
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

uint64_t FrameGrabber::duration() const
{
    return GST_TIME_AS_MSECONDS(timestamp_);
}

void FrameGrabber::stop ()
{
    // send end of stream
    expecting_finished_ = true;
    gst_app_src_end_of_stream (src_);

    // stop recording
    active_ = false;
}

std::string FrameGrabber::info() const
{
    if (active_)
        return GstToolkit::time_to_string(timestamp_);
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
    if (grabber)
        grabber->accept_buffer_ = false;
}

GstPadProbeReturn FrameGrabber::callback_event_probe(GstPad *, GstPadProbeInfo * info, gpointer user_data)
{
  GstEvent *event = GST_PAD_PROBE_INFO_EVENT(info);
  if (GST_EVENT_TYPE (event) == GST_EVENT_EOS)
  {
      FrameGrabber *g = static_cast<FrameGrabber *>(user_data);
      if (g)
          g->finished_ = true;
  }

  return GST_PAD_PROBE_OK;
}

void FrameGrabber::addFrame (GstBuffer *buffer, GstCaps *caps, float dt)
{
    // ignore
    if (buffer == nullptr)
        return;

    // first time initialization
    if (pipeline_ == nullptr) {
        // type specific initialisation
        init(caps);
        // generic EOS detector
        GstPad *pad = gst_element_get_static_pad (gst_bin_get_by_name (GST_BIN (pipeline_), "sink"), "sink");
        gst_pad_add_probe (pad, GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM, FrameGrabber::callback_event_probe, this, NULL);
        gst_object_unref (pad);
    }

    // terminate properly if finished
    if (finished_) {
        terminate();
    }

    // stop if an incompatilble frame buffer given
    else if ( !gst_caps_is_equal( caps_, caps ))
    {
        stop();
        Log::Warning("FrameGrabber interrupted because the resolution changed.");
    }

    // store a frame if recording is active
    else if (active_)
    {
        // initialize timer
        if (timer_ == nullptr) {
            timer_ = gst_pipeline_get_clock ( GST_PIPELINE(pipeline_) );
            timer_firstframe_ = gst_clock_get_time(timer_);
        }

        // time since begin
        GstClockTime t = gst_clock_get_time(timer_) - timer_firstframe_;
        t = (t / frame_duration_) * frame_duration_;

        // if delta time is passed one frame duration (with a margin)
        // and if the encoder accepts data
        if ( (t - timestamp_) > (frame_duration_ - 3000) && accept_buffer_) {

            // set timing of buffer
            buffer->pts = t;
            buffer->duration = t - timestamp_;

            // increment ref counter to make sure the frame remains available
            gst_buffer_ref(buffer);

            // push frame
            gst_app_src_push_buffer (src_, buffer);
            // NB: buffer will be unrefed by the appsrc

            // keep timestamp
            timestamp_ = t;
        }
    }

}
