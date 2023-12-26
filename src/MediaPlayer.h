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
#include "Metronome.h"

// Forward declare classes referenced
class Visitor;

#define MAX_PLAY_SPEED 20.0
#define MIN_PLAY_SPEED 0.1
#define N_VFRAME 5

struct MediaInfo {

    guint width;
    guint par_width;  // width to match pixel aspect ratio
    guint height;
    guint bitrate;
    guint framerate_n;
    guint framerate_d;
    std::string codec_name;
    bool isimage;
    bool interlaced;
    bool seekable;
    bool valid;
    GstClockTime dt;
    GstClockTime end;
    std::string log;
    bool hasaudio;

    MediaInfo() {
        width = par_width = 1;
        height = 1;
        bitrate = 0;
        framerate_n = 0;
        framerate_d = 1;
        codec_name = "Undefined";
        isimage = false;
        interlaced = false;
        seekable = false;
        valid = false;
        hasaudio = false;
        dt  = GST_CLOCK_TIME_NONE;
        end = GST_CLOCK_TIME_NONE;
        log = "";
    }
};

class MediaPlayer {

public:

    /**
     * Constructor of a GStreamer Media Player
     */
    MediaPlayer();
    /**
     * Destructor.
     */
    ~MediaPlayer();
    /**
     * Get unique id
     */
    inline uint64_t id() const { return id_; }
    /** 
     * Open a media using gstreamer URI 
     * */
    void open ( const std::string &filename, const std::string &uri = "");
    void reopen ();
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
    MediaInfo media() const;
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
     * True if it has only one frame
     * */
    bool singleFrame() const;
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
    void setRate(double s);
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
     * Step when paused
     * (aka next frame)
     * Can go backward if play speed is negative
     * */
    void step(uint milisecond = 0);
    /**
     * Step forward when playing
     * (aka fast-forward)
     * Can go backward if play speed is negative
     * */
    void jump(uint milisecond = 200);
    /**
     * Seek to zero
     * */
    void rewind(bool force = false);
    /**
     * pending
     * */
    bool pending() const { return pending_; }
    /**
     * Get position time
     * */
    GstClockTime position();
    /**
     * go to a valid position in media timeline
     * pos in nanoseconds.
     * return true if seek is performed
     * */
    bool go_to(GstClockTime pos);
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
    Timeline *timeline();
    void setTimeline(const Timeline &tl);
    /**
     * Get fading value at current time
     * */
    float currentTimelineFading();
    /**
     * Get fading mode
     * */
    typedef enum { FADING_COLOR = 0, FADING_ALPHA = 1 } FadingMode;
    FadingMode timelineFadingMode() { return fading_mode_; }
    /**
     * Set fading mode
     * */
    void setTimelineFadingMode(FadingMode m) { fading_mode_ = m; }
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
     * Get frames display aspect ratio
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
     * Forces open using software decoding
     * (i.e. without hadrware decoding)
     * NB: this reopens the video and reset decoder name
     * */
    void setSoftwareDecodingForced(bool on);
    bool softwareDecodingForced();
    /**
     * Option to automatically rewind each time the player is disabled
     * (i.e. when enable(false) is called )
     * */
    inline void setRewindOnDisabled(bool on) { rewind_on_disable_ = on; }
    inline bool rewindOnDisabled() const { return rewind_on_disable_; }
    /**
     * Option to synchronize with metronome
     * */
    inline void setSyncToMetronome(Metronome::Synchronicity s) { metro_sync_ = s; }
    inline Metronome::Synchronicity syncToMetronome() const { return metro_sync_; }
    /**
     * Adds a video effect into the gstreamer pipeline
     * NB: setVideoEffect reopens the video
     * */
    void setVideoEffect(const std::string &pipeline_element);
    inline std::string videoEffect() { return video_filter_; }
    inline bool videoEffectAvailable() { return video_filter_available_; }
    /**
     * Enables or disables audio
     * NB: setAudioEnabled reopens the video
     * */
    void setAudioEnabled(bool on);
    void setAudioVolume(int vol = -1);
    void setAudioVolumeFactor(uint index, float value);
    typedef enum  {
        VOLUME_ONLY = 0,
        VOLUME_MULT_1 = 1,
        VOLUME_MULT_2 = 2,
        VOLUME_MULT_BOTH = 3
    } VolumeFactorsMix;
    void setAudioVolumeMix(VolumeFactorsMix m);
    inline VolumeFactorsMix audioVolumeMix() const { return audio_volume_mix_; }
    inline bool audioEnabled() const { return audio_enabled_; }
    inline int  audioVolume()  const { return (int) (audio_volume_[0] * 100.f); }
    inline bool audioAvailable() const { return media_.hasaudio; }

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
    /**
     * Discoverer to check uri and get media info
     * */
    static MediaInfo UriDiscoverer(const std::string &uri);
    std::string log() const { return media_.log; }


private:

    // video player description
    uint64_t id_;
    std::string filename_;
    std::string uri_;
    guint textureindex_;

    // general properties of media
    MediaInfo media_;
    Timeline timeline_;
    FadingMode fading_mode_;
    std::future<MediaInfo> discoverer_;

    // GST & Play status
    GstClockTime position_;
    LoopMode loop_;
    GstState desired_state_;
    GstElement *pipeline_;
    GstVideoInfo v_frame_video_info_;
    std::atomic<bool> opened_;
    std::atomic<bool> failed_;
    bool force_update_;
    bool pending_;
    bool seeking_;
    bool enabled_;
    bool rewind_on_disable_;
    bool force_software_decoding_;
    std::string decoder_name_;
    bool video_filter_available_;
    std::string video_filter_;

    // audio
    bool audio_enabled_;
    float audio_volume_[3];
    VolumeFactorsMix audio_volume_mix_;

    // Play speed
    gdouble rate_;
    typedef enum  {
        RATE_CHANGE_NONE = 0,
        RATE_CHANGE_INSTANT = 1,
        RATE_CHANGE_FLUSH = 2
    } RateChangeMode;
    RateChangeMode rate_change_;

    Metronome::Synchronicity metro_sync_;

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
        void unmap();
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
    void execute_play_command(bool on);
    void execute_loop_command();
    void execute_seek_command(GstClockTime target = GST_CLOCK_TIME_NONE, bool force = false);

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
