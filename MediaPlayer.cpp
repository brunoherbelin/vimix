#include "MediaPlayer.h"
#include <algorithm>

// vmix
#include "defines.h"
#include "Log.h"
#include "RenderingManager.h"
#include "Resource.h"
#include "Visitor.h"
#include "UserInterfaceManager.h"
#include "SystemToolkit.h"
#include "GstToolkit.h"

//  Desktop OpenGL function loader
#include <glad/glad.h>  

// GStreamer
#include <gst/gl/gl.h>
#include <gst/gstformat.h>
#include <gst/pbutils/gstdiscoverer.h>
#include <gst/app/gstappsink.h>

#ifndef NDEBUG
#define MEDIA_PLAYER_DEBUG
#endif



MediaPlayer::MediaPlayer(string name) : id_(name)
{
    if (std::empty(id_))
        id_ = SystemToolkit::date_time_string();

    uri_ = "undefined";
    pipeline_ = nullptr;
    discoverer_ = nullptr;

    ready_ = false;
    failed_ = false;
    seekable_ = false;
    isimage_ = false;
    interlaced_ = false;
    need_loop_ = false;
    v_frame_is_full_ = false;
    rate_ = 1.0;
    framerate_ = 0.0;

    width_ = par_width_ = 640;
    height_ = 480;
    position_ = GST_CLOCK_TIME_NONE;
    duration_ = GST_CLOCK_TIME_NONE;
    start_position_ = GST_CLOCK_TIME_NONE;
    frame_duration_ = GST_CLOCK_TIME_NONE;
    desired_state_ = GST_STATE_PAUSED;
    loop_ = LoopMode::LOOP_REWIND;
    current_segment_ = segments_.begin();
    v_frame_.buffer = nullptr;

    textureindex_ = 0;
}

MediaPlayer::~MediaPlayer()
{
    close();
    // g_free(v_frame);
}

void MediaPlayer::accept(Visitor& v) {
    v.visit(*this);
}

guint MediaPlayer::texture() const
{
    if (textureindex_ == 0)
        return Resource::getTextureBlack();

    return textureindex_;
}

void MediaPlayer::open(string path)
{
    // set uri to open
    uri_ = string( gst_uri_construct("file", path.c_str()) );

    // reset
    ready_ = false;

    /* Instantiate the Discoverer */
    GError *err = NULL;
    discoverer_ = gst_discoverer_new (5 * GST_SECOND, &err);
    if (!discoverer_) {
        Log::Warning("MediaPlayer Error creating discoverer instance: %s\n", err->message);
        g_clear_error (&err);
        failed_ = true;
        return;
    }

    // set callback for filling in information into this MediaPlayer 
    g_signal_connect (discoverer_, "discovered", G_CALLBACK (callback_discoverer_process), this);
    // set callback when finished discovering
    g_signal_connect (discoverer_, "finished", G_CALLBACK (callback_discoverer_finished), this);
    
    // start discoverer
    gst_discoverer_start(discoverer_);
    // Add the request to process asynchronously the URI 
    if (!gst_discoverer_discover_uri_async (discoverer_, uri_.c_str())) {
        Log::Warning("MediaPlayer %s Failed to start discovering URI '%s'\n", id_.c_str(), uri_.c_str());
        g_object_unref (discoverer_);
        discoverer_ = nullptr;
        failed_ = true;
    }

    // and wait for discoverer to finish...
}


void MediaPlayer::execute_open() 
{
    // build string describing pipeline
    string description = "uridecodebin uri=" + uri_ + " name=decoder !";
    if (interlaced_)
        description += " deinterlace !";
    description += " videoconvert ! appsink name=sink";

    // parse pipeline descriptor
    GError *error = NULL;
    pipeline_ = gst_parse_launch (description.c_str(), &error);
    if (error != NULL) {
        Log::Warning("MediaPlayer %s Could not construct pipeline %s:\n%s", id_.c_str(), description.c_str(), error->message);
        g_clear_error (&error);
        failed_ = true;
        return;
    }
    g_object_set(G_OBJECT(pipeline_), "name", id_.c_str(), NULL);

    // GstCaps *caps = gst_static_caps_get (&frame_render_caps);    
    string capstring = "video/x-raw,format=RGBA,width="+ std::to_string(width_) + ",height=" + std::to_string(height_);
    GstCaps *caps = gst_caps_from_string(capstring.c_str());
    if (!gst_video_info_from_caps (&v_frame_video_info_, caps)) {
        Log::Warning("MediaPlayer %s Could not configure video frame info", id_.c_str());
        failed_ = true;
        return;
    }

    // setup appsink
    GstElement *sink = gst_bin_get_by_name (GST_BIN (pipeline_), "sink");
    if (sink) {

        // set all properties 
        g_object_set (sink, "emit-signals", TRUE, "sync", TRUE, "enable-last-sample", TRUE,  
                    "wait-on-eos", FALSE, "max-buffers", 1000, "caps", caps, NULL);

        // connect callbacks
        g_signal_connect(G_OBJECT(sink), "new-sample", G_CALLBACK (callback_pull_sample_video), this);
        g_signal_connect(G_OBJECT(sink), "new-preroll", G_CALLBACK (callback_pull_sample_video), this);
        g_signal_connect(G_OBJECT(sink), "eos", G_CALLBACK (callback_end_of_video), this);

        // Instruct appsink to drop old buffers when the maximum amount of queued buffers is reached.
        // here max-buffers set to 1000
        gst_app_sink_set_drop ( (GstAppSink*) sink, true);
        
        // done with ref to sink
        gst_object_unref (sink);
    } 
    else {
        Log::Warning("MediaPlayer %s Could not configure  sink", id_.c_str());
        failed_ = true;
        return;
    }
    gst_caps_unref (caps);
    
    // capture bus signals to force a unique opengl context for all GST elements 
    //Rendering::LinkPipeline(GST_PIPELINE (pipeline));

    // set to desired state (PLAY or PAUSE)
    GstStateChangeReturn ret = gst_element_set_state (pipeline_, desired_state_);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        Log::Warning("MediaPlayer %s Could not open %s", id_.c_str(), uri_.c_str());
        failed_ = true;
        return;
    }

    // all good
    Log::Info("MediaPlayer %s Open %s (%s %d x %d)", id_.c_str(), uri_.c_str(), codec_name_.c_str(), width_, height_);
    ready_ = true;
}

bool MediaPlayer::isOpen() const
{
    return ready_;
}

bool MediaPlayer::failed() const
{
    return failed_;
}

void MediaPlayer::close()
{
    // stop discovering stream
    if (discoverer_ != nullptr) {
        gst_discoverer_stop (discoverer_);
        g_object_unref (discoverer_);
        discoverer_ = nullptr;
    }

    if (!ready_)
        return;

    // clean up GST
    if (pipeline_ != nullptr) {
        gst_element_set_state (pipeline_, GST_STATE_NULL);
        gst_object_unref (pipeline_);
        pipeline_ = nullptr;
    }

    // nothing to display
    glDeleteTextures(1, &textureindex_);
    textureindex_ = Resource::getTextureBlack();

    // un-ready the media player
    ready_ = false;
}


GstClockTime MediaPlayer::duration()
{
    // cannot play an image
    if (isimage_)
        return GST_CLOCK_TIME_NONE;

    if (duration_ == GST_CLOCK_TIME_NONE && pipeline_ != nullptr) {
        gint64 d = GST_CLOCK_TIME_NONE;
        if ( gst_element_query_duration(pipeline_, GST_FORMAT_TIME, &d) )
            duration_ = d;
    }

    return duration_;
}

GstClockTime MediaPlayer::frameDuration()
{
    return frame_duration_;
}

guint MediaPlayer::width() const
{
    return width_;
}

guint MediaPlayer::height() const
{
    return height_;
}

float MediaPlayer::aspectRatio() const
{
    return static_cast<float>(par_width_) / static_cast<float>(height_);
}

GstClockTime MediaPlayer::position()
{
    GstClockTime pos = position_;
    
    if (pos == GST_CLOCK_TIME_NONE && pipeline_ != nullptr) {
        gint64 p = GST_CLOCK_TIME_NONE;
        if ( gst_element_query_position (pipeline_, GST_FORMAT_TIME, &p) )
            pos = p;
    }
    
    return pos - start_position_;
}

void MediaPlayer::play(bool on)
{
    // cannot play an image
    if (isimage_)
        return;

    // request state 
    GstState requested_state = on ? GST_STATE_PLAYING : GST_STATE_PAUSED;
    // ignore if requesting twice same state
    if (desired_state_ == requested_state)
        return;

    // accept request to the desired state
    desired_state_ = requested_state;

    // if not ready yet, the requested state will be handled later
    if ( pipeline_ == nullptr  )
        return;

    // requesting to play, but stopped at end of stream : rewind first !
    if ( desired_state_ == GST_STATE_PLAYING) {
        if ( ( rate_>0.0 ? duration_ - position() : position() ) < 2 * frame_duration_ )
            rewind();
    }

    // all ready, apply state change immediately
    GstStateChangeReturn ret = gst_element_set_state (pipeline_, desired_state_);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        Log::Warning("MediaPlayer %s Failed to start", gst_element_get_name(pipeline_));
        failed_ = true;
    }    
#ifdef MEDIA_PLAYER_DEBUG
    else if (on)
        Log::Info("MediaPlayer %s Start", gst_element_get_name(pipeline_));
    else
        Log::Info("MediaPlayer %s Stop", gst_element_get_name(pipeline_));
#endif

    // reset time counter on stop
    if (!on)
        timecount_.reset();
}

bool MediaPlayer::isPlaying(bool testpipeline) const
{
    // image cannot play
    if (isimage_)
        return false;

    // if not ready yet, answer with requested state
    if ( !testpipeline || pipeline_ == nullptr )
        return desired_state_ == GST_STATE_PLAYING;

    // if ready, answer with actual state
    GstState state;
    gst_element_get_state (pipeline_, &state, NULL, GST_CLOCK_TIME_NONE);
    return state == GST_STATE_PLAYING;

    // return desired_state == GST_STATE_PLAYING;
}


MediaPlayer::LoopMode MediaPlayer::loop() const
{
    return loop_;
}
    
void MediaPlayer::setLoop(MediaPlayer::LoopMode mode)
{
    loop_ = mode;
}

void MediaPlayer::rewind()
{
    if (!seekable_)
        return;

    if (rate_ > 0.0)
        // playing forward, loop to begin
        execute_seek_command(0);
    else
        // playing backward, loop to end
        execute_seek_command(duration_);
}


void MediaPlayer::seekNextFrame()
{
    // useful only when Paused
    if (isPlaying())
        return;

    if ( loop_ != LOOP_NONE) {
        // eventually loop if mode allows
        if ( ( rate_>0.0 ? duration_ - position() : position() ) <  2 * frame_duration_ )
                need_loop_ = true;
    }

    // step 
    gst_element_send_event (pipeline_, gst_event_new_step (GST_FORMAT_BUFFERS, 1, ABS(rate_), TRUE,  FALSE));
    
}

void MediaPlayer::seekTo(GstClockTime pos)
{
    if (!seekable_)
        return;

    // apply seek
    GstClockTime target = CLAMP(pos, 0, duration_);
    execute_seek_command(target);

}

void MediaPlayer::fastForward()
{
    if (!seekable_)
        return;

    double step = SIGN(rate_) * 0.01 * static_cast<double>(duration_);
    GstClockTime target = position() + static_cast<GstClockTime>(step);

    // manage loop
    if ( target > duration_ ) {
        if (loop_ == LOOP_NONE)
            target = duration_;
        else 
            target = target - duration_;
    }

    seekTo(target);
}



bool MediaPlayer::addPlaySegment(GstClockTime begin, GstClockTime end)
{
    return addPlaySegment( MediaSegment(begin, end) );
}

bool MediaPlayer::addPlaySegment(MediaSegment s)
{
    if ( s.is_valid() )
        return segments_.insert(s).second;

    return false;
}

bool MediaPlayer::removeAllPlaySegmentOverlap(MediaSegment s)
{
    bool ret = removePlaySegmentAt(s.begin);
    return removePlaySegmentAt(s.end) || ret;
}

bool MediaPlayer::removePlaySegmentAt(GstClockTime t)
{
    MediaSegmentSet::const_iterator s = std::find_if(segments_.begin(), segments_.end(), containsTime(t));

    if ( s != segments_.end() ) {
        segments_.erase(s);
        return true;
    }

    return false;
}

std::list< std::pair<guint64, guint64> > MediaPlayer::getPlaySegments() const
{
    std::list< std::pair<guint64, guint64> > ret;
    for (MediaSegmentSet::iterator it = segments_.begin(); it != segments_.end(); it++)
        ret.push_back( std::make_pair( it->begin, it->end ) );

    return ret;
}

void MediaPlayer::update()
{
    // discard 
    if (!ready_)
        return;

    // done discovering stream
    if (discoverer_ != nullptr) {
        gst_discoverer_stop (discoverer_);
        g_object_unref (discoverer_);
        discoverer_ = nullptr;
    }

    // apply texture
    if (v_frame_is_full_) {
        // first occurence; create texture
        if (textureindex_==0) {
            glActiveTexture(GL_TEXTURE0);
            glGenTextures(1, &textureindex_);
            glBindTexture(GL_TEXTURE_2D, textureindex_);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width_, height_,
                         0, GL_RGBA, GL_UNSIGNED_BYTE, v_frame_.data[0]);
        }
        else // bind texture
        {
            glBindTexture(GL_TEXTURE_2D, textureindex_);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width_, height_,
                            GL_RGBA, GL_UNSIGNED_BYTE, v_frame_.data[0]);
        }        

        // sync with callback_pull_last_sample_video 
        v_frame_is_full_ = false;
    }

    // manage loop mode
    if (need_loop_ && !isimage_) {
        execute_loop_command();
        need_loop_ = false;
    }

    // all other updates below are only for playing mode
    if (desired_state_ != GST_STATE_PLAYING)
        return;

    // test segments
    if ( segments_.begin() != segments_.end()) {

        if ( current_segment_ == segments_.end() )
            current_segment_ = segments_.begin();

        if ( position() > current_segment_->end) {
            g_print("switch to next segment ");
            current_segment_++;
            if ( current_segment_ == segments_.end() )
                current_segment_ = segments_.begin();
            seekTo(current_segment_->begin);
        }

    }

}

void MediaPlayer::execute_loop_command()
{
    if (loop_==LOOP_REWIND) {
        rewind();
    } 
    else if (loop_==LOOP_BIDIRECTIONAL) {
        rate_ *= - 1.f;
        execute_seek_command();
    }
    else {
        play(false);
    }
}

void MediaPlayer::execute_seek_command(GstClockTime target)
{
    if ( pipeline_ == nullptr || !seekable_)
        return;

    GstEvent *seek_event = nullptr;

    // seek position : default to target
    GstClockTime seek_pos = target;

    // no target given
    if (target == GST_CLOCK_TIME_NONE) 
        // create seek event with current position (rate changed ?)
        seek_pos = position();
    // target is given but useless
    else if ( ABS_DIFF(target, position()) < frame_duration_) {
        // ignore request
#ifdef MEDIA_PLAYER_DEBUG
        Log::Info("MediaPlayer %s Ignored seek to current position", id_.c_str());
#endif
        return;
    }

    // seek with flush (always)
    int seek_flags = GST_SEEK_FLAG_FLUSH;
    // seek with trick mode if fast speed
    if ( ABS(rate_) > 2.0 )
        seek_flags |= GST_SEEK_FLAG_TRICKMODE;

    // create seek event depending on direction
    if (rate_ > 0) {
        seek_event = gst_event_new_seek (rate_, GST_FORMAT_TIME, (GstSeekFlags) seek_flags,
            GST_SEEK_TYPE_SET, seek_pos, GST_SEEK_TYPE_END, 0);
    } else {
        seek_event = gst_event_new_seek (rate_, GST_FORMAT_TIME, (GstSeekFlags) seek_flags,
            GST_SEEK_TYPE_SET, 0, GST_SEEK_TYPE_SET, seek_pos);
    }

    // Send the event (ASYNC)
    if (seek_event && !gst_element_send_event(pipeline_, seek_event) )
        Log::Warning("MediaPlayer %s Seek failed", gst_element_get_name(pipeline_));
#ifdef MEDIA_PLAYER_DEBUG
    else
        Log::Info("MediaPlayer %s Seek %ld %f", gst_element_get_name(pipeline_), seek_pos, rate_);
#endif
}

void MediaPlayer::setPlaySpeed(double s)
{
    if (isimage_)
        return;

    // bound to interval [-MAX_PLAY_SPEED MAX_PLAY_SPEED] 
    rate_ = CLAMP(s, -MAX_PLAY_SPEED, MAX_PLAY_SPEED);
    // skip interval [-MIN_PLAY_SPEED MIN_PLAY_SPEED]
    if (ABS(rate_) < MIN_PLAY_SPEED)
        rate_ = SIGN(rate_) * MIN_PLAY_SPEED;
        
    // apply with seek
    execute_seek_command();
}

double MediaPlayer::playSpeed() const
{
    return rate_;
}


std::string MediaPlayer::codec() const
{
    return codec_name_;
}

double MediaPlayer::frameRate() const
{
    return framerate_;
}

double MediaPlayer::updateFrameRate() const
{
    return timecount_.frameRate();
}


// CALLBACKS

bool MediaPlayer::fill_v_frame(GstBuffer *buf, bool ignorepts)
{
    // always empty frame before filling it again
    if (v_frame_.buffer)
        gst_video_frame_unmap(&v_frame_);

    // get the frame from buffer
    if ( !gst_video_frame_map (&v_frame_, &v_frame_video_info_, buf, GST_MAP_READ ) ) {
        Log::Info("MediaPlayer %s Failed to map the video buffer", id_.c_str());
        return false;
    }

    // validate frame format
    if( GST_VIDEO_INFO_IS_RGB(&(v_frame_).info) && GST_VIDEO_INFO_N_PLANES(&(v_frame_).info) == 1) {

        // validate time
        if (ignorepts || position_ != buf->pts)
        {

            // got a new RGB frame !
            v_frame_is_full_ = true;

            // get presentation time stamp
            position_ = buf->pts;

            // set start position (i.e. pts of first frame we got)
            if (start_position_ == GST_CLOCK_TIME_NONE)
                start_position_ = position_;

            // keep update time (i.e. actual FPS of update)
            timecount_.tic();
        }

    }

    return true;
}

GstFlowReturn MediaPlayer::callback_pull_sample_video (GstElement *bin, MediaPlayer *m)
{
    GstFlowReturn ret = GST_FLOW_OK;

    if (m && !m->v_frame_is_full_) {

        // get last sample (non blocking)
        GstSample *sample = nullptr;
        g_object_get (bin, "last-sample", &sample, NULL);        

        // if got a valid sample
        if (sample != nullptr) {

            // get buffer from sample
            GstBuffer *buf = gst_buffer_ref ( gst_sample_get_buffer (sample) );

            // fill frame from buffer
            if ( !m->fill_v_frame(buf, m->isimage_) )
                ret = GST_FLOW_ERROR;
            // free buffer
            gst_buffer_unref (buf);
        }
        else
            ret = GST_FLOW_FLUSHING;

        // cleanup stack of samples (non blocking)
        // NB : overkill as gst_app_sink_set_drop is set to TRUE, but better be safe than leak memory...
        while (sample != nullptr) {
            gst_sample_unref (sample);
            sample = gst_app_sink_try_pull_sample( (GstAppSink * ) bin, 0 );
        }

    }

    return ret;
}

void MediaPlayer::callback_end_of_video (GstElement *, MediaPlayer *m)
{
    if (m) {
        // reached end of stream (eos) : might need to loop !
        m->need_loop_ = true;
    }
}

void MediaPlayer::callback_discoverer_process (GstDiscoverer *discoverer, GstDiscovererInfo *info, GError *err, MediaPlayer *m)
{
    if (!discoverer || !m)
        return;

    // handle general errors
    const gchar *uri = gst_discoverer_info_get_uri (info);
    GstDiscovererResult result = gst_discoverer_info_get_result (info);
    switch (result) {
        case GST_DISCOVERER_URI_INVALID:
            m->discoverer_message_ << "Invalid URI: " << uri;
        break;
        case GST_DISCOVERER_ERROR:
            m->discoverer_message_ << "Error: " << err->message;
        break;
        case GST_DISCOVERER_TIMEOUT:
            m->discoverer_message_ << "Time out";
        break;
        case GST_DISCOVERER_BUSY:
            m->discoverer_message_ << "Busy";
        break;
        case GST_DISCOVERER_MISSING_PLUGINS:
        {
            const GstStructure *s = gst_discoverer_info_get_misc (info);
            gchar *str = gst_structure_to_string (s);
            m->discoverer_message_ << "Unknown file format / " << str;
            g_free (str);
        }
        break;
        case GST_DISCOVERER_OK:
        break;
    }
    // no error, handle information found
    if ( result == GST_DISCOVERER_OK ) {

        // look for video stream at that uri
        bool foundvideostream = false;
        GList *streams = gst_discoverer_info_get_video_streams(info);
        GList *tmp;
        for (tmp = streams; tmp && !foundvideostream; tmp = tmp->next ) {
            GstDiscovererStreamInfo *tmpinf = (GstDiscovererStreamInfo *) tmp->data;
            if ( GST_IS_DISCOVERER_VIDEO_INFO(tmpinf) )
            {
                // found a video / image stream : fill-in information
                GstDiscovererVideoInfo* vinfo = GST_DISCOVERER_VIDEO_INFO(tmpinf);
                m->width_ = gst_discoverer_video_info_get_width(vinfo);
                m->height_ = gst_discoverer_video_info_get_height(vinfo);
                m->isimage_ = gst_discoverer_video_info_is_image(vinfo);
                m->interlaced_ = gst_discoverer_video_info_is_interlaced(vinfo);
                guint parn = gst_discoverer_video_info_get_par_num(vinfo);
                guint pard = gst_discoverer_video_info_get_par_denom(vinfo);
                m->par_width_ = (m->width_ * parn) / pard;
                // if its a video, it duration, framerate, etc.
                if ( !m->isimage_ ) {
                    m->duration_ = gst_discoverer_info_get_duration (info);
                    m->seekable_ = gst_discoverer_info_get_seekable (info);
                    guint frn = gst_discoverer_video_info_get_framerate_num(vinfo);
                    guint frd = gst_discoverer_video_info_get_framerate_denom(vinfo);
                    m->framerate_ = static_cast<double>(frn) / static_cast<double>(frd);
                    m->frame_duration_ = (GST_SECOND * static_cast<guint64>(frd)) / (static_cast<guint64>(frn));
                }
                // try to fill-in the codec information
                GstCaps *caps = gst_discoverer_stream_info_get_caps (tmpinf);
                if (caps) {
                    m->codec_name_ = std::string( gst_pb_utils_get_codec_description(caps) );
                    gst_caps_unref (caps);
                }
//                const GstTagList *tags = gst_discoverer_stream_info_get_tags(tmpinf);
//                if ( tags ) {
//                    gchar *container = NULL;
//                    gst_tag_list_get_string(tags, GST_TAG_CONTAINER_FORMAT, &container);
//                    if (container)  m->codec_name = std::string(container) + " ";
//                    gchar *codec = NULL;
//                    gst_tag_list_get_string(tags, GST_TAG_VIDEO_CODEC, &codec);
//                    if (!codec) gst_tag_list_get_string (tags, GST_TAG_CODEC, &codec);
//                    if (codec)  m->codec_name += std::string(codec);
//                }
                // exit loop
                foundvideostream = true;
            }
        }
        gst_discoverer_stream_info_list_free(streams);

        if (!foundvideostream) {
            m->discoverer_message_ << "No video stream.";
        }
    
    }
}

void MediaPlayer::callback_discoverer_finished(GstDiscoverer *discoverer, MediaPlayer *m)
{
    // finished the discoverer : finalize open status
    if (discoverer && m) {
        // no error message, open media
        if ( m->discoverer_message_.str().empty())
            m->execute_open();
        else {
            Log::Warning("MediaPlayer %s Failed to open %s\n%s", m->id_.c_str(), m->uri_.c_str(), m->discoverer_message_.str().c_str());
            m->discoverer_message_.clear();
            m->failed_ = true;
        }
    }
}

TimeCounter::TimeCounter() {

    reset();
}

void TimeCounter::tic ()
{
    // how long since last time
    current_time = gst_util_get_timestamp ();
    gint64 dt = current_time - last_time;

    // one more frame since last time
    nbFrames++;

    // calculate instantaneous framerate
    // Exponential moving averate with previous framerate to filter jutter (50/50)
    // The divition of frame/time is done on long integer GstClockTime, counting in microsecond
    // NB: factor 100 to get 0.01 precision
    fps = 0.5f * fps + 0.005f * static_cast<float>( ( 100 * GST_SECOND * nbFrames ) / dt );

    // reset counter every second
    if ( dt >= GST_SECOND)
    {
        last_time = current_time;
        nbFrames = 0;
    }

}

void TimeCounter::reset ()
{
    current_time = gst_util_get_timestamp ();
    last_time = gst_util_get_timestamp();
    nbFrames = 0;
    fps = 0.f;
}

float TimeCounter::frameRate() const
{
    return  fps;
}

