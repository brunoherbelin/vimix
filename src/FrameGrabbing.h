#ifndef FRAMEGRABBING_H
#define FRAMEGRABBING_H

#include <list>
#include <map>
#include <string>

#include <gst/gst.h>

#include "FrameGrabber.h"

class FrameBuffer;
class GPUVideoRecorder;

/**
 * @brief The FrameGrabbing class manages all frame grabbers
 *
 * Session calls grabFrame after each render
 *
 */
class FrameGrabbing
{
    friend class Mixer;

    // Private Constructor
    FrameGrabbing();
    FrameGrabbing(FrameGrabbing const& copy) = delete;
    FrameGrabbing& operator=(FrameGrabbing const& copy) = delete;

public:

    static FrameGrabbing& manager()
    {
        // The only instance
        static FrameGrabbing _instance;
        return _instance;
    }
    ~FrameGrabbing();

    inline uint width() const { return width_; }
    inline uint height() const { return height_; }

    void add(FrameGrabber *rec);
    void chain(FrameGrabber *rec, FrameGrabber *new_rec);
    void verify(FrameGrabber **rec);
    bool busy() const;
    FrameGrabber *get(uint64_t id);
    FrameGrabber *get(FrameGrabber::Type t);
    void stopAll();
    void clearAll();

    void add (GPUVideoRecorder *rec);

protected:

    // only for friend Session
    void grabFrame(FrameBuffer *frame_buffer);

private:
    std::list<FrameGrabber *> grabbers_;
    std::map<FrameGrabber *, FrameGrabber *> grabbers_chain_;
    guint pbo_[2];
    guint pbo_index_;
    guint pbo_next_index_;
    guint size_;
    guint width_;
    guint height_;
    bool  use_alpha_;
    GstCaps *caps_;
    GPUVideoRecorder *gpu_grabber_;
};

/**
 * @brief The Outputs class gives global access to launch and stop output FrameGrabbers
 *
 * Manages all output destinations (recordings, broadcasts, loopback, shared memory, etc.)
 * FrameGrabbers can terminate (normal behavior or failure) and the FrameGrabber
 * is deleted automatically; pointer is then invalid and cannot be accessed. To avoid this,
 * the Outputs::manager() gives access to FrameGrabbers by type (single instance).
 *
 * Singleton mechanism ensures only one active instance of each FrameGrabber::Type
 */
class Outputs
{
    // Private Constructor
    Outputs() {};
    Outputs(Outputs const& copy) = delete;
    Outputs& operator=(Outputs const& copy) = delete;

public:

    static Outputs& manager ()
    {
        // The only instance
        static Outputs _instance;
        return _instance;
    }

    void start(FrameGrabber *ptr);
    bool enabled(FrameGrabber::Type);
    bool busy(FrameGrabber::Type);
    std::string info(FrameGrabber::Type, bool extended = false);
    void stop(FrameGrabber::Type);
};


#endif // FRAMEGRABBING_H
