#ifndef FRAMEGRABBER_H
#define FRAMEGRABBER_H

#include <atomic>
#include <list>
#include <map>
#include <string>

#include <gst/gst.h>
#include <gst/app/gstappsrc.h>


// use glReadPixel or glGetTextImage
// read pixels & pbo should be the fastest
// https://stackoverflow.com/questions/38140527/glreadpixels-vs-glgetteximage
#define USE_GLREADPIXEL
#define DEFAULT_GRABBER_FPS 30
#define MIN_BUFFER_SIZE 33177600  // 33177600 bytes = 1 frames 4K, 9 frames 720p

class FrameBuffer;


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

    virtual void stop();
    virtual std::string info() const;
    virtual uint64_t duration() const;
    virtual bool finished() const;
    virtual bool busy() const;

protected:

    // only FrameGrabbing manager can add frame
    virtual void addFrame(GstBuffer *buffer, GstCaps *caps);

    // only addFrame method shall call those
    virtual void init(GstCaps *caps) = 0;
    virtual void terminate() = 0;

    // thread-safe testing termination
    std::atomic<bool> finished_;
    std::atomic<bool> active_;
    std::atomic<bool> endofstream_;
    std::atomic<bool> accept_buffer_;
    std::atomic<bool> buffering_full_;

    // gstreamer pipeline
    GstElement   *pipeline_;
    GstAppSrc    *src_;
    GstCaps      *caps_;

    GstClock     *timer_;
    GstClockTime timestamp_;
    GstClockTime duration_;
    GstClockTime frame_duration_;
    guint64      frame_count_;
    guint64      buffering_size_;
    bool         timestamp_on_clock_;

    GstClockTime timer_firstframe_;

    // gstreamer callbacks
    static void callback_need_data (GstAppSrc *, guint, gpointer user_data);
    static void callback_enough_data (GstAppSrc *, gpointer user_data);    
    static GstPadProbeReturn callback_event_probe(GstPad *, GstPadProbeInfo *info, gpointer user_data);
};

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
    FrameGrabber *front();
    FrameGrabber *get(uint64_t id);
    void stopAll();
    void clearAll();

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
};



#endif // FRAMEGRABBER_H
