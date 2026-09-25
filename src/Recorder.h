#ifndef RECORDER_H
#define RECORDER_H

#include <string>


#include <gst/pbutils/pbutils.h>
#include <gst/app/gstappsrc.h>

#include "FrameGrabber.h"

class PNGRecorder : public FrameGrabber
{
    std::string basename_;
    std::string filename_;

public:

    PNGRecorder(const std::string &basename = std::string());
    std::string filename() const { return filename_; }

    FrameGrabber::Type type () const override { return FrameGrabber::GRABBER_PNG; }

protected:

    std::string init(GstCaps *read_caps, GstCaps *write_caps) override;
    void terminate() override;
    void addFrame(GstBuffer *buffer, GstCaps *read_caps, GstCaps *write_caps) override;

};

class VideoRecorder : public FrameGrabber
{
    std::string basename_;
    std::string filename_;

    std::string init(GstCaps *read_caps, GstCaps *write_caps) override;
    void terminate() override;

public:

    static const char*   buffering_preset_name[6];
    static const guint64 buffering_preset_value[6];
    static const char*   framerate_preset_name[3];
    static const int     framerate_preset_value[3];

    VideoRecorder(const std::string &basename = std::string());
    FrameGrabber::Type type () const override { return FrameGrabber::GRABBER_VIDEO; }

    std::string info(bool extended = false) const override;
    std::string filename() const { return filename_; }
};


#endif // RECORDER_H
