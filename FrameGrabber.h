#ifndef FRAMEGRABBER_H
#define FRAMEGRABBER_H

#include <atomic>
#include <string>

#include <gst/gst.h>

class FrameBuffer;
/**
 * @brief The FrameGrabber class defines the base class for all recorders
 * used to save images or videos from a frame buffer.
 *
 * The Mixer class calls addFrame() at each newly rendered frame for all of its recorder.
 */
class FrameGrabber
{
public:
    FrameGrabber(): finished_(false), pbo_index_(0), pbo_next_index_(0), size_(0)
    {
        pbo_[0] = pbo_[1] = 0;
    }

    virtual ~FrameGrabber() {}

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

#endif // FRAMEGRABBER_H
