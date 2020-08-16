#ifndef __GST_MEDIA_PLAYER_H_
#define __GST_MEDIA_PLAYER_H_

#include <string>
#include <atomic>
#include <mutex>
#include <future>

// GStreamer
#include <gst/pbutils/gstdiscoverer.h>
#include <gst/pbutils/pbutils.h>
#include <gst/app/gstappsink.h>

#include "Timeline.h"

// Forward declare classes referenced
class Visitor;

#define MAX_PLAY_SPEED 20.0
#define MIN_PLAY_SPEED 0.1
#define N_VFRAME 3

struct MediaInfo {

    Timeline timeline;
    guint width;
    guint par_width;  // width to match pixel aspect ratio
    guint height;
    guint bitrate;
    gdouble framerate;
    std::string codec_name;
    bool isimage;
    bool interlaced;
    bool seekable;
    bool valid;

    MediaInfo() {
        width = par_width = 640;
        height = 480;
        bitrate = 0;
        framerate = 0.0;
        codec_name = "unknown";
        isimage = false;
        interlaced = false;
        seekable = false;
        valid = false;
    }

    inline MediaInfo& operator = (const MediaInfo& b)
    {
        if (this != &b) {
            this->timeline = b.timeline;
            this->width = b.width;
            this->par_width = b.par_width;
            this->height = b.height;
            this->bitrate = b.bitrate;
            this->framerate = b.framerate;
            this->codec_name = b.codec_name;
            this->valid = b.valid;
            this->isimage = b.isimage;
            this->interlaced = b.interlaced;
            this->seekable = b.seekable;
        }
        return *this;
    }
};

class MediaPlayer {

public:

    /**
     * Constructor of a GStreamer Media Player
     */
    MediaPlayer( std::string name = std::string() );
    /**
     * Destructor.
     */
    ~MediaPlayer();
    /** 
     * Open a media using gstreamer URI 
     * */
    void open( std::string path);
    /**
     * Get name of the media
     * */
    std::string uri() const;
    /**
     * Get name of the file
     * */
    std::string filename() const;
    /**
     * Get name of Codec of the media
     * */
    std::string codec() const;
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
    void update();
    /**
     * Enable / Disable
     * Suspend playing activity
     * (restores playing state when re-enabled)
     * */
    void enable(bool on);
    /**
     * True if enabled
     * */
    bool isEnabled() const;
    /**
     * True if its an image
     * */
    bool isImage() const;
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
     * Speed factor for playing
     * Can be negative.
     * */
    double playSpeed() const;
    /**
     * Set the speed factor for playing
     * Can be negative.
     * */
    void setPlaySpeed(double s);
    /**
     * Loop Mode: Behavior when reaching an extremity
     * */
    typedef enum {
        LOOP_NONE = 0,
        LOOP_REWIND = 1,
        LOOP_BIDIRECTIONAL = 2
    } LoopMode;
    /**
     * Get the current loop mode
     * */
    LoopMode loop() const;
    /**
     * Set the loop mode
     * */
    void setLoop(LoopMode mode);
    /**
     * Seek to next frame when paused
     * (aka next frame)
     * Can go backward if play speed is negative
     * */
    void step();
    /**
     * Jump fast when playing
     * (aka fast-forward)
     * Can go backward if play speed is negative
     * */
    void jump();
    /**
     * Seek to zero
     * */
    void rewind();
    /**
     * Seek to any position in media
     * pos in nanoseconds.
     * */
    void seek(GstClockTime pos);
    /**
     * @brief timeline contains all info on timing:
     * - start position : timeline.start()
     * - end position   : timeline.end()
     * - duration       : timeline.duration()
     * - frame duration : timeline.step()
     */
    Timeline timeline();
    /**
     * Get position time
     * */
    GstClockTime position();
    /**
     * Get framerate of the media
     * */
    double frameRate() const;
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
    /**
     * @brief registered
     * @return list of media players currently registered
     */
    static std::list<MediaPlayer*> registered() { return registered_; }
    static std::list<MediaPlayer*>::const_iterator begin() { return registered_.cbegin(); }
    static std::list<MediaPlayer*>::const_iterator end()   { return registered_.cend(); }

private:

    // video player description
    std::string id_;
    std::string filename_;
    std::string uri_;
    guint textureindex_;

    // general properties of media
    MediaInfo media_;
    std::future<MediaInfo> discoverer_;

    // GST & Play status
    GstClockTime position_;
    gdouble rate_;
    LoopMode loop_;
    GstState desired_state_;
    GstElement *pipeline_;
    GstVideoInfo v_frame_video_info_;
    std::atomic<bool> ready_;
    std::atomic<bool> failed_;
    bool seeking_;
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
    Frame frame_[N_VFRAME];
    guint write_index_;
    guint last_index_;
    std::mutex index_lock_;

    // for PBO
    guint pbo_[2];
    guint pbo_index_, pbo_next_index_;
    guint pbo_size_;

    // gst pipeline control
    void execute_open();
    void execute_loop_command();
    void execute_seek_command(GstClockTime target = GST_CLOCK_TIME_NONE);

    // gst frame filling
    void init_texture(guint index);
    void fill_texture(guint index);
    bool fill_frame(GstBuffer *buf, FrameStatus status);

    // gst callbacks
    static void callback_end_of_stream (GstAppSink *, gpointer);
    static GstFlowReturn callback_new_preroll (GstAppSink *, gpointer );
    static GstFlowReturn callback_new_sample  (GstAppSink *, gpointer);

    // global list of registered media player
    static std::list<MediaPlayer*> registered_;
};



#endif // __GST_MEDIA_PLAYER_H_
