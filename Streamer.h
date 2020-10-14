#ifndef STREAMER_H
#define STREAMER_H

#include <vector>

#include <gst/pbutils/pbutils.h>
#include <gst/app/gstappsrc.h>

#include "FrameGrabber.h"

class VideoStreamer : public FrameGrabber
{
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
        UDP_MJPEG = 0,
        DEFAULT
    } Profile;
    static const char* profile_name[DEFAULT];
    static const std::vector<std::string> profile_description;

    VideoStreamer();
    ~VideoStreamer();

    void addFrame(FrameBuffer *frame_buffer, float dt) override;
    void stop() override;
    std::string info() override;

    double duration() override;
};

#endif // STREAMER_H
