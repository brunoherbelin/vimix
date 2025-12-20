#ifndef STREAM_H
#define STREAM_H

#include <string>
#include <list>
#include <mutex>
#include <future>
#include <condition_variable>

// GStreamer
#include <gst/pbutils/pbutils.h>
#include <gst/app/gstappsink.h>

// Forward declare classes referenced
class Visitor;

#define N_FRAME 3
#define TIMEOUT 10

struct StreamInfo {

    guint width;
    guint height;
    std::condition_variable discovered;
    std::string message;

    StreamInfo(guint w=0, guint h=0) {
        width = w;
        height = h;
    }

    StreamInfo(const StreamInfo& b) {
        width = b.width;
        height = b.height;
        message = b.message;
    }

    inline bool valid() { return width > 0 && height > 0; }
};

class Stream {

    friend class StreamSource;

public:
    /**
     * Constructor of a GStreamer Stream
     */
    Stream();
    /**
     * Destructor.
     */
    virtual ~Stream();
    /**
     * Get unique id
     */
    inline uint64_t id() const { return id_; }
    /**
     * Open a media using gstreamer pipeline keyword
     * */
    void open(const std::string &gstreamer_description, guint w = 0, guint h = 0);
    /**
     * Get description string
     * */
    std::string description() const;
    /**
     * True if a media was oppenned
     * */
    bool isOpen() const;
    /**
     * True if problem occurred
     * */
    bool failed() const;
    /**
     * Close the Media
     * */
    virtual void close();
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
     * True if it has only one frame
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
     * Attempt to restart
     * */
    virtual void rewind();
    /**
     * Get position time
     * */
    virtual GstClockTime position();
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
     * Get the name of the decoder used,
     * return 'software' if no hardware decoder is used
     * NB: perform request on pipeline on first call
     * */
    std::string decoderName();
    /**
     * Get logs
     * */
    std::string log() const { return log_; }
    /**
     * Accept visitors
     * Used for saving session file
     * */
    void accept(Visitor& v);
    /**
     * @brief registered
     * @return list of streams currently registered
     */
    static std::list<GstElement*> registered() { return registered_; }

protected:

    // video player description
    uint64_t id_;
    std::string description_;
    guint textureindex_;

    // general properties of media
    guint width_;
    guint height_;
    bool single_frame_;
    bool live_;
    std::future<StreamInfo> discoverer_;

    // GST & Play status
    GstClockTime position_;
    GstState desired_state_;
    GstElement *pipeline_;
    GstBus *bus_;
    GstVideoInfo v_frame_video_info_;
    std::atomic<bool> opened_;
    std::atomic<bool> failed_;
    bool enabled_;
    std::string decoder_name_;

    // fps counter
    struct TimeCounter {
        GTimer *timer;
        gdouble fps;
    public:
        TimeCounter();
        ~TimeCounter();
        void tic();
        inline gdouble frameRate() const { return fps; }
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
        GstBuffer *buffer;
        FrameStatus status;
        bool is_new;
        GstClockTime position;
        std::mutex access;

        Frame() {
            buffer = NULL;
            is_new = false;
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
    virtual void fail(const std::string &message);
    std::string log_;

    // gst frame filling
    bool textureinitialized_;
    void init_texture(guint index);
    void fill_texture(guint index);
    bool fill_frame(GstBuffer *buf, FrameStatus status);
    std::condition_variable initialized_;
    static void timeout_initialize(Stream *str);

    // gst callbacks
    static void callback_end_of_stream (GstAppSink *, gpointer);
    static GstFlowReturn callback_new_preroll (GstAppSink *, gpointer );
    static GstFlowReturn callback_new_sample  (GstAppSink *, gpointer);

    // global list of registered streams
    static void pipeline_terminate(GstElement *p, GstBus *b);
    static std::list<GstElement*> registered_;
};



#endif // STREAM_H
