#ifndef __GST_MEDIA_PLAYER_H_
#define __GST_MEDIA_PLAYER_H_

#include <string>
#include <atomic>
#include <sstream>
#include <set>
#include <list>
#include <utility>

#include <gst/gst.h>
#include <gst/gl/gl.h>
#include <gst/pbutils/pbutils.h>

// Forward declare classes referenced
class Visitor;

#define MAX_PLAY_SPEED 20.0
#define MIN_PLAY_SPEED 0.1

struct TimeCounter {

    GstClockTime current_time;
    GstClockTime last_time;
    int nbFrames;
    float fps;
public:
    TimeCounter();
    void tic();
    void reset();
    float framerate() const;
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
    void Open( std::string uri);
    /**
     * True if a media was oppenned
     * */
    bool isOpen() const;
    /**
     * Close the Media
     * */
    void Close();
    /**
     * Update status
     * Must be called in update loop
     * */
    void Update();
    /**
     * Pause / Play
     * Can play backward if play speed is negative
     * */
    void Play(bool on);
    /**
     * Get Pause / Play
     * */
    bool isPlaying() const;
    /**
     * Speed factor for playing
     * Can be negative.
     * */
    double PlaySpeed() const;
    /**
     * Set the speed factor for playing
     * Can be negative.
     * */
    void SetPlaySpeed(double s);
    /**
     * True if the player will loop when at begin or end
     * */
    typedef enum {
        LOOP_NONE = 0,
        LOOP_REWIND = 1,
        LOOP_BIDIRECTIONAL = 2
    } LoopMode;
    LoopMode Loop() const;
    /**
     * Set the player to loop
     * */
    void setLoop(LoopMode mode);
    /**
     * Restart from zero
     * */
    void Rewind();
    /**
     * Seek to next frame when paused
     * Can go backward if play speed is negative
     * */
    void SeekNextFrame();
    /**
     * Seek to any position in media
     * pos in nanoseconds.
     * */
    void SeekTo(GstClockTime pos);
    /**
     * Jump by 10% of the duration
     * */
    void FastForward();
    /**
     * Get position time
     * */
    GstClockTime Position();
    /**
     * Get total duration time
     * */
    GstClockTime Duration();
    /**
     * Get duration of one frame
     * */
    GstClockTime FrameDuration();
    /**
     * Get framerate of the media
     * */
    double FrameRate() const;    
    /**
     * Get name of Codec of the media
     * */
    std::string Codec() const;
    /**
     * Get rendering update framerate
     * measured during play
     * */
    double UpdateFrameRate() const;
    /**
     * Bind / Get the OpenGL texture 
     * Must be called in OpenGL context
     * */
    void Bind();
    guint Texture() const;
    /**
     * Get Image properties
     * */
    guint Width() const;
    guint Height() const;
    float AspectRatio() const;

    /**
     * Accept visitors
     * */
    void accept(Visitor& v);

private:

    bool addPlaySegment(GstClockTime begin, GstClockTime end);
    bool addPlaySegment(MediaSegment s);
    bool removePlaySegmentAt(GstClockTime t);
    bool removeAllPlaySegmentOverlap(MediaSegment s);
    std::list< std::pair<guint64, guint64> > getPlaySegments() const;

    std::string id;
    std::string uri;
    guint textureindex;  
    guint width;  
    guint height;  
    guint par_width;  // width to match pixel aspect ratio
    GstClockTime position;
    GstClockTime start_position;
    GstClockTime duration;
    GstClockTime frame_duration;
    gdouble rate;
    LoopMode loop;
    TimeCounter timecount;
    gdouble framerate;
    GstState desired_state;
    GstElement *pipeline;
    GstDiscoverer *discoverer;
    std::stringstream discoverer_message;
    std::string codec_name;
    GstVideoFrame v_frame;
    GstVideoInfo v_frame_video_info;
    std::atomic<bool> v_frame_is_full;
    std::atomic<bool> need_loop;

    MediaSegmentSet segments;
    MediaSegmentSet::iterator current_segment;

    bool ready;
    bool seekable;
    bool isimage;
    bool interlaced;

    void execute_open();
    void execute_loop_command();
    void execute_seek_command(GstClockTime target = GST_CLOCK_TIME_NONE);   
    bool fill_v_frame(GstBuffer *buf);

    static GstFlowReturn callback_pull_sample_video (GstElement *bin, MediaPlayer *m);
    static void callback_end_of_video (GstElement *bin, MediaPlayer *m);
    static void callback_discoverer_process (GstDiscoverer *discoverer, GstDiscovererInfo *info, GError *err, MediaPlayer *m);
    static void callback_discoverer_finished(GstDiscoverer *discoverer, MediaPlayer *m);

};



#endif // __GST_MEDIA_PLAYER_H_
