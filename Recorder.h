#ifndef RECORDER_H
#define RECORDER_H

#include <atomic>
#include <string>

#include <gst/pbutils/pbutils.h>
#include <gst/app/gstappsrc.h>

class FrameBuffer;

/**
 * @brief The Recorder class defines the base class for all recorders
 * used to save images or videos from a frame buffer.
 *
 * The Mixer class calls addFrame() at each newly rendered frame for all of its recorder.
 */
class Recorder
{
public:
    Recorder();
    virtual ~Recorder() {}

    virtual void addFrame(FrameBuffer *frame_buffer, float dt) = 0;

    inline void finish() { finished_ = true; }
    inline bool finished() const { return finished_; }

    inline void setEnabled( bool on ) { enabled_ = on; }
    inline bool enabled() const { return enabled_; }

protected:
    std::atomic<bool> enabled_;
    std::atomic<bool> finished_;
};

class PNGRecorder : public Recorder
{
    std::string     filename_;

public:

    PNGRecorder();
    void addFrame(FrameBuffer *frame_buffer, float);

};


class H264Recorder : public Recorder
{
    std::string  filename_;

    // Frame buffer information
    FrameBuffer  *frame_buffer_;
    uint width_;
    uint height_;
    uint buf_size_;

    // operation
    std::atomic<bool> recording_;
    std::atomic<bool> accept_buffer_;

    // gstreamer pipeline
    GstElement   *pipeline_;
    GstAppSrc    *src_;
    GstBaseSink  *sink_;
    GstClockTime time_;
    GstClockTime timestamp_;
    GstClockTime frame_duration_;

    static void callback_need_data (GstAppSrc *src, guint length, gpointer user_data);
    static void callback_enough_data (GstAppSrc *src, gpointer user_data);

public:

    H264Recorder();
    ~H264Recorder();

    void addFrame(FrameBuffer *frame_buffer, float dt);


};


#endif // RECORDER_H
