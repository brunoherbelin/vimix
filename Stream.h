#ifndef STREAM_H
#define STREAM_H

#include <string>
#include <atomic>
#include <mutex>
#include <future>

// GStreamer
#include <gst/pbutils/pbutils.h>
#include <gst/app/gstappsink.h>

// Forward declare classes referenced
class Visitor;

#define N_FRAME 3

class Stream {

public:
    /**
     * Constructor of a GStreamer Stream
     */
    Stream();
    /**
     * Destructor.
     */
    ~Stream();
    /**
     * Get unique id
     */
    inline int id() const { return id_; }
    /**
     * Open a media using gstreamer pipeline keyword
     * */
    void open(const std::string &gstreamer_description, int w = 1024, int h = 576);
    /**
     * Get description string
     * */
    std::string description() const;
    /**
     * True if a media was oppenned
     * */
    bool isOpen() const;
    /**
     * True if problem occured
     * */
    bool failed() const;
    /**
     * Close the Media
     * */
    void close();
    /**
     * Update texture with latest frame
     * Must be called in rendering update loop
     * */
    virtual void update();
    /**
     * Enable / Disable
     * Suspend playing activity
     * (restores playing state when re-enabled)
     * */
    void enable(bool on);
    /**
     * True if enabled
     * */
    bool enabled() const;
    /**
     * True if its an image
     * */
    bool singleFrame() const;
    /**
     * True if its a live stream
     * */
    bool live() const;
    /**
     * Pause / Play
     * Can play backward if play speed is negative
     * */
    void play(bool on);
    /**
     * Get Pause / Play status
     * Performs a full check of the Gstreamer pipeline if testpipeline is true
     * */
    bool isPlaying(bool testpipeline = false) const;
    /**
     * Get rendering update framerate
     * measured during play
     * */
    double updateFrameRate() const;
    /**
     * Get frame width
     * */
    guint width() const;
    /**
     * Get frame height
     * */
    guint height() const;
    /**
     * Get frames displayt aspect ratio
     * NB: can be different than width() / height()
     * */
    float aspectRatio() const;
    /**
     * Get the OpenGL texture
     * Must be called in OpenGL context
     * */
    guint texture() const;
    /**
     * Accept visitors
     * Used for saving session file
     * */
    void accept(Visitor& v);

protected:

    // video player description
    int id_;
    std::string description_;
    guint textureindex_;

    // general properties of media
    guint width_;
    guint height_;
    bool single_frame_;
    bool live_;

    // GST & Play status
    GstState desired_state_;
    GstElement *pipeline_;
    GstVideoInfo v_frame_video_info_;
    std::atomic<bool> ready_;
    std::atomic<bool> failed_;
    bool enabled_;

    // fps counter
    struct TimeCounter {

        GstClockTime last_time;
        GstClockTime tic_time;
        int nbFrames;
        gdouble fps;
    public:
        TimeCounter();
        GstClockTime dt();
        void tic();
        void reset();
        gdouble frameRate() const;
    };
    TimeCounter timecount_;

    // frame stack
    typedef enum  {
        SAMPLE = 0,
        PREROLL = 1,
        EOS = 2,
        INVALID = 3
    } FrameStatus;

    struct Frame {
        GstVideoFrame vframe;
        FrameStatus status;
        bool full;
        GstClockTime position;
        std::mutex access;

        Frame() {
            full = false;
            status = INVALID;
            position = GST_CLOCK_TIME_NONE;
        }
    };
    Frame frame_[N_FRAME];
    guint write_index_;
    guint last_index_;
    std::mutex index_lock_;

    // for PBO
    guint pbo_[2];
    guint pbo_index_, pbo_next_index_;
    guint pbo_size_;

    // gst pipeline control
    virtual void execute_open();

    // gst frame filling
    void init_texture(guint index);
    void fill_texture(guint index);
    bool fill_frame(GstBuffer *buf, FrameStatus status);

    // gst callbacks
    static void callback_end_of_stream (GstAppSink *, gpointer);
    static GstFlowReturn callback_new_preroll (GstAppSink *, gpointer );
    static GstFlowReturn callback_new_sample  (GstAppSink *, gpointer);

};



#endif // STREAM_H
