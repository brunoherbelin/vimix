#include "MediaPlayer.h"
#include <algorithm>
#include <thread>

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

std::list<MediaPlayer*> MediaPlayer::registered_;

static bool is_lower(GstClockTime a, GstClockTime b) {
    if ( a == GST_CLOCK_TIME_NONE || b == GST_CLOCK_TIME_NONE )
        return true;
    return a < b;
}

static bool is_higher(GstClockTime a, GstClockTime b) {
    if ( a == GST_CLOCK_TIME_NONE || b == GST_CLOCK_TIME_NONE )
        return true;
    return a > b;
}

static bool always_true(GstClockTime a, GstClockTime b) {
    return true;
}


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
    enabled_ = true;
    need_loop_ = false;
    rate_ = 1.0;
    framerate_ = 0.0;

    width_ = par_width_ = 640;
    height_ = 480;
    position_ = GST_CLOCK_TIME_NONE;
    duration_ = GST_CLOCK_TIME_NONE;
    start_position_ = GST_CLOCK_TIME_NONE;
    frame_duration_ = GST_CLOCK_TIME_NONE;
    TimeComparator_ = &is_higher;
    desired_state_ = GST_STATE_PAUSED;
    loop_ = LoopMode::LOOP_REWIND;
    current_segment_ = segments_.begin();

    vframe_write_index_ = 0;
    vframe_read_index_ = 0;
    for(guint i = 0; i < N_VFRAME; i++){
        vframe_[i].buffer = nullptr;
        vframe_is_full_[i] = false;
        vframe_position_[i] = GST_CLOCK_TIME_NONE;
    }

    // no PBO by default
    pbo_[0] = pbo_[1] = 0;
    pbo_size_ = 0;
    pbo_index_ = 0;
    pbo_next_index_ = 0;

    textureindex_ = 0;
}

MediaPlayer::~MediaPlayer()
{
    close();
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
    filename_ = path;
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
    // Create the simplest gstreamer pipeline possible :
    //         " uridecodebin uri=file:///path_to_file/filename.mp4 ! videoconvert ! appsink "
    // equivalent to gst-launch-1.0 uridecodebin uri=file:///path_to_file/filename.mp4 ! videoconvert ! ximagesink


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
                    "wait-on-eos", FALSE, "max-buffers", 50, "caps", caps, NULL);

        // connect callbacks
        g_signal_connect(G_OBJECT(sink), "new-sample", G_CALLBACK (callback_pull_sample_video), this);
        g_signal_connect(G_OBJECT(sink), "new-preroll", G_CALLBACK (callback_pull_sample_video), this);
        g_signal_connect(G_OBJECT(sink), "eos", G_CALLBACK (callback_end_of_video), this);

        // Instruct appsink to drop old buffers when the maximum amount of queued buffers is reached.
        // here max-buffers set to 100
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

    MediaPlayer::registered_.push_back(this);
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

    // cleanup eventual remaining frame related memory
    for(guint i = 0; i < N_VFRAME; i++){
        if (vframe_[i].buffer)
            gst_video_frame_unmap(&vframe_[i]);
        vframe_is_full_[i] = false;
        vframe_position_[i] = GST_CLOCK_TIME_NONE;
    }


    // cleanup opengl texture
    if (textureindex_)
        glDeleteTextures(1, &textureindex_);
    textureindex_ = 0;

    // delete picture buffer
    if (pbo_[0])
        glDeleteBuffers(2, pbo_);
    pbo_size_ = 0;

    // un-ready the media player
    ready_ = false;

    MediaPlayer::registered_.remove(this);
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

void MediaPlayer::enable(bool on)
{
    if ( !ready_ )
        return;

    if ( enabled_ != on ) {

        enabled_ = on;

        GstState requested_state = GST_STATE_PAUSED;

        if (enabled_) {
            requested_state = desired_state_;
        }

        //  apply state change
        GstStateChangeReturn ret = gst_element_set_state (pipeline_, requested_state);
        if (ret == GST_STATE_CHANGE_FAILURE) {
            Log::Warning("MediaPlayer %s Failed to enable", gst_element_get_name(pipeline_));
            failed_ = true;
        }

    }
}

bool MediaPlayer::isEnabled() const
{
    return enabled_;
}

void MediaPlayer::play(bool on)
{
    // cannot play an image
    if (!enabled_ || isimage_)
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
        Log::Warning("MediaPlayer %s Failed to play", gst_element_get_name(pipeline_));
        failed_ = true;
    }    
#ifdef MEDIA_PLAYER_DEBUG
    else if (on)
        Log::Info("MediaPlayer %s Start", gst_element_get_name(pipeline_));
    else
        Log::Info("MediaPlayer %s Stop", gst_element_get_name(pipeline_));
#endif

    // reset time counter on stop
    if (on)
    {
        if (rate_ > 0)
            TimeComparator_ = &is_higher;
        else
            TimeComparator_ = &is_lower;
    }
    else
        timecount_.reset();

}

bool MediaPlayer::isPlaying(bool testpipeline) const
{
    // image cannot play
    if (isimage_)
        return false;

    // if not ready yet, answer with requested state
    if ( !testpipeline || pipeline_ == nullptr || !enabled_)
        return desired_state_ == GST_STATE_PLAYING;

    // if ready, answer with actual state
    GstState state;
    gst_element_get_state (pipeline_, &state, NULL, GST_CLOCK_TIME_NONE);
    return state == GST_STATE_PLAYING;
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
    if (!enabled_ || !seekable_)
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
    if (!enabled_ || isPlaying())
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
    if (!enabled_ || !seekable_)
        return;

    // apply seek
    GstClockTime target = CLAMP(pos, 0, duration_);
    execute_seek_command(target);

}

void MediaPlayer::fastForward()
{
    if (!enabled_ || !seekable_)
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

void MediaPlayer::init_texture()
{
    glActiveTexture(GL_TEXTURE0);
    glGenTextures(1, &textureindex_);
    glBindTexture(GL_TEXTURE_2D, textureindex_);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width_, height_,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, vframe_[vframe_read_index_].data[0]);

    if (!isimage_) {

        // need to fill image size
        pbo_size_ = height_ * width_ * 4;

        // create pixel buffer objects,
        if (pbo_[0])
            glDeleteBuffers(2, pbo_);
        glGenBuffers(2, pbo_);

        for(int i = 0; i < 2; i++ ) {
            // create  PBO
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo_[i]);
            // glBufferDataARB with NULL pointer reserves only memory space.
            glBufferData(GL_PIXEL_UNPACK_BUFFER, pbo_size_, 0, GL_STREAM_DRAW);
            // fill in with reset picture
            GLubyte* ptr = (GLubyte*) glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
            if (ptr)  {
                // update data directly on the mapped buffer
                memmove(ptr, vframe_[vframe_read_index_].data[0], pbo_size_);
                // release pointer to mapping buffer
                glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
            }
            else {
                // did not work, disable PBO
                glDeleteBuffers(4, pbo_);
                pbo_[0] = pbo_[1] = 0;
                pbo_size_ = 0;
                break;
            }

        }

        // should be good to go, wrap it up
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        pbo_index_ = 0;
        pbo_next_index_ = 1;

#ifdef MEDIA_PLAYER_DEBUG
        Log::Info("MediaPlayer %s Using Pixel Buffer Object texturing.", id_.c_str());
#endif
    }

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

    if (!enabled_)
        return;

    // bind texture in any case (except if not initialized yet)
    if (textureindex_>0)
        glBindTexture(GL_TEXTURE_2D, textureindex_);

    // try to access a vframe (non blocking)
    guint i = vframe_read_index_;
    if (vframe_lock_[i].try_lock() )
    {
        // if we got a full vframe
        if (vframe_is_full_[vframe_read_index_] && (*TimeComparator_)(vframe_position_[vframe_read_index_], position_) )
        {
            // first occurence; create texture
            if (textureindex_==0) {
                init_texture();
            }
            // all other times, fill the texture
            else
            {
                // use dual Pixel Buffer Object
                if (pbo_size_ > 0) {
                    // In dual PBO mode, increment current index first then get the next index
                    pbo_index_ = (pbo_index_ + 1) % 2;
                    pbo_next_index_ = (pbo_index_ + 1) % 2;

                    // bind PBO to read pixels
                    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo_[pbo_index_]);

                    // copy pixels from PBO to texture object
                    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width_, height_, GL_RGBA, GL_UNSIGNED_BYTE, 0);

                    // bind the next PBO to write pixels
                    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo_[pbo_next_index_]);
                    // See http://www.songho.ca/opengl/gl_pbo.html#map for more details
                    glBufferData(GL_PIXEL_UNPACK_BUFFER, pbo_size_, 0, GL_STREAM_DRAW);
                    // map the buffer object into client's memory
                    GLubyte* ptr = (GLubyte*) glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
                    if (ptr) {
                        // update data directly on the mapped buffer
                        // NB : equivalent but faster (memmove instead of memcpy ?) than
                        // glNamedBufferSubData(pboIds[nextIndex], 0, imgsize, vp->getBuffer())
                        memmove(ptr, vframe_[vframe_read_index_].data[0], pbo_size_);

                        // release pointer to mapping buffer
                        glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
                    }
                    // done with PBO
                    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
                }
                else {
                    // without PBO, use standard opengl (slower)
                    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width_, height_,
                                    GL_RGBA, GL_UNSIGNED_BYTE, vframe_[vframe_read_index_].data[0]);
                }

            }

            // we just displayed a vframe : set position time to vframe PTS
            position_ = vframe_position_[vframe_read_index_];

            // sync with callback_pull_last_sample_video : inform its free
            vframe_is_full_[vframe_read_index_] = false;

            // read next in stack
            vframe_read_index_ = (vframe_read_index_ +1) % N_VFRAME;

        }

        vframe_lock_[i].unlock();
    }


    // manage loop mode
    if (need_loop_ && !isimage_) {
        need_loop_ = false;
        execute_loop_command();
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
    else { //LOOP_NONE
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
    }
    else {
        seek_event = gst_event_new_seek (rate_, GST_FORMAT_TIME, (GstSeekFlags) seek_flags,
            GST_SEEK_TYPE_SET, 0, GST_SEEK_TYPE_SET, seek_pos);
    }

    // Send the event (ASYNC)
    if (seek_event && !gst_element_send_event(pipeline_, seek_event) )
        Log::Warning("MediaPlayer %s Seek failed", gst_element_get_name(pipeline_));
    else {
#ifdef MEDIA_PLAYER_DEBUG
        Log::Info("MediaPlayer %s Seek %ld %f", gst_element_get_name(pipeline_), seek_pos, rate_);
#endif
        // temporarily ignore the ordering test
        TimeComparator_ = &always_true;

        // make sure the intermediate buffered frames are ignored
        guint i = vframe_read_index_;
        vframe_lock_[i].lock();
        if (vframe_write_index_ != vframe_read_index_)
        {
            vframe_lock_[vframe_write_index_].lock();

            // catch-up with vframe stack
            vframe_read_index_ = vframe_write_index_;
#ifdef MEDIA_PLAYER_DEBUG
            Log::Info("MediaPlayer %s reset vframe %d", gst_element_get_name(pipeline_), vframe_write_index_);
#endif
            vframe_lock_[vframe_write_index_].unlock();
        }
        vframe_lock_[i].unlock();
    }
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

std::string MediaPlayer::uri() const
{
    return uri_;
}

std::string MediaPlayer::filename() const
{
    return filename_;
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

bool MediaPlayer::fill_v_frame(GstBuffer *buf)
{

    // non-blocking attempt to access vframe
    guint i = vframe_write_index_;
    if ( vframe_lock_[i].try_lock())
    {
        // blocking  access vframe
//        vframe_lock_[i].lock();

        // always empty frame before filling it again
        if (vframe_[vframe_write_index_].buffer)
            gst_video_frame_unmap(&vframe_[vframe_write_index_]);


        // get the frame from buffer
        if ( !gst_video_frame_map (&vframe_[vframe_write_index_], &v_frame_video_info_, buf, GST_MAP_READ ) ) {
            Log::Info("MediaPlayer %s Failed to map the video buffer", id_.c_str());
            return false;
        }

        // validate frame format
        if( GST_VIDEO_INFO_IS_RGB(&(vframe_[vframe_write_index_]).info) && GST_VIDEO_INFO_N_PLANES(&(vframe_[vframe_write_index_]).info) == 1) {

            // got a new RGB frame !
            // validate time
            if (isimage_ || vframe_position_[vframe_write_index_] != buf->pts)
            {

                // calculate actual FPS of update
                timecount_.tic();

                // get presentation time stamp
                vframe_position_[vframe_write_index_] = buf->pts;

                // set start position (i.e. pts of first frame we got)
                if (start_position_ == GST_CLOCK_TIME_NONE)
                    start_position_ = vframe_position_[vframe_write_index_];

                // sync with MediaPlayer::update() : inform its full
                vframe_is_full_[vframe_write_index_] = true;

                // write next in stack
                vframe_write_index_ = (vframe_write_index_ + 1) % N_VFRAME;
            }
        }

        // unlock
        vframe_lock_[i].unlock();
    }

    return true;
}

GstFlowReturn MediaPlayer::callback_pull_sample_video (GstElement *bin, MediaPlayer *m)
{
    GstFlowReturn ret = GST_FLOW_OK;

    // get last sample (non blocking)
    GstSample *sample = nullptr;
    g_object_get (bin, "last-sample", &sample, NULL);

    // if got a valid sample
    if (sample != nullptr) {

        // get buffer from sample
        GstBuffer *buf = gst_buffer_ref ( gst_sample_get_buffer (sample) );

        if (m) {

            // fill frame from buffer
            if ( !m->fill_v_frame(buf) )
                ret = GST_FLOW_ERROR;

        }

        // free buffer
        gst_buffer_unref (buf);
    }
    else
        ret = GST_FLOW_FLUSHING;

    // cleanup stack of samples (non blocking)
    // NB : possible overkill as gst_app_sink_set_drop is set to TRUE in pipeline
    while (sample != nullptr) {
        gst_sample_unref (sample);
        sample = gst_app_sink_try_pull_sample( (GstAppSink * ) bin, 0 );
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
                    if (frn == 0 || frd == 0) {
                        frn = 25;
                        frd = 1;
                    }
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
    GstClockTime t = gst_util_get_timestamp ();
    GstClockTime dt = t - last_time;

    // one more frame since last time
    nbFrames++;

    // calculate instantaneous framerate
    // Exponential moving averate with previous framerate to filter jitter (50/50)
    // The divition of frame/time is done on long integer GstClockTime, counting in microsecond
    // NB: factor 100 to get 0.01 precision
    fps = 0.5f * fps + 0.005f * static_cast<float>( ( 100 * GST_SECOND * nbFrames ) / dt );

    // reset counter every second
    if ( dt >= GST_SECOND)
    {
        last_time = t;
        nbFrames = 0;
    }
}

GstClockTime TimeCounter::dt ()
{
    GstClockTime t = gst_util_get_timestamp ();
    GstClockTime dt = t - tic_time;
    tic_time = t;

    // return the instantaneous delta t
    return dt;
}

void TimeCounter::reset ()
{
    last_time = gst_util_get_timestamp ();;
    tic_time = last_time;
    nbFrames = 0;
    fps = 0.f;
}

float TimeCounter::frameRate() const
{
    return  fps;
}

