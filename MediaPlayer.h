#ifndef __GST_MEDIA_PLAYER_H_
#define __GST_MEDIA_PLAYER_H_

#include <string>
#include <atomic>
#include <sstream>
#include <set>
#include <list>
#include <map>
#include <utility>
#include <mutex>

#include <gst/gst.h>
#include <gst/gl/gl.h>
#include <gst/pbutils/pbutils.h>
#include <gst/app/gstappsink.h>


// Forward declare classes referenced
class Visitor;

#define MAX_PLAY_SPEED 20.0
#define MIN_PLAY_SPEED 0.1
#define N_VFRAME 3

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

struct MediaSegment
{
    GstClockTime begin;
    GstClockTime end;

    MediaSegment()
    {
        begin = GST_CLOCK_TIME_NONE;
        end = GST_CLOCK_TIME_NONE;
    }

    MediaSegment(GstClockTime b, GstClockTime e)
    {
        if ( b < e ) {
            begin = b;
            end = e;
        } else {
            begin = GST_CLOCK_TIME_NONE;
            end = GST_CLOCK_TIME_NONE;
        }
    }
    inline bool is_valid() const
    {
        return begin != GST_CLOCK_TIME_NONE && end != GST_CLOCK_TIME_NONE && begin < end;
    }
    inline bool operator < (const MediaSegment b) const
    {
        return (this->is_valid() && b.is_valid() && this->end < b.begin);
    }
    inline bool operator == (const MediaSegment b) const
    {
        return (this->begin == b.begin && this->end == b.end);
    }
    inline bool operator != (const MediaSegment b) const
    {
        return (this->begin != b.begin || this->end != b.end);
    }
};

struct containsTime: public std::unary_function<MediaSegment, bool>
{
    inline bool operator()(const MediaSegment s) const
    {
       return ( s.is_valid() && _t > s.begin && _t < s.end );
    }

    containsTime(GstClockTime t) : _t(t) { }

private:
    GstClockTime _t;
};


typedef std::set<MediaSegment> MediaSegmentSet;

class MediaPlayer {

public:

    /**
     * Constructor of a GStreamer Media
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
     * Update status
     * Must be called in update loop
     * */
    void update();
    void update_old();

    void enable(bool on);

    bool isEnabled() const;

    /**
     * Pause / Play
     * Can play backward if play speed is negative
     * */
    void play(bool on);
    /**
     * Get Pause / Play
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
     * True if the player will loop when at begin or end
     * */
    typedef enum {
        LOOP_NONE = 0,
        LOOP_REWIND = 1,
        LOOP_BIDIRECTIONAL = 2
    } LoopMode;
    LoopMode loop() const;
    /**
     * Set the player to loop
     * */
    void setLoop(LoopMode mode);
    /**
     * Restart from zero
     * */
    void rewind();
    /**
     * Seek to next frame when paused
     * Can go backward if play speed is negative
     * */
    void seekNextFrame();
    /**
     * Seek to any position in media
     * pos in nanoseconds.
     * */
    void seekTo(GstClockTime pos);
    /**
     * Jump by 10% of the duration
     * */
    void fastForward();
    /**
     * Get position time
     * */
    GstClockTime position();
    /**
     * Get total duration time
     * */
    GstClockTime duration();
    /**
     * Get duration of one frame
     * */
    GstClockTime frameDuration();
    /**
     * Get framerate of the media
     * */
    double frameRate() const;
    /**
     * Get name of Codec of the media
     * */
    std::string codec() const;
    /**
     * Get rendering update framerate
     * measured during play
     * */
    double updateFrameRate() const;
    /**
     * Get the OpenGL texture
     * Must be called in OpenGL context
     * */
    guint texture() const;
    /**
     * Get Image properties
     * */
    guint width() const;
    guint height() const;
    float aspectRatio() const;

    /**
     * Get name of the media
     * */
    std::string uri() const;
    std::string filename() const;

    /**
     * Accept visitors
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

    bool addPlaySegment(GstClockTime begin, GstClockTime end);
    bool addPlaySegment(MediaSegment s);
    bool removePlaySegmentAt(GstClockTime t);
    bool removeAllPlaySegmentOverlap(MediaSegment s);
    std::list< std::pair<guint64, guint64> > getPlaySegments() const;

    std::string id_;
    std::string filename_;
    std::string uri_;
    guint textureindex_;
    guint width_;
    guint height_;
    guint par_width_;  // width to match pixel aspect ratio
    guint bitrate_;
    GstClockTime position_;
    GstClockTime start_position_;
    GstClockTime duration_;
    GstClockTime frame_duration_;
    gdouble rate_;
    LoopMode loop_;
    TimeCounter timecount_;
    gdouble framerate_;
    GstState desired_state_;
    GstElement *pipeline_;
    GstDiscoverer *discoverer_;
    std::stringstream discoverer_message_;
    std::string codec_name_;
    GstVideoInfo v_frame_video_info_;

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

    MediaSegmentSet segments_;
    MediaSegmentSet::iterator current_segment_;

    bool ready_;
    bool failed_;
    bool seekable_;
    bool isimage_;
    bool interlaced_;
    bool enabled_;

    void execute_open();
    void execute_loop_command();
    void execute_seek_command(GstClockTime target = GST_CLOCK_TIME_NONE);

    // gst frame filling
    void init_texture(guint index);
    void fill_texture(guint index);
    bool fill_frame(GstBuffer *buf, FrameStatus status);
    static void callback_end_of_stream (GstAppSink *, gpointer);
    static GstFlowReturn callback_new_preroll (GstAppSink *, gpointer );
    static GstFlowReturn callback_new_sample  (GstAppSink *, gpointer);

    static void callback_discoverer_process (GstDiscoverer *discoverer, GstDiscovererInfo *info, GError *err, MediaPlayer *m);
    static void callback_discoverer_finished(GstDiscoverer *discoverer, MediaPlayer *m);

    static std::list<MediaPlayer*> registered_;
};



#endif // __GST_MEDIA_PLAYER_H_
