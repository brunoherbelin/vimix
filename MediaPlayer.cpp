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

#define USE_GST_APPSINK_CALLBACKS

std::list<MediaPlayer*> MediaPlayer::registered_;

MediaPlayer::MediaPlayer()
{
    // create unique id
    auto duration = std::chrono::high_resolution_clock::now().time_since_epoch();
    id_ = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count() % 1000000000;

    uri_ = "undefined";
    pipeline_ = nullptr;
    converter_ = nullptr;

    ready_ = false;
    failed_ = false;
    seeking_ = false;
    enabled_ = true;
    rate_ = 1.0;
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

    // OpenGL texture
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

static MediaInfo UriDiscoverer_(std::string uri)
{
#ifdef MEDIA_PLAYER_DEBUG
    Log::Info("Checking '%s'", uri.c_str());
#endif

    MediaInfo video_stream_info;

    /* Instantiate the Discoverer */
    GError *err = NULL;
    GstDiscoverer *discoverer = gst_discoverer_new (15 * GST_SECOND, &err);
    if (!discoverer) {
        Log::Warning("MediaPlayer Error creating discoverer instance: %s\n", err->message);
        g_clear_error (&err);
    }
    else {
        GstDiscovererInfo *info = gst_discoverer_discover_uri (discoverer, uri.c_str(), &err);
        GstDiscovererResult result = gst_discoverer_info_get_result (info);
        switch (result) {
        case GST_DISCOVERER_URI_INVALID:
            Log::Warning("Invalid URI '%s'", uri.c_str());
            break;
        case GST_DISCOVERER_ERROR:
            Log::Warning("Warning: %s", err->message);
            break;
        case GST_DISCOVERER_TIMEOUT:
            Log::Warning("Timeout loading URI '%s'", uri.c_str());
            break;
        case GST_DISCOVERER_BUSY:
            Log::Warning("Busy URI '%s'", uri.c_str());
            break;
        case GST_DISCOVERER_MISSING_PLUGINS:
        {
            const GstStructure *s = gst_discoverer_info_get_misc (info);
            gchar *str = gst_structure_to_string (s);
            Log::Warning("Warning: Unknown file format %s", str);
            g_free (str);
        }
            break;
        default:
        case GST_DISCOVERER_OK:
            break;
        }
        // no error, handle information found
        if ( result == GST_DISCOVERER_OK ) {

            GList *streams = gst_discoverer_info_get_video_streams(info);
            GList *tmp;
            for (tmp = streams; tmp && !video_stream_info.valid; tmp = tmp->next ) {
                GstDiscovererStreamInfo *tmpinf = (GstDiscovererStreamInfo *) tmp->data;
                if ( GST_IS_DISCOVERER_VIDEO_INFO(tmpinf) )
                {
                    // found a video / image stream : fill-in information
                    GstDiscovererVideoInfo* vinfo = GST_DISCOVERER_VIDEO_INFO(tmpinf);
                    video_stream_info.width = gst_discoverer_video_info_get_width(vinfo);
                    video_stream_info.height = gst_discoverer_video_info_get_height(vinfo);
                    guint parn = gst_discoverer_video_info_get_par_num(vinfo);
                    guint pard = gst_discoverer_video_info_get_par_denom(vinfo);
                    video_stream_info.par_width = (video_stream_info.width * parn) / pard;
                    video_stream_info.interlaced = gst_discoverer_video_info_is_interlaced(vinfo);
                    video_stream_info.bitrate = gst_discoverer_video_info_get_bitrate(vinfo);
                    video_stream_info.isimage = gst_discoverer_video_info_is_image(vinfo);
                    // if its a video, it duration, framerate, etc.
                    if ( !video_stream_info.isimage ) {
                        video_stream_info.timeline.setEnd( gst_discoverer_info_get_duration (info) );
                        video_stream_info.seekable = gst_discoverer_info_get_seekable (info);
                        guint frn = gst_discoverer_video_info_get_framerate_num(vinfo);
                        guint frd = gst_discoverer_video_info_get_framerate_denom(vinfo);
                        if (frn == 0 || frd == 0) {
                            frn = 25;
                            frd = 1;
                        }
                        video_stream_info.framerate = static_cast<double>(frn) / static_cast<double>(frd);
                        video_stream_info.timeline.setStep( (GST_SECOND * static_cast<guint64>(frd)) / (static_cast<guint64>(frn)) );
                    }
                    // try to fill-in the codec information
                    GstCaps *caps = gst_discoverer_stream_info_get_caps (tmpinf);
                    if (caps) {
                        gchar *codecstring = gst_pb_utils_get_codec_description(caps);
                        video_stream_info.codec_name = std::string( codecstring );
                        g_free(codecstring);
                        gst_caps_unref (caps);
                    }
                    const GstTagList *tags = gst_discoverer_stream_info_get_tags(tmpinf);
                    if ( tags ) {
                        gchar *container = NULL;
                        if ( gst_tag_list_get_string (tags, GST_TAG_CONTAINER_FORMAT, &container) )
                             video_stream_info.codec_name += " " + std::string(container);
                        g_free(container);
                    }
                    // exit loop
                    // inform that it succeeded
                    video_stream_info.valid = true;
                }
            }
            gst_discoverer_stream_info_list_free(streams);

            if (!video_stream_info.valid) {
                Log::Warning("Warning: No video stream in '%s'", uri.c_str());
            }
        }

        g_object_unref (discoverer);
    }


    // return the info
    return video_stream_info;
}

void MediaPlayer::open(string path)
{
    // set path
    filename_ = path;

    // set uri to open
    gchar *uritmp = gst_filename_to_uri(path.c_str(), NULL);
    uri_ = string( uritmp );
    g_free(uritmp);

    // reset
    ready_ = false;

    // start URI discovering thread:
    discoverer_ = std::async( UriDiscoverer_, uri_);

    // wait for discoverer to finish in the future (test in update)
}


void MediaPlayer::execute_open() 
{   
    // Create the simplest gstreamer pipeline possible :
    //         " uridecodebin uri=file:///path_to_file/filename.mp4 ! videoconvert ! appsink "
    // equivalent to gst-launch-1.0 uridecodebin uri=file:///path_to_file/filename.mp4 ! videoconvert ! ximagesink

    // Build string describing pipeline
    // video deinterlacing method
    //      tomsmocomp (0) – Motion Adaptive: Motion Search
    //      greedyh (1) – Motion Adaptive: Advanced Detection
    //      greedyl (2) – Motion Adaptive: Simple Detection
    //      vfir (3) – Blur Vertical
    //      linear (4) – Linear
    //      scalerbob (6) – Double lines
    // video convertion chroma-resampler
    //      Duplicates the samples when upsampling and drops when downsampling 0
    //      Uses linear interpolation 1 (default)
    //      Uses cubic interpolation 2
    //      Uses sinc interpolation 3
    string description = "uridecodebin uri=" + uri_ + " ! ";
    if (media_.interlaced)
        description += "deinterlace method=2 ! ";
    description += "videoconvert chroma-resampler=2 n-threads=2 ! appsink name=sink";

    // parse pipeline descriptor
    GError *error = NULL;
    pipeline_ = gst_parse_launch (description.c_str(), &error);
    if (error != NULL) {
        Log::Warning("MediaPlayer %s Could not construct pipeline %s:\n%s", std::to_string(id_).c_str(), description.c_str(), error->message);
        g_clear_error (&error);
        failed_ = true;
        return;
    }
    g_object_set(G_OBJECT(pipeline_), "name", std::to_string(id_).c_str(), NULL);

    // GstCaps *caps = gst_static_caps_get (&frame_render_caps);    
    string capstring = "video/x-raw,format=RGBA,width="+ std::to_string(media_.width) +
            ",height=" + std::to_string(media_.height);
    GstCaps *caps = gst_caps_from_string(capstring.c_str());
    if (!gst_video_info_from_caps (&v_frame_video_info_, caps)) {
        Log::Warning("MediaPlayer %s Could not configure video frame info", std::to_string(id_).c_str());
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
        gst_app_sink_set_max_buffers( GST_APP_SINK(sink), 50);
        gst_app_sink_set_drop (GST_APP_SINK(sink), true);

#ifdef USE_GST_APPSINK_CALLBACKS
        // set the callbacks
        GstAppSinkCallbacks callbacks;
        callbacks.new_preroll = callback_new_preroll;
        if (media_.isimage) {
            callbacks.eos = NULL;
            callbacks.new_sample = NULL;
        }
        else {
            callbacks.eos = callback_end_of_stream;
            callbacks.new_sample = callback_new_sample;
        }
        gst_app_sink_set_callbacks (GST_APP_SINK(sink), &callbacks, this, NULL);
        gst_app_sink_set_emit_signals (GST_APP_SINK(sink), false);
#else
        // connect signals callbacks
        g_signal_connect(G_OBJECT(sink), "new-sample", G_CALLBACK (callback_new_sample), this);
        g_signal_connect(G_OBJECT(sink), "new-preroll", G_CALLBACK (callback_new_preroll), this);
        g_signal_connect(G_OBJECT(sink), "eos", G_CALLBACK (callback_end_of_stream), this);
        gst_app_sink_set_emit_signals (GST_APP_SINK(sink), true);
#endif
        // done with ref to sink
        gst_object_unref (sink);
    } 
    else {
        Log::Warning("MediaPlayer %s Could not configure  sink", std::to_string(id_).c_str());
        failed_ = true;
        return;
    }
    gst_caps_unref (caps);
    
    // capture bus signals to force a unique opengl context for all GST elements 
    //Rendering::LinkPipeline(GST_PIPELINE (pipeline));

    // set to desired state (PLAY or PAUSE)
    GstStateChangeReturn ret = gst_element_set_state (pipeline_, desired_state_);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        Log::Warning("MediaPlayer %s Could not open '%s'", std::to_string(id_).c_str(), uri_.c_str());
        failed_ = true;
        return;
    }

    // in case discoverer failed to get duration
    if (media_.timeline.end() == GST_CLOCK_TIME_NONE) {
        gint64 d = GST_CLOCK_TIME_NONE;
        if ( gst_element_query_duration(pipeline_, GST_FORMAT_TIME, &d) )
            media_.timeline.setEnd(d);
    }


    // all good
    Log::Info("MediaPlayer %d Opened '%s' (%s %d x %d)", id_,
              uri_.c_str(), media_.codec_name.c_str(), media_.width, media_.height);

    Log::Info("MediaPlayer %d Timeline [%ld %ld] %ld frames, %d gaps", id_,
              media_.timeline.begin(), media_.timeline.end(), media_.timeline.numFrames(), media_.timeline.numGaps());

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
    // not openned?
    if (!ready_) {
        // wait for loading to finish
        discoverer_.wait();
        // nothing else to change
        return;
    }

    // un-ready the media player
    ready_ = false;

    // no more need to reference converter
    if (converter_ != nullptr)
        gst_object_unref (converter_);

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
    return media_.width;
}

guint MediaPlayer::height() const
{
    return media_.height;
}

float MediaPlayer::aspectRatio() const
{
    return static_cast<float>(media_.par_width) / static_cast<float>(media_.height);
}

GstClockTime MediaPlayer::position()
{
    if (position_ == GST_CLOCK_TIME_NONE && pipeline_ != nullptr) {
        gint64 p = GST_CLOCK_TIME_NONE;
        if ( gst_element_query_position (pipeline_, GST_FORMAT_TIME, &p) )
            position_ = p;
    }

    return position_;
}

void MediaPlayer::enable(bool on)
{
    if ( !ready_ )
        return;

    if ( enabled_ != on ) {

        enabled_ = on;

        // default to pause
        GstState requested_state = GST_STATE_PAUSED;

        // unpause only if enabled
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
    return media_.isimage;
}

void MediaPlayer::play(bool on)
{
    // ignore if disabled, and cannot play an image
    if (!enabled_ || media_.isimage)
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
        if ( ( rate_ < 0.0 && position_ <= media_.timeline.next(0)  )
             || ( rate_ > 0.0 && position_ >= media_.timeline.previous(media_.timeline.last()) ) )
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
        Log::Info("MediaPlayer %s Stop [%ld]", gst_element_get_name(pipeline_), position());
#endif

    // reset time counter
    timecount_.reset();

}

bool MediaPlayer::isPlaying(bool testpipeline) const
{
    // image cannot play
    if (media_.isimage)
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
    if (!enabled_ || !media_.seekable)
        return;

    // playing forward, loop to begin
    if (rate_ > 0.0) {
        // begin is the end of a gab which includes the first PTS (if exists)
        // normal case, begin is zero
        execute_seek_command( media_.timeline.next(0) );
    }
    // playing backward, loop to endTimeInterval gap;
    else {
        // end is the start of a gab which includes the last PTS (if exists)
        // normal case, end is last frame
        execute_seek_command( media_.timeline.previous(media_.timeline.last()) );
    }
}


void MediaPlayer::step()
{
    // useful only when Paused
    if (!enabled_ || isPlaying())
        return;

    if ( ( rate_ < 0.0 && position_ <= media_.timeline.next(0)  )
         || ( rate_ > 0.0 && position_ >= media_.timeline.previous(media_.timeline.last()) ) )
        rewind();

    // step 
    gst_element_send_event (pipeline_, gst_event_new_step (GST_FORMAT_BUFFERS, 1, ABS(rate_), TRUE,  FALSE));
}

bool MediaPlayer::go_to(GstClockTime pos)
{
    bool ret = false;
    TimeInterval gap;
    if (pos != GST_CLOCK_TIME_NONE ) {

        GstClockTime jumpPts = pos;

        if (media_.timeline.gapAt(pos, gap)) {
            // if in a gap, find closest seek target
            if (gap.is_valid()) {
                // jump in one or the other direction
                jumpPts = (rate_>0.f) ? gap.end : gap.begin;
            }
        }

        if (ABS_DIFF (position_, jumpPts) > 2 * media_.timeline.step() ) {
            ret = true;
            seek( jumpPts );
        }
    }
    return ret;
}

void MediaPlayer::seek(GstClockTime pos)
{
    if (!enabled_ || !media_.seekable || seeking_)
        return;

    // apply seek
    GstClockTime target = CLAMP(pos, media_.timeline.begin(), media_.timeline.end());
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
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, media_.width, media_.height);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, media_.width, media_.height,
                    GL_RGBA, GL_UNSIGNED_BYTE, frame_[index].vframe.data[0]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    if (!media_.isimage) {

        // set pbo image size
        pbo_size_ = media_.height * media_.width * 4;

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
        Log::Info("MediaPlayer %s Using Pixel Buffer Object texturing.", std::to_string(id_).c_str());
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
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, media_.width, media_.height, GL_RGBA, GL_UNSIGNED_BYTE, 0);
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
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, media_.width, media_.height,
                            GL_RGBA, GL_UNSIGNED_BYTE, frame_[index].vframe.data[0]);
        }
    }
}

void MediaPlayer::update()
{
    // discard
    if (failed_)
        return;

    // not ready yet
    if (!ready_) {
        // try to get info from discoverer
        if (discoverer_.wait_for( std::chrono::milliseconds(4) ) == std::future_status::ready )
        {
            media_ = discoverer_.get();
            // if its ok, open the media
            if (media_.valid)
                execute_open();
        }
        // wait next frame to display
        return;
    }

    // prevent unnecessary updates: disabled or already filled image
    if (!enabled_ || (media_.isimage && textureindex_>0 ) )
        return;

    // local variables before trying to update
    guint read_index = 0;
    bool need_loop = false;

    // locked access to current index
    index_lock_.lock();
    // get the last frame filled from fill_frame()
    read_index = last_index_;
    // Do NOT miss and jump directly (after seek) to a pre-roll
    for (guint i = 0; i < N_VFRAME; ++i) {
        if (frame_[i].status == PREROLL) {
            read_index = i;
            break;
        }
    }
    // unlock access to index change
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

    // if already seeking (asynch)
    if (seeking_) {
        // request status update to pipeline (re-sync gst thread)
        GstState state;
        gst_element_get_state (pipeline_, &state, NULL, GST_CLOCK_TIME_NONE);
        // seek should be resolved next frame
        seeking_ = false;
        // do NOT do another seek yet
    }
    // otherwise check for need to seek (pipeline management)
    else {
        // manage timeline: test if position falls into a gap
        TimeInterval gap;
        if (position_ != GST_CLOCK_TIME_NONE && media_.timeline.gapAt(position_, gap)) {
            // if in a gap, seek to next section
            if (gap.is_valid()) {
                // jump in one or the other direction
                GstClockTime jumpPts = (rate_>0.f) ? gap.end : gap.begin;
                // seek to next valid time (if not beginnig or end of timeline)
                if (jumpPts > media_.timeline.first() && jumpPts < media_.timeline.last())
                    seek( jumpPts );
                // otherwise, we should loop
                else
                    need_loop = true;
            }

        }
        // manage loop mode
        if (need_loop) {
            execute_loop_command();
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
    if ( pipeline_ == nullptr || !media_.seekable )
        return;

    // seek position : default to target
    GstClockTime seek_pos = target;

    // no target given
    if (target == GST_CLOCK_TIME_NONE) 
        // create seek event with current position (rate changed ?)
        seek_pos = position_;
    // target is given but useless
    else if ( ABS_DIFF(target, position_) < media_.timeline.step()) {
        // ignore request
        return;
    }

    // seek with flush (always)
    int seek_flags = GST_SEEK_FLAG_FLUSH;
    // seek with trick mode if fast speed
    if ( ABS(rate_) > 1.0 )
        seek_flags |= GST_SEEK_FLAG_TRICKMODE;

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
        Log::Warning("MediaPlayer %s Seek failed", std::to_string(id_).c_str());
    else {
        seeking_ = true;
#ifdef MEDIA_PLAYER_DEBUG
        Log::Info("MediaPlayer %s Seek %ld %f", std::to_string(id_).c_str(), seek_pos, rate_);
#endif
    }

}

void MediaPlayer::setPlaySpeed(double s)
{
    if (media_.isimage)
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

Timeline *MediaPlayer::timeline()
{
    return &media_.timeline;
}

float MediaPlayer::currentTimelineFading()
{
    return media_.timeline.fadingAt(position_);
}

void MediaPlayer::setTimeline(Timeline tl)
{
    media_.timeline = tl;
}

//void MediaPlayer::toggleGapInTimeline(GstClockTime from, GstClockTime to)
//{
//    return media_.timeline.toggleGaps(from, to);
//}

std::string MediaPlayer::codec() const
{
    return media_.codec_name;
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
    return media_.framerate;
}

double MediaPlayer::updateFrameRate() const
{
    return timecount_.frameRate();
}


// CALLBACKS

bool MediaPlayer::fill_frame(GstBuffer *buf, FrameStatus status)
{
    // Do NOT overwrite an unread EOS
    if ( frame_[write_index_].status == EOS )
        write_index_ = (write_index_ + 1) % N_VFRAME;

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
            Log::Info("MediaPlayer %s Failed to map the video buffer", std::to_string(id_).c_str());
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
            if (media_.timeline.begin() == GST_CLOCK_TIME_NONE) {
                media_.timeline.setFirst(buf->pts);
            }
        }
        // full but invalid frame : will be deleted next iteration
        // (should never happen)
        else
            frame_[write_index_].status = INVALID;
    }
    // else; null buffer for EOS: give a position
    else {
        frame_[write_index_].status = EOS;
        frame_[write_index_].position = rate_ > 0.0 ? media_.timeline.end() : media_.timeline.begin();
    }

    // unlock access to frame
    frame_[write_index_].access.unlock();

    // lock access to change current index (very quick)
    index_lock_.lock();
    // indicate update() that this is the last frame filled (and unlocked)
    last_index_ = write_index_;
    // unlock access to index change
    index_lock_.unlock();

    // for writing, we will access the next in stack
    write_index_ = (write_index_ + 1) % N_VFRAME;

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
            else if (m->playSpeed() < 0.f && !(buf->pts > 0) ) {
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
            else if (m->playSpeed() < 0.f && !(buf->pts > 0) ) {
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

