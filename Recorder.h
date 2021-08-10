#ifndef RECORDER_H
#define RECORDER_H

#include <vector>

#include <gst/pbutils/pbutils.h>
#include <gst/app/gstappsrc.h>

#include "FrameGrabber.h"

class PNGRecorder : public FrameGrabber
{
    std::string     filename_;

public:

    PNGRecorder();

protected:

    void init(GstCaps *caps) override;
    void terminate() override;
    void addFrame(GstBuffer *buffer, GstCaps *caps, float dt) override;

};

class VideoRecorder : public FrameGrabber
{
    std::string  filename_;

    void init(GstCaps *caps) override;
    void terminate() override;

public:

    typedef enum {
        H264_STANDARD = 0,
        H264_HQ,
        H265_REALTIME,
        H265_ANIMATION,
        PRORES_STANDARD,
        PRORES_HQ,
        VP8,
        JPEG_MULTI,
        DEFAULT
    } Profile;
    static const char*   profile_name[DEFAULT];
    static const std::vector<std::string> profile_description;

    static const char*   buffering_preset_name[6];
    static const guint64 buffering_preset_value[6];
    static const char*   framerate_preset_name[3];
    static const int     framerate_preset_value[3];

    VideoRecorder();
    std::string info() const override;

};


#endif // RECORDER_H
