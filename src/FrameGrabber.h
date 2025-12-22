#ifndef FRAMEGRABBER_H
#define FRAMEGRABBER_H

#include <atomic>
#include <future>
#include <string>

#include <gst/gst.h>
#include <gst/app/gstappsrc.h>


// use glReadPixel or glGetTextImage
// read pixels & pbo should be the fastest
// https://stackoverflow.com/questions/38140527/glreadpixels-vs-glgetteximage
#define USE_GLREADPIXEL
#define DEFAULT_GRABBER_FPS 30
#define MIN_BUFFER_SIZE 33177600  // 33177600 bytes = 1 frames 4K, 9 frames 720p


/**
 * @brief The FrameGrabber class defines the base class for all recorders
 * used to save images or videos from a frame buffer.
 *
 * Every subclass shall at least implement init() and terminate()
 *
 * The FrameGrabbing manager calls addFrame() for all its grabbers.
 */
class FrameGrabber
{
    friend class FrameGrabbing;

    uint64_t id_;

public:
    FrameGrabber();
    virtual ~FrameGrabber();

    inline uint64_t id() const { return id_; }
    
    // types of sub-classes of FrameGrabber
    typedef enum {
        GRABBER_GENERIC = 0,
        GRABBER_PNG = 1,
        GRABBER_VIDEO = 2,
        GRABBER_P2P = 3,
        GRABBER_BROADCAST = 4,
        GRABBER_SHM = 5,
        GRABBER_LOOPBACK = 6,
        GRABBER_INVALID
    } Type;
    virtual Type type () const { return GRABBER_GENERIC; }

    virtual void stop();
    virtual std::string info(bool extended = false) const;
    virtual uint64_t duration() const;
    virtual bool finished() const;
    virtual bool busy() const;

    virtual bool paused() const;
    virtual void setPaused(bool pause);

    uint buffering() const;
    guint64 frames() const;

protected:

    // only FrameGrabbing manager can add frame
    virtual void addFrame(GstBuffer *buffer, GstCaps *caps);

    // only addFrame method shall call those
    virtual std::string init(GstCaps *caps) = 0;
    virtual void terminate() = 0;

    // thread-safe testing termination
    std::atomic<bool> finished_;
    std::atomic<bool> initialized_;
    std::atomic<bool> active_;
    std::atomic<bool> endofstream_;
    std::atomic<bool> accept_buffer_;
    std::atomic<bool> buffering_full_;
    std::atomic<bool> pause_;

    // gstreamer pipeline
    GstElement   *pipeline_;
    GstAppSrc    *src_;
    GstCaps      *caps_;

    GstClock     *timer_;
    GstClockTime timer_firstframe_;
    GstClockTime timer_pauseframe_;
    GstClockTime timestamp_;
    GstClockTime duration_;
    GstClockTime pause_duration_;
    GstClockTime frame_duration_;
    guint64      frame_count_;
    guint64      keyframe_count_;
    guint64      buffering_size_;
    guint64      buffering_count_;
    bool         timestamp_on_clock_;

    // async threaded initializer
    std::future<std::string> initializer_;
    static std::string initialize(FrameGrabber *rec, GstCaps *caps);

    // gstreamer callbacks
    static void callback_need_data (GstAppSrc *, guint, gpointer user_data);
    static void callback_enough_data (GstAppSrc *, gpointer user_data);    
    static GstPadProbeReturn callback_event_probe(GstPad *, GstPadProbeInfo *info, gpointer user_data);
    static GstBusSyncReply signal_handler(GstBus *, GstMessage *, gpointer);
};


#endif // FRAMEGRABBER_H
