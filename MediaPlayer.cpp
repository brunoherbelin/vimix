#include <thread>

using namespace std;

//  Desktop OpenGL function loader
#include <glad/glad.h>


// vmix
#include "defines.h"
#include "Log.h"
#include "Resource.h"
#include "Visitor.h"
#include "SystemToolkit.h"

#include "MediaPlayer.h"

#ifndef NDEBUG
#define MEDIA_PLAYER_DEBUG
#endif

std::list<MediaPlayer*> MediaPlayer::registered_;

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
    rate_ = 1.0;
    framerate_ = 0.0;

    width_ = par_width_ = 640;
    height_ = 480;
    position_ = GST_CLOCK_TIME_NONE;
    desired_state_ = GST_STATE_PAUSED;
    loop_ = LoopMode::LOOP_REWIND;

    // start index in frame_ stack
    write_index_ = 0;
    last_index_ = 0;

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
    gchar *uritmp = gst_filename_to_uri(path.c_str(), NULL);
    uri_ = string( uritmp );
    g_free(uritmp);

    // reset
    ready_ = false;

    /* Instantiate the Discoverer */
    GError *err = NULL;
    discoverer_ = gst_discoverer_new (45 * GST_SECOND, &err);
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

    // Build string describing pipeline
    // NB: video convertion chroma-resampler
    //            Duplicates the samples when upsampling and drops when downsampling 0
    //            Uses linear interpolation 1 (default)
    //            Uses cubic interpolation 2
    //            Uses sinc interpolation 3
    string description = "uridecodebin uri=" + uri_ + " ! ";
    if (interlaced_)
        description += "deinterlace ! ";
    description += "videoconvert chroma-resampler=2 ! appsink name=sink";

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

        // instruct the sink to send samples synched in time
        gst_base_sink_set_sync (GST_BASE_SINK(sink), true);

        // instruct sink to use the required caps
        gst_app_sink_set_caps (GST_APP_SINK(sink), caps);

        // Instruct appsink to drop old buffers when the maximum amount of queued buffers is reached.
        gst_app_sink_set_max_buffers( GST_APP_SINK(sink), 100);
        gst_app_sink_set_drop (GST_APP_SINK(sink), true);

//        // set the callbacks
//        GstAppSinkCallbacks callbacks;
//        callbacks.new_preroll = callback_new_preroll;
//        if (isimage_) {
//            callbacks.eos = NULL;
//            callbacks.new_sample = NULL;
//        }
//        else {
//            callbacks.eos = callback_end_of_stream;
//            callbacks.new_sample = callback_new_sample;
//        }
//        gst_app_sink_set_callbacks (GST_APP_SINK(sink), &callbacks, this, NULL);
//        gst_app_sink_set_emit_signals (GST_APP_SINK(sink), false);

        // connect callbacks
        g_signal_connect(G_OBJECT(sink), "new-sample", G_CALLBACK (callback_new_sample), this);
        g_signal_connect(G_OBJECT(sink), "new-preroll", G_CALLBACK (callback_new_preroll), this);
        g_signal_connect(G_OBJECT(sink), "eos", G_CALLBACK (callback_end_of_stream), this);
        gst_app_sink_set_emit_signals (GST_APP_SINK(sink), true);


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

    // in case discoverer failed to get duration
    if (timeline.end() == GST_CLOCK_TIME_NONE) {
        gint64 d = GST_CLOCK_TIME_NONE;
        if ( gst_element_query_duration(pipeline_, GST_FORMAT_TIME, &d) )
            timeline.setEnd(d);
    }

    // all good
    Log::Info("MediaPlayer %s Open %s (%s %d x %d)", id_.c_str(), uri_.c_str(), codec_name_.c_str(), width_, height_);
    ready_ = true;


    // register media player
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

    // not openned? nothing to close!
    if (!ready_)
        return;

    // un-ready the media player
    ready_ = false;

    // reset timeline
    timeline.reset();

    // clean up GST
    if (pipeline_ != nullptr) {
        GstStateChangeReturn ret = gst_element_set_state (pipeline_, GST_STATE_NULL);
        if (ret == GST_STATE_CHANGE_ASYNC) {
            GstState state;
            gst_element_get_state (pipeline_, &state, NULL, GST_CLOCK_TIME_NONE);
        }
        gst_object_unref (pipeline_);
        pipeline_ = nullptr;
    }

    // cleanup eventual remaining frame memory
    for(guint i = 0; i < N_VFRAME; i++){
        if ( frame_[i].full ) {
            gst_video_frame_unmap(&frame_[i].vframe);
            frame_[i].status = INVALID;
        }
    }

    // cleanup opengl texture
    if (textureindex_)
        glDeleteTextures(1, &textureindex_);
    textureindex_ = 0;

    // cleanup picture buffer
    if (pbo_[0])
        glDeleteBuffers(2, pbo_);
    pbo_size_ = 0;

    // unregister media player
    MediaPlayer::registered_.remove(this);
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
    
    return pos - timeline.start();
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


bool MediaPlayer::isImage() const
{
    return isimage_;
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
    if ( pipeline_ == nullptr )
        return;

    // requesting to play, but stopped at end of stream : rewind first !
    if ( desired_state_ == GST_STATE_PLAYING) {
        if ( ( rate_>0.0 ? timeline.end() - position() : position() ) < 2 * timeline.step() )
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
    if (!on)
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
        execute_seek_command(timeline.end() - timeline.step());
}


void MediaPlayer::step()
{
    // useful only when Paused
    if (!enabled_ || isPlaying())
        return;

    if ( position_ == ( rate_ < 0.0 ? timeline.start() : timeline.end() ) )
        rewind();

    // step 
    gst_element_send_event (pipeline_, gst_event_new_step (GST_FORMAT_BUFFERS, 1, ABS(rate_), TRUE,  FALSE));
}

void MediaPlayer::seek(GstClockTime pos)
{
    if (!enabled_ || !seekable_)
        return;

    // apply seek
    GstClockTime target = CLAMP(pos, 0, timeline.end());
//    GstClockTime target = CLAMP(pos, timeline.start(), timeline.end());
    execute_seek_command(target);

}

void MediaPlayer::jump()
{
    if (!enabled_ || !isPlaying())
        return;

    gst_element_send_event (pipeline_, gst_event_new_step (GST_FORMAT_BUFFERS, 1, 30.f * ABS(rate_), TRUE,  FALSE));
}

void MediaPlayer::init_texture(guint index)
{
    glActiveTexture(GL_TEXTURE0);
    glGenTextures(1, &textureindex_);
    glBindTexture(GL_TEXTURE_2D, textureindex_);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, width_, height_);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width_, height_,
                    GL_RGBA, GL_UNSIGNED_BYTE, frame_[index].vframe.data[0]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    if (!isimage_) {

        // set pbo image size
        pbo_size_ = height_ * width_ * 4;

        // create pixel buffer objects,
        if (pbo_[0])
            glDeleteBuffers(2, pbo_);
        glGenBuffers(2, pbo_);

        for(int i = 0; i < 2; i++ ) {
            // create 2 PBOs
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo_[i]);
            // glBufferDataARB with NULL pointer reserves only memory space.
            glBufferData(GL_PIXEL_UNPACK_BUFFER, pbo_size_, 0, GL_STREAM_DRAW);
            // fill in with reset picture
            GLubyte* ptr = (GLubyte*) glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
            if (ptr)  {
                // update data directly on the mapped buffer
                memmove(ptr, frame_[index].vframe.data[0], pbo_size_);
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


void MediaPlayer::fill_texture(guint index)
{
    // is this the first frame ?
    if (textureindex_ < 1)
    {
        // initialize texture
        init_texture(index);

    }
    else {
        glBindTexture(GL_TEXTURE_2D, textureindex_);

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
                memmove(ptr, frame_[index].vframe.data[0], pbo_size_);

                // release pointer to mapping buffer
                glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
            }
            // done with PBO
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        }
        else {
            // without PBO, use standard opengl (slower)
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width_, height_,
                            GL_RGBA, GL_UNSIGNED_BYTE, frame_[index].vframe.data[0]);
        }
    }
}

void MediaPlayer::update()
{
    // discard
    if (!ready_) {
        timecount_.reset();
        return;
    }

    // done discovering stream
    if (discoverer_ != nullptr) {
        gst_discoverer_stop (discoverer_);
        g_object_unref (discoverer_);
        discoverer_ = nullptr;
    }

    // prevent unnecessary updates
    if (!enabled_ || (isimage_ && textureindex_>0 ) )
        return;

    // local variables before trying to update
    guint read_index = 0;
    bool need_loop = false;

    // locked access to changing index
    index_lock_.lock();
    read_index = last_index_;
    // Do NOT miss an EOS or a pre-roll
    for (guint i = 0; i < N_VFRAME; ++i) {
        if ( frame_[i].status == EOS || frame_[i].status == PREROLL) {
            read_index = i;
            break;
        }
    }
    index_lock_.unlock();

    // lock frame while reading it
    frame_[read_index].access.lock();

    // do not fill a frame twice
    if (frame_[read_index].status != INVALID ) {


        // is this an End-of-Stream frame ?
        if (frame_[read_index].status == EOS )
        {
            // will execute seek command below (after unlock)
            need_loop = true;
        }
        // otherwise just fill non-empty SAMPLE or PREROLL
        else if (frame_[read_index].full)
        {
            // fill the texture with the frame at reading index
            fill_texture(read_index);

            // double update for pre-roll frame and dual PBO (ensure frame is displayed now)
            if (frame_[read_index].status == PREROLL && pbo_size_ > 0)
                fill_texture(read_index);
        }

        // we just displayed a vframe : set position time to frame PTS
        position_ = frame_[read_index].position;

        // avoid reading it again
        frame_[read_index].status = INVALID;

//        // TODO : try to do something when the update is too slow :(
//        if ( timecount_.dt() > frame_duration_  * 2) {
//            Log::Info("frame late %d", 2 * frame_duration_);
//        }
    }

    // unkock frame after reading it
    frame_[read_index].access.unlock();

    if (isimage_)
        return;

    // manage loop mode
    if (need_loop) {
        execute_loop_command();
    }

    // manage timeline
//    TimeInterval gap;
//    if (position_ != GST_CLOCK_TIME_NONE && timeline.gapAt(position_, gap)) {
//        seek( (rate_>0.f) ? gap.end : gap.begin);

//        // TODO : manage loop when jumping out of timeline

//    }

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

    // seek position : default to target
    GstClockTime seek_pos = target;

    // no target given
    if (target == GST_CLOCK_TIME_NONE) 
        // create seek event with current position (rate changed ?)
        seek_pos = position();
    // target is given but useless
    else if ( ABS_DIFF(target, position()) < timeline.step()) {
        // ignore request
        return;
    }

    // seek with flush (always)
    int seek_flags = GST_SEEK_FLAG_FLUSH;
    // seek with trick mode if fast speed
    if ( ABS(rate_) > 1.0 )
        seek_flags |= GST_SEEK_FLAG_TRICKMODE;

//    bool ret = false;

//    if (rate_ > 0) {

//        ret = gst_element_seek (pipeline_, rate_, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH,
//                                 GST_SEEK_TYPE_SET, seek_pos,
//                                 GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE);
//    }
//    else {

//        ret = gst_element_seek (pipeline_, rate_, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH,
//                                GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE,
//                                GST_SEEK_TYPE_SET, seek_pos);
//    }

//    if (!ret)
//         Log::Warning("MediaPlayer %s Seek failed", gst_element_get_name(pipeline_));

    // create seek event depending on direction
    GstEvent *seek_event = nullptr;
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
        Log::Warning("MediaPlayer %s Seek failed", id_.c_str());
    else {
#ifdef MEDIA_PLAYER_DEBUG
        Log::Info("MediaPlayer %s Seek %ld %f", id_.c_str(), seek_pos, rate_);
#endif
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

bool MediaPlayer::fill_frame(GstBuffer *buf, FrameStatus status)
{
    // lock access to frame
    frame_[write_index_].access.lock();

    // always empty frame before filling it again
    if ( frame_[write_index_].full ) {
        gst_video_frame_unmap(&frame_[write_index_].vframe);
        frame_[write_index_].full = false;
    }

    // accept status of frame received
    frame_[write_index_].status = status;

    // a buffer is given (not EOS)
    if (buf != NULL) {

        // get the frame from buffer
        if ( !gst_video_frame_map (&frame_[write_index_].vframe, &v_frame_video_info_, buf, GST_MAP_READ ) )
        {
            Log::Info("MediaPlayer %s Failed to map the video buffer", id_.c_str());
            // free access to frame & exit
            frame_[write_index_].status = INVALID;
            frame_[write_index_].access.unlock();
            return false;
        }

        // successfully filled the frame
        frame_[write_index_].full = true;

        // validate frame format
        if( GST_VIDEO_INFO_IS_RGB(&(frame_[write_index_].vframe).info) && GST_VIDEO_INFO_N_PLANES(&(frame_[write_index_].vframe).info) == 1)
        {
            // set presentation time stamp
            frame_[write_index_].position = buf->pts;

            // set the start position (i.e. pts of first frame we got)
            if (timeline.start() == GST_CLOCK_TIME_NONE)
                timeline.setStart(buf->pts);
        }
        // full but invalid frame : will be deleted next iteration
        // (should never happen)
        else
            frame_[write_index_].status = INVALID;
    }
    // else; null buffer for EOS: give a position
    else {
        frame_[write_index_].status = EOS;
        frame_[write_index_].position = rate_ > 0.0 ? timeline.end() : timeline.start();
    }

    // unlock access to frame
    frame_[write_index_].access.unlock();

    // lock access to index change (very quick)
    index_lock_.lock();
    // indicate update that this is the last frame filled (and unlocked)
    last_index_ = write_index_;
    // for writing, we will access the next in stack
    write_index_ = (write_index_ + 1) % N_VFRAME;
    // unlock access to index change
    index_lock_.unlock();

    // calculate actual FPS of update
    timecount_.tic();

    return true;
}

void MediaPlayer::callback_end_of_stream (GstAppSink *, gpointer p)
{
    MediaPlayer *m = (MediaPlayer *)p;
    if (m && m->ready_) {
        m->fill_frame(NULL, MediaPlayer::EOS);
    }
}

GstFlowReturn MediaPlayer::callback_new_preroll (GstAppSink *sink, gpointer p)
{
    GstFlowReturn ret = GST_FLOW_OK;

    // blocking read pre-roll samples
    GstSample *sample = gst_app_sink_pull_preroll(sink);

    // if got a valid sample
    if (sample != NULL) {

        // get buffer from sample
        GstBuffer *buf = gst_sample_get_buffer (sample);

        // send frames to media player only if ready
        MediaPlayer *m = (MediaPlayer *)p;
        if (m && m->ready_) {
            // fill frame from buffer
            if ( !m->fill_frame(buf, MediaPlayer::PREROLL) )
                ret = GST_FLOW_ERROR;
            // loop negative rate: emulate an EOS
            else if (m->playSpeed() < 0.f && buf->pts <= m->timeline.start()) {
                m->fill_frame(NULL, MediaPlayer::EOS);
            }
        }
    }
    else
        ret = GST_FLOW_FLUSHING;

    // release sample
    gst_sample_unref (sample);

    return ret;
}

GstFlowReturn MediaPlayer::callback_new_sample (GstAppSink *sink, gpointer p)
{
    GstFlowReturn ret = GST_FLOW_OK;

    // non-blocking read new sample
    GstSample *sample = gst_app_sink_pull_sample(sink);

    // if got a valid sample
    if (sample != NULL && !gst_app_sink_is_eos (sink)) {

        // get buffer from sample (valid until sample is released)
        GstBuffer *buf = gst_sample_get_buffer (sample) ;

        // send frames to media player only if ready
        MediaPlayer *m = (MediaPlayer *)p;
        if (m && m->ready_) {
            // fill frame with buffer
            if ( !m->fill_frame(buf, MediaPlayer::SAMPLE) )
                ret = GST_FLOW_ERROR;
            // loop negative rate: emulate an EOS
            else if (m->playSpeed() < 0.f && buf->pts <= m->timeline.start()) {
                m->fill_frame(NULL, MediaPlayer::EOS);
            }
        }
    }
    else
        ret = GST_FLOW_FLUSHING;

    // release sample
    gst_sample_unref (sample);

    return ret;
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
                m->bitrate_ = gst_discoverer_video_info_get_bitrate(vinfo);
                guint parn = gst_discoverer_video_info_get_par_num(vinfo);
                guint pard = gst_discoverer_video_info_get_par_denom(vinfo);
                m->par_width_ = (m->width_ * parn) / pard;
                // if its a video, it duration, framerate, etc.
                if ( !m->isimage_ ) {
                    m->timeline.setEnd( gst_discoverer_info_get_duration (info) );
                    m->seekable_ = gst_discoverer_info_get_seekable (info);
                    guint frn = gst_discoverer_video_info_get_framerate_num(vinfo);
                    guint frd = gst_discoverer_video_info_get_framerate_denom(vinfo);
                    if (frn == 0 || frd == 0) {
                        frn = 25;
                        frd = 1;
                    }
                    m->framerate_ = static_cast<double>(frn) / static_cast<double>(frd);
                    m->timeline.setStep( (GST_SECOND * static_cast<guint64>(frd)) / (static_cast<guint64>(frn)) );
                }
                // try to fill-in the codec information
                GstCaps *caps = gst_discoverer_stream_info_get_caps (tmpinf);
                if (caps) {
                    gchar *codecstring = gst_pb_utils_get_codec_description(caps);
                    m->codec_name_ = std::string( codecstring ) + " ";
                    g_free(codecstring);
                    gst_caps_unref (caps);
                }
//                const GstTagList *tags = gst_discoverer_stream_info_get_tags(tmpinf);
//                if ( tags ) {
//                    gchar *container = NULL;
//                    gst_tag_list_get_string (tags, GST_TAG_CONTAINER_FORMAT, &container);
//                    if (container)  m->codec_name_ += std::string(container);
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

MediaPlayer::TimeCounter::TimeCounter() {

    reset();
}

void MediaPlayer::TimeCounter::tic ()
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
    fps = 0.5 * fps + 0.005 * static_cast<double>( ( 100 * GST_SECOND * nbFrames ) / dt );

    // reset counter every second
    if ( dt >= GST_SECOND)
    {
        last_time = t;
        nbFrames = 0;
    }
}

GstClockTime MediaPlayer::TimeCounter::dt ()
{
    GstClockTime t = gst_util_get_timestamp ();
    GstClockTime dt = t - tic_time;
    tic_time = t;

    // return the instantaneous delta t
    return dt;
}

void MediaPlayer::TimeCounter::reset ()
{
    last_time = gst_util_get_timestamp ();;
    tic_time = last_time;
    nbFrames = 0;
    fps = 0.0;
}

double MediaPlayer::TimeCounter::frameRate() const
{
    return fps;
}

