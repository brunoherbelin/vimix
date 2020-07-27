#ifndef RECORDER_H
#define RECORDER_H

#include <atomic>
#include <string>
#include <vector>

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
    virtual void stop() { }
    virtual std::string info() { return ""; }
    virtual double duration() { return 0.0; }

    inline bool finished() const { return finished_; }

protected:
    // thread-safe testing termination
    std::atomic<bool> finished_;

    // PBO
    guint pbo_[2];
    guint pbo_index_, pbo_next_index_;
    guint size_;
};

class PNGRecorder : public Recorder
{
    std::string     filename_;

public:

    PNGRecorder();
    void addFrame(FrameBuffer *frame_buffer, float) override;

};


class VideoRecorder : public Recorder
{
    std::string  filename_;

    // Frame buffer information
    FrameBuffer  *frame_buffer_;
    uint width_;
    uint height_;

    // operation
    std::atomic<bool> recording_;
    std::atomic<bool> accept_buffer_;

    // gstreamer pipeline
    GstElement   *pipeline_;
    GstAppSrc    *src_;
    GstClockTime timeframe_;
    GstClockTime timestamp_;
    GstClockTime frame_duration_;

    static void callback_need_data (GstAppSrc *, guint, gpointer user_data);
    static void callback_enough_data (GstAppSrc *, gpointer user_data);

public:

    typedef enum {
        H264_STANDARD = 0,
        H264_HQ,
        PRORES_STANDARD,
        PRORES_HQ,
        VP8,
        JPEG_MULTI,
        DEFAULT
    } Profile;
    static const char* profile_name[DEFAULT];
    static const std::vector<std::string> profile_description;

    VideoRecorder();
    ~VideoRecorder();

    void addFrame(FrameBuffer *frame_buffer, float dt) override;
    void stop() override;
    std::string info() override;

    double duration() override;

};


#endif // RECORDER_H
