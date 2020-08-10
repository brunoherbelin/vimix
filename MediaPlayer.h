#ifndef __GST_MEDIA_PLAYER_H_
#define __GST_MEDIA_PLAYER_H_

#include <string>
#include <mutex>

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
     * Get position time
     * */
    GstClockTime position();
    /**
     * @brief timeline contains all info on timing:
     * - start position : timeline.start()
     * - end position   : timeline.end()
     * - duration       : timeline.duration()
     * - frame duration : timeline.step()
     */
    Timeline timeline;
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
    guint width_;
    guint height_;
    guint par_width_;  // width to match pixel aspect ratio
    guint bitrate_;
    GstClockTime position_;

//    GstClockTime start_position_;
//    GstClockTime duration_;
//    GstClockTime frame_duration_;

    gdouble rate_;
    LoopMode loop_;
    gdouble framerate_;
    GstState desired_state_;
    GstElement *pipeline_;
    GstDiscoverer *discoverer_;
    std::stringstream discoverer_message_;
    std::string codec_name_;
    GstVideoInfo v_frame_video_info_;

    // status
    bool ready_;
    bool failed_;
    bool seekable_;
    bool isimage_;
    bool interlaced_;
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
        EMPTY = 0,
        SAMPLE = 1,
        PREROLL = 2,
        EOS = 4
    } FrameStatus;

    struct Frame {
        GstVideoFrame vframe;
        FrameStatus status;
        GstClockTime position;
        std::mutex access;

        Frame() {
            vframe.buffer = nullptr;
            status = EMPTY;
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

    static void callback_discoverer_process (GstDiscoverer *discoverer, GstDiscovererInfo *info, GError *err, MediaPlayer *m);
    static void callback_discoverer_finished(GstDiscoverer *discoverer, MediaPlayer *m);

    // global list of registered media player
    static std::list<MediaPlayer*> registered_;
};



#endif // __GST_MEDIA_PLAYER_H_
