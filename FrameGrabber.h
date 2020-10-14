#ifndef FRAMEGRABBER_H
#define FRAMEGRABBER_H

#include <atomic>
#include <string>

#include <gst/gst.h>

#include "GlmToolkit.h"

class FrameBuffer;
/**
 * @brief The FrameGrabber class defines the base class for all recorders
 * used to save images or videos from a frame buffer.
 *
 * The Mixer class calls addFrame() at each newly rendered frame for all of its recorder.
 */
class FrameGrabber
{
    uint64_t id_;

public:
    FrameGrabber(): finished_(false), pbo_index_(0), pbo_next_index_(0), size_(0)
    {
        id_ = GlmToolkit::uniqueId();
        pbo_[0] = pbo_[1] = 0;
    }
    virtual ~FrameGrabber() {}

    inline uint64_t id() const { return id_; }
    struct hasId: public std::unary_function<FrameGrabber*, bool>
    {
        inline bool operator()(const FrameGrabber* elem) const {
           return (elem && elem->id() == _id);
        }
        hasId(uint64_t id) : _id(id) { }
    private:
        uint64_t _id;
    };

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
