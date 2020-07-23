#ifndef RECORDER_H
#define RECORDER_H

#include <atomic>
#include <string>
#include <gst/gst.h>

class FrameBuffer;

class Recorder
{
public:
    Recorder();
    virtual ~Recorder() {}

    virtual void addFrame(const FrameBuffer *frame_buffer, float dt) = 0;

    inline bool finished() const { return finished_; }
    inline bool enabled() const { return enabled_; }

protected:
    std::atomic<bool> enabled_;
    std::atomic<bool> finished_;
};

class FrameRecorder : public Recorder
{
    std::string     filename_;

public:

    FrameRecorder();
    void addFrame(const FrameBuffer *frame_buffer, float dt);

};

#endif // RECORDER_H
