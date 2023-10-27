/*
 * This file is part of vimix - video live mixer
 *
 * **Copyright** (C) 2019-2023 Bruno Herbelin <bruno.herbelin@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
**/

#include <gst/gst.h>

//  Desktop OpenGL function loader
#include <glad/glad.h>

#include "defines.h"
#include "Log.h"
#include "Resource.h"
#include "Visitor.h"
#include "SystemToolkit.h"
#include "BaseToolkit.h"
#include "GstToolkit.h"
#include "Metronome.h"

#include "MediaPlayer.h"

#ifndef NDEBUG
#define MEDIA_PLAYER_DEBUG
#endif

#define DISCOVER_TIMOUT 15

#if GST_VERSION_MAJOR > 0 && GST_VERSION_MINOR > 18
#define USE_GST_PLAYBIN
#endif


std::list<MediaPlayer*> MediaPlayer::registered_;

MediaPlayer::MediaPlayer()
{
    // create unique id
    id_ = BaseToolkit::uniqueId();

    uri_ = "undefined";
    pipeline_ = nullptr;
    opened_ = false;
    enabled_ = true;
    desired_state_ = GST_STATE_PAUSED;

    failed_ = false;
    pending_ = false;
    metro_sync_ = Metronome::SYNC_NONE;
    force_update_ = false;
    seeking_ = false;
    rewind_on_disable_ = false;
    force_software_decoding_ = false;
    rate_ = 1.0;
    rate_change_ = RATE_CHANGE_NONE;
    decoder_name_ = "";
    video_filter_ = "";
    video_filter_available_ = true;
    position_ = GST_CLOCK_TIME_NONE;
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

    // cleanup opengl texture
    if (textureindex_)
        glDeleteTextures(1, &textureindex_);

    // cleanup picture buffer
    if (pbo_[0])
        glDeleteBuffers(2, pbo_);
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

#define LIMIT_DISCOVERER

MediaInfo MediaPlayer::UriDiscoverer(const std::string &uri)
{
#ifdef MEDIA_PLAYER_DEBUG
    Log::Info("Checking uri '%s'", uri.c_str());
#endif

#ifdef LIMIT_DISCOVERER
    // Limiting the number of discoverer thread to TWO in parallel
    // Otherwise, a large number of discoverers are executed (when loading a file)
    // leading to a peak of memory and CPU usage : this causes slow down of FPS
    // and a hungry consumption of RAM.
    static std::mutex mtx_primary;
    static std::mutex mtx_secondary;
    bool use_primary = true;
    if ( !mtx_primary.try_lock() ) { // non-blocking
        use_primary = false;
        mtx_secondary.lock(); // blocking
    }
#endif

    // returned value
    MediaInfo video_stream_info;

    // if uri is valid
    if ( SystemToolkit::file_exists( gst_uri_get_location(uri.c_str()) ) ) {

        GError *err = NULL;
        GstDiscoverer *discoverer = gst_discoverer_new (DISCOVER_TIMOUT * GST_SECOND, &err);

        /* Instantiate the Discoverer */
        if (!discoverer) {
            Log::Warning("MediaPlayer Error creating discoverer instance: %s\n", err->message);
        }
        else {
            GstDiscovererInfo *info = NULL;
            info = gst_discoverer_discover_uri (discoverer, uri.c_str(), &err);
            GstDiscovererResult result = gst_discoverer_info_get_result (info);
            switch (result) {
            case GST_DISCOVERER_URI_INVALID:
                video_stream_info.log = "Invalid URI";
                break;
            case GST_DISCOVERER_ERROR:
                video_stream_info.log = std::string( "Warning: " ) + err->message;
                result = GST_DISCOVERER_OK; // try to read the file anyways, discoverer can report errors but still read the file
                break;
            case GST_DISCOVERER_TIMEOUT:
                video_stream_info.log = "Timeout loading";
                break;
            case GST_DISCOVERER_BUSY:
                video_stream_info.log = "Busy";
                break;
            case GST_DISCOVERER_MISSING_PLUGINS:
            {
                const GstStructure *s = gst_discoverer_info_get_misc (info);
                gchar *str = gst_structure_to_string (s);
                video_stream_info.log = std::string( "Warning: " ) + std::string(str);
                g_free (str);
                result = GST_DISCOVERER_OK; // try to read the file anyways, discoverer can report errors but still read the file
            }
                break;
            default:
                break;
            }

            if ( result == GST_DISCOVERER_OK ) {
                // get videos in information found
                GList *streams = gst_discoverer_info_get_video_streams(info);
                if ( g_list_length(streams) > 0) {
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
                            // if its a video, set duration, framerate, etc.
                            if ( !video_stream_info.isimage ) {
                                video_stream_info.end = gst_discoverer_info_get_duration (info) ;
                                video_stream_info.seekable = gst_discoverer_info_get_seekable (info);
                                video_stream_info.framerate_n = gst_discoverer_video_info_get_framerate_num(vinfo);
                                video_stream_info.framerate_d = gst_discoverer_video_info_get_framerate_denom(vinfo);
                                if (video_stream_info.framerate_n == 0 || video_stream_info.framerate_d == 0) {
                                    Log::Info("'%s': No framerate indicated in the file; using default 30fps", uri.c_str());
                                    video_stream_info.framerate_n = 30;
                                    video_stream_info.framerate_d = 1;
                                }
                                video_stream_info.dt = ( (GST_SECOND * static_cast<guint64>(video_stream_info.framerate_d)) / (static_cast<guint64>(video_stream_info.framerate_n)) );
                                // confirm (or infirm) that its not a single frame
                                if ( video_stream_info.end < video_stream_info.dt * 2)
                                    video_stream_info.isimage = true;
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
                                    video_stream_info.codec_name += ", " + std::string(container);
                                if (container)
                                    g_free(container);
                            }
                            // exit loop
                            // inform that it succeeded
                            video_stream_info.valid = true;
                        }
                    }

                    if (!video_stream_info.valid)
                        video_stream_info.log = "Invalid video stream";
                }
                else
                    video_stream_info.log = "No video stream";

                gst_discoverer_stream_info_list_free(streams);
            }

            if (info)
                gst_discoverer_info_unref (info);

            g_object_unref( discoverer );
        }

        g_clear_error (&err);

    }
    else {
        video_stream_info.log = "No such file";
    }

#ifdef LIMIT_DISCOVERER
    if (use_primary)
        mtx_primary.unlock();
    else
        mtx_secondary.unlock();
#endif
    // return the info
    return video_stream_info;
}

void MediaPlayer::open (const std::string & filename, const std::string &uri)
{
    // set path
    filename_ = BaseToolkit::transliterate( filename );

    // set uri to open
    if (uri.empty())
        uri_ = GstToolkit::filename_to_uri( filename );
    else
        uri_ = uri;

    if (uri_.empty())
        failed_ = true;

    // close before re-openning
    if (isOpen())
        close();

    // start URI discovering thread:
    discoverer_ = std::async( MediaPlayer::UriDiscoverer, uri_);
    // wait for discoverer to finish in the future (test in update)

//    // debug without thread
//    media_ = MediaPlayer::UriDiscoverer(uri_);
//    if (media_.valid) {
//        timeline_.setEnd( media_.end );
//        timeline_.setStep( media_.dt );
//        execute_open();
//    }

}


void MediaPlayer::reopen()
{
    // re-openning is meaningfull only if it was already open
    if (pipeline_ != nullptr && opened_) {
        // reload : terminate pipeline and re-create it
        close();
        execute_open();
    }
}

#ifdef USE_GST_PLAYBIN

// GstPlayFlags flags from playbin. It is the policy of GStreamer to
// not publicly expose element-specific enums. That's why this
// GstPlayFlags enum has been copied here.
// https://gstreamer.freedesktop.org/documentation/playback/playsink.html?gi-language=c#GstPlayFlags
typedef enum {
    GST_PLAY_FLAG_VIDEO         = 0x00000001,
    GST_PLAY_FLAG_AUDIO         = 0x00000002,
    GST_PLAY_FLAG_TEXT          = 0x00000004,
    GST_PLAY_FLAG_VIS           = 0x00000008,
    GST_PLAY_FLAG_SOFT_VOLUME   = 0x00000010,
    GST_PLAY_FLAG_NATIVE_AUDIO  = 0x00000020,
    GST_PLAY_FLAG_NATIVE_VIDEO  = 0x00000040,
    GST_PLAY_FLAG_DOWNLOAD      = 0x00000080,
    GST_PLAY_FLAG_BUFFERING     = 0x00000100,
    GST_PLAY_FLAG_DEINTERLACE   = 0x00000200,
    GST_PLAY_FLAG_SW_COLOR      = 0x00000400,
    GST_PLAY_FLAG_FORCE_FILTER  = 0x00000800,
    GST_PLAY_FLAG_SW_DECODER    = 0x00001000
} GstPlayFlags;


//
// Setup a media player using gstreamer playbin
//
void MediaPlayer::execute_open()
{
    // create playbin
    pipeline_ = gst_element_factory_make ("playbin", std::to_string(id_).c_str());

    // set uri of file to open
    g_object_set ( G_OBJECT (pipeline_),  "uri", uri_.c_str(),  NULL);

    // Get and modify playbin flags
    gint flags;
    // ENABLE ONLY VIDEO, NOT AUDIO AND TEXT SUBTITLES
    flags = GST_PLAY_FLAG_VIDEO;
    // ENABLE DEINTERLACING
    if (media_.interlaced)
        flags |= GST_PLAY_FLAG_DEINTERLACE;
    // ENABLE/DISABLE FORCE SOFTWARE DECODING
    if (force_software_decoding_ || media_.isimage) {
        flags |= GST_PLAY_FLAG_SW_COLOR;
        flags |= GST_PLAY_FLAG_SW_DECODER;
    }
    g_object_set( G_OBJECT (pipeline_), "flags", flags, NULL);

    // hack to compensate for lack of PTS in gif animations
    if (media_.codec_name.compare("image/gst-libav-gif") == 0) {
        video_filter_ = "videorate rate=1.0";
        video_filter_available_ = false;
    }

    // hack to simulate a playable and seekable stream with an image
    if (media_.isimage && !singleFrame()) {
        media_.seekable = true;
        video_filter_ = "imagefreeze";
        video_filter_available_ = false;
    }

    // Add a filter to playbin if provided
    if ( !video_filter_.empty()) {
        // try to create the pipeline element given
        GError *error = NULL;
        GstElement *effect = gst_parse_launch( video_filter_.c_str(), &error);
        if (effect == NULL || error != NULL) {
            if (error != NULL)
                Log::Warning("MediaPlayer %s Error constructing filter %s:\n%s", std::to_string(id_).c_str(), video_filter_.c_str(), error->message);
            g_clear_error (&error);
            if (effect)
                gst_object_unref(effect);
        }
        else {
            g_object_set( G_OBJECT (pipeline_), "video-filter", effect, nullptr);
            Log::Info("MediaPlayer %s Added filter '%s'", std::to_string(id_).c_str(), video_filter_.c_str());

        }
    }

    // setup appsink
    GstElement *sink = gst_element_factory_make ("appsink", "appsink");
    if (!sink) {
        Log::Warning("MediaPlayer %s Could not configure  sink", std::to_string(id_).c_str());
        failed_ = true;
        return;
    }

    // instruct the sink to send samples synched in time
    gst_base_sink_set_sync (GST_BASE_SINK(sink), true);

    // instruct sink to use the required caps
    std::string capstring = "video/x-raw,format=RGBA,width="+ std::to_string(media_.width) +
            ",height=" + std::to_string(media_.height);
    GstCaps *caps = gst_caps_from_string(capstring.c_str());
    if (!gst_video_info_from_caps (&v_frame_video_info_, caps)) {
        Log::Warning("MediaPlayer %s Could not configure video frame info", std::to_string(id_).c_str());
        failed_ = true;
        return;
    }
    gst_app_sink_set_caps (GST_APP_SINK(sink), caps);

    // Instruct appsink to drop old buffers when the maximum amount of queued buffers is reached.
    gst_app_sink_set_max_buffers( GST_APP_SINK(sink), 5);
    gst_app_sink_set_drop (GST_APP_SINK(sink), true);

    // set the callbacks
    GstAppSinkCallbacks callbacks;
#if GST_VERSION_MINOR > 18 && GST_VERSION_MAJOR > 0
    callbacks.new_event = NULL;
#endif
    callbacks.new_preroll = callback_new_preroll;
    if (singleFrame()) {
        callbacks.eos = NULL;
        callbacks.new_sample = NULL;
    }
    else {
        callbacks.eos = callback_end_of_stream;
        callbacks.new_sample = callback_new_sample;
    }
    gst_app_sink_set_callbacks (GST_APP_SINK(sink), &callbacks, this, NULL);
    gst_app_sink_set_emit_signals (GST_APP_SINK(sink), false);

    // set playbin sink
    g_object_set ( G_OBJECT (pipeline_), "video-sink", sink, NULL);

    // done with ref to sink
    gst_object_unref (sink);
    gst_caps_unref (caps);

#ifdef USE_GST_OPENGL_SYNC_HANDLER
    // capture bus signals to force a unique opengl context for all GST elements
    Rendering::LinkPipeline(GST_PIPELINE (pipeline_));
#endif

    // set to desired state (PLAY or PAUSE)
    GstStateChangeReturn ret = gst_element_set_state (pipeline_, desired_state_);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        Log::Warning("MediaPlayer %s Could not open '%s'", std::to_string(id_).c_str(), uri_.c_str());
        failed_ = true;
        return;
    }

    // in case discoverer failed to get duration of a video
    if (!media_.isimage && timeline_.end() == GST_CLOCK_TIME_NONE) {
        gint64 d = GST_CLOCK_TIME_NONE;
        if ( gst_element_query_duration(pipeline_, GST_FORMAT_TIME, &d) )
            timeline_.setEnd(d);
    }

    // all good
    Log::Info("MediaPlayer %s Opened '%s' (%s %d x %d)", std::to_string(id_).c_str(),
              SystemToolkit::filename(uri_).c_str(), media_.codec_name.c_str(), media_.width, media_.height);

    if (!singleFrame())
        Log::Info("MediaPlayer %s Timeline [%ld %ld] %ld frames, %d gaps", std::to_string(id_).c_str(),
                  timeline_.begin(), timeline_.end(), timeline_.numFrames(), timeline_.numGaps());

    opened_ = true;

    // register media player
    MediaPlayer::registered_.push_back(this);
}

#else

//
// Setup a media player using gstreamer custom pipeline
//
void MediaPlayer::execute_open()
{
    // Create gstreamer pipeline :
    //         "uridecodebin uri=file:///path_to_file/filename.mp4 ! videoconvert ! appsink "
    // equivalent to command line
    //         "gst-launch-1.0 uridecodebin uri=file:///path_to_file/filename.mp4 ! videoconvert ! ximagesink"
    std::string description = "uridecodebin name=decoder uri=" + uri_ + " ! ";
    // NB: queue adds some control over the buffer, thereby limiting the frame delay. zero size means no buffering

    // video deinterlacing method (if media is interlaced)
    //      tomsmocomp (0) – Motion Adaptive: Motion Search
    //      greedyh (1) – Motion Adaptive: Advanced Detection
    //      greedyl (2) – Motion Adaptive: Simple Detection
    //      vfir (3) – Blur Vertical
    //      linear (4) – Linear
    //      scalerbob (6) – Double lines
    if (media_.interlaced)
        description += "deinterlace method=2 ! ";

    // Add a filter to playbin if provided
    if ( !video_filter_.empty()) {
        description += "videoconvert chroma-resampler=1 dither=0 ! "; // fast
        description += video_filter_ + " ! ";
    }

    // video convertion algorithm (should only do colorspace conversion, no scaling)
    // chroma-resampler:
    //      Duplicates the samples when upsampling and drops when downsampling 0
    //      Uses linear interpolation 1 (default)
    //      Uses cubic interpolation 2
    //      Uses sinc interpolation 3
    //  dither:
    //      no dithering 0
    //      propagate rounding errors downwards 1
    //      Dither with floyd-steinberg error diffusion 2
    //      Dither with Sierra Lite error diffusion 3
    //      ordered dither using a bayer pattern 4 (default)
    description += "videoconvert chroma-resampler=1 dither=0 ! "; // fast

    // hack to compensate for lack of PTS in gif animations
    if (media_.codec_name.compare("image/gst-libav-gif") == 0){
        description += "videorate ! video/x-raw,framerate=";
        description += std::to_string(media_.framerate_n) + "/";
        description += std::to_string(media_.framerate_d) + " ! ";
    }

    // hack to simulate a playable and seekable stream with an image
    if (media_.isimage && !singleFrame()) {
        media_.seekable = true;
        description += "imagefreeze ! ";
    }

    // set app sink
    description += "queue ! appsink name=sink";

    // parse pipeline descriptor
    GError *error = NULL;
    pipeline_ = gst_parse_launch (description.c_str(), &error);
    if (error != NULL) {
        Log::Warning("MediaPlayer %s Could not construct pipeline %s:\n%s", std::to_string(id_).c_str(), description.c_str(), error->message);
        g_clear_error (&error);
        failed_ = true;
        return;
    }
#ifdef MEDIA_PLAYER_DEBUG
        Log::Info("MediaPlayer %s Pipeline [%s]", std::to_string(id_).c_str(), description.c_str());
#endif

    // setup pipeline
    g_object_set(G_OBJECT(pipeline_), "name", std::to_string(id_).c_str(), NULL);
    gst_pipeline_set_auto_flush_bus( GST_PIPELINE(pipeline_), true);

    // setup software decode
    if (force_software_decoding_ || media_.isimage) {
        g_object_set (G_OBJECT (gst_bin_get_by_name (GST_BIN (pipeline_), "decoder")), "force-sw-decoders", true,  NULL);
    }

    // setup appsink
    GstElement *sink = gst_bin_get_by_name (GST_BIN (pipeline_), "sink");
    if (!sink) {
        Log::Warning("MediaPlayer %s Could not configure  sink", std::to_string(id_).c_str());
        failed_ = true;
        return;
    }

    // instruct the sink to send samples synched in time
    gst_base_sink_set_sync (GST_BASE_SINK(sink), true);

    // instruct sink to use the required caps
    std::string capstring = "video/x-raw,format=RGBA,width="+ std::to_string(media_.width) +
            ",height=" + std::to_string(media_.height);
    GstCaps *caps = gst_caps_from_string(capstring.c_str());
    if (!gst_video_info_from_caps (&v_frame_video_info_, caps)) {
        Log::Warning("MediaPlayer %s Could not configure video frame info", std::to_string(id_).c_str());
        failed_ = true;
        return;
    }
    gst_app_sink_set_caps (GST_APP_SINK(sink), caps);

    // Instruct appsink to drop old buffers when the maximum amount of queued buffers is reached.
    gst_app_sink_set_max_buffers( GST_APP_SINK(sink), 5);
    gst_app_sink_set_drop (GST_APP_SINK(sink), true);

    // set the callbacks
    GstAppSinkCallbacks callbacks;
#if GST_VERSION_MINOR > 18 && GST_VERSION_MAJOR > 0
    callbacks.new_event = NULL;
#endif
    callbacks.new_preroll = callback_new_preroll;
    if (singleFrame()) {
        callbacks.eos = NULL;
        callbacks.new_sample = NULL;
    }
    else {
        callbacks.eos = callback_end_of_stream;
        callbacks.new_sample = callback_new_sample;
    }
    gst_app_sink_set_callbacks (GST_APP_SINK(sink), &callbacks, this, NULL);
    gst_app_sink_set_emit_signals (GST_APP_SINK(sink), false);

    // done with ref to sink
    gst_object_unref (sink);
    gst_caps_unref (caps);

#ifdef USE_GST_OPENGL_SYNC_HANDLER
    // capture bus signals to force a unique opengl context for all GST elements
    Rendering::LinkPipeline(GST_PIPELINE (pipeline_));
#endif

    // set to desired state (PLAY or PAUSE)
    GstStateChangeReturn ret = gst_element_set_state (pipeline_, desired_state_);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        Log::Warning("MediaPlayer %s Could not open '%s'", std::to_string(id_).c_str(), uri_.c_str());
        failed_ = true;
        return;
    }

    // in case discoverer failed to get duration of a video
    if (!media_.isimage && timeline_.end() == GST_CLOCK_TIME_NONE) {
        gint64 d = GST_CLOCK_TIME_NONE;
        if ( gst_element_query_duration(pipeline_, GST_FORMAT_TIME, &d) )
            timeline_.setEnd(d);
    }

    // all good
    Log::Info("MediaPlayer %s Opened '%s' (%s %d x %d)", std::to_string(id_).c_str(),
              SystemToolkit::filename(uri_).c_str(), media_.codec_name.c_str(), media_.width, media_.height);

    if (!singleFrame())
        Log::Info("MediaPlayer %s Timeline [%ld %ld] %ld frames, %d gaps", std::to_string(id_).c_str(),
                  timeline_.begin(), timeline_.end(), timeline_.numFrames(), timeline_.numGaps());

    opened_ = true;

    // register media player
    MediaPlayer::registered_.push_back(this);
}

#endif


void MediaPlayer::setVideoEffect(const std::string &pipeline_element)
{
    if ( !pipeline_element.empty() ) {
        GError *error = NULL;
        GstElement *effect = gst_parse_launch( pipeline_element.c_str(), &error);
        if (effect == NULL || error != NULL) {
            Log::Warning("MediaPlayer %s Error constructing %s:\n%s", std::to_string(id_).c_str(), video_filter_.c_str(), error->message);
            g_clear_error (&error);
            if (effect)
                gst_object_unref(effect);
            return ;
        }
        // cleanup
        g_clear_error (&error);
        if (effect)
            gst_object_unref(effect);
    }

    video_filter_ = pipeline_element;
    reopen();
}

bool MediaPlayer::isOpen() const
{
    return opened_;
}

bool MediaPlayer::failed() const
{
    return failed_;
}

void MediaPlayer::Frame::unmap()
{
    if ( full )
        gst_video_frame_unmap(&vframe);
    full = false;
}


void delayed_terminate( GstElement *p )
{
    GstObject *__pipeline = (GstObject *) gst_object_ref(p);

    // end pipeline
    gst_element_set_state ( GST_ELEMENT(__pipeline), GST_STATE_NULL);

    // unref to free pipeline
    gst_object_unref ( __pipeline );
}


void MediaPlayer::close()
{
    // not opened?
    if (!opened_) {
        // wait for loading to finish
        if (discoverer_.valid())
            if ( discoverer_.wait_for(std::chrono::seconds(DISCOVER_TIMOUT)) == std::future_status::timeout )
                 failed_ = true;
        // nothing else to change
        return;
    }

    // un-ready the media player
    opened_ = false;
    failed_ = false;
    pending_ = false;
    seeking_ = false;
    force_update_ = false;
    rate_ = 1.0;
    rate_change_ = RATE_CHANGE_NONE;

    // clean up GST
    if (pipeline_ != nullptr) {

        // end pipeline asynchronously // TODO more stress test?
        std::thread(delayed_terminate, pipeline_).detach();

        pipeline_ = nullptr;
    }

    // cleanup eventual remaining frame memory
    for(guint i = 0; i < N_VFRAME; i++) {
        frame_[i].access.lock();
        frame_[i].unmap();
        frame_[i].access.unlock();
    }
    write_index_ = 0;
    last_index_ = 0;


#ifdef MEDIA_PLAYER_DEBUG
    Log::Info("MediaPlayer %s closed", std::to_string(id_).c_str());
#endif

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
    if ( !opened_ || pipeline_ == nullptr)
        return;

    if ( enabled_ != on ) {

        // option to automatically rewind each time the player is disabled
        if (!on && rewind_on_disable_ && desired_state_ == GST_STATE_PLAYING)
            rewind(true);

        // apply change
        enabled_ = on;

        // default to pause
        GstState requested_state = GST_STATE_PAUSED;

        // unpause only if enabled
        if (enabled_)
            requested_state = desired_state_;

        //  apply state change
        GstStateChangeReturn ret = gst_element_set_state (pipeline_, requested_state);
        if (ret == GST_STATE_CHANGE_FAILURE) {
            Log::Warning("MediaPlayer %s Failed to enable", std::to_string(id_).c_str());
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

bool MediaPlayer::singleFrame() const
{
    if (media_.isimage)
        return timeline_.end() == GST_CLOCK_TIME_NONE;
    return false;
}

std::string MediaPlayer::decoderName()
{
    if (decoder_name_.empty()) {
        // try to know if it is a hardware decoder
        if (pipeline_)
            decoder_name_ = GstToolkit::used_gpu_decoding_plugins(pipeline_);
        // nope, then it is a sofware decoder
        if (decoder_name_.empty())
            decoder_name_ = "software";
    }

    return decoder_name_;
}

bool MediaPlayer::softwareDecodingForced()
{
    return force_software_decoding_;
}

void MediaPlayer::setSoftwareDecodingForced(bool on)
{
    bool need_reload = force_software_decoding_ != on;

    // set parameter
    force_software_decoding_ = on;
    decoder_name_ = "";

    // changing state requires reload
    if (need_reload)
        reopen();
}

void MediaPlayer::execute_play_command(bool on)
{
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
        if (rate_ > 0.0 && position_ >= timeline_.previous(timeline_.last()))
            execute_seek_command(timeline_.next(0));
        else if ( rate_ < 0.0 && position_ <= timeline_.next(0)  )
            execute_seek_command(timeline_.previous(timeline_.last()));
    }

    // all ready, apply state change immediately
    GstStateChangeReturn ret = gst_element_set_state (pipeline_, desired_state_);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        Log::Warning("MediaPlayer %s Failed to play", std::to_string(id_).c_str());
        failed_ = true;
    }
#ifdef MEDIA_PLAYER_DEBUG
    else if (on)
        Log::Info("MediaPlayer %s Start", std::to_string(id_).c_str());
    else
        Log::Info("MediaPlayer %s Stop [%ld]", std::to_string(id_).c_str(), position());
#endif

}

void MediaPlayer::play(bool on)
{
    // ignore if disabled, and cannot play a single frame
    if (!enabled_ || pending_ || singleFrame())
        return;

    // Metronome
    if (metro_sync_ > Metronome::SYNC_NONE) {
        // busy with this delayed action
        pending_ = true;
        // delayed execution function
         std::function<void()> playlater = std::bind([](MediaPlayer *p, bool o) {
                 p->execute_play_command(o); p->pending_=false; }, this, on);
        // Execute: sync to Metronome
        if (metro_sync_ > Metronome::SYNC_BEAT)
            Metronome::manager().executeAtPhase( playlater );
        else
            Metronome::manager().executeAtBeat( playlater );
    }
    else
        // execute immediately
        execute_play_command( on );
}

bool MediaPlayer::isPlaying(bool testpipeline) const
{
    // image cannot play
    if (singleFrame())
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

void MediaPlayer::rewind(bool force)
{
    if (!enabled_ || !media_.seekable || pending_)
        return;

    // playing forward, loop to begin;
    //          begin is the end of a gab which includes the first PTS (if exists)
    //          normal case, begin is zero
    // playing backward, loop to endTimeInterval gap;
    //          end is the start of a gab which includes the last PTS (if exists)
    //          normal case, end is last frame
    GstClockTime target = (rate_ > 0.0) ? timeline_.next(0) : timeline_.previous(timeline_.last());

    // Metronome
    if (metro_sync_) {
        // busy with this delayed action
        pending_ = true;
        // delayed execution function
         std::function<void()> rewindlater = std::bind([](MediaPlayer *p, GstClockTime t, bool f) {
                 p->execute_seek_command( t, f ); p->pending_=false; }, this, target, force);
        // Execute: sync to Metronome
        if (metro_sync_ > Metronome::SYNC_BEAT)
            Metronome::manager().executeAtPhase( rewindlater );
        else
            Metronome::manager().executeAtBeat( rewindlater );
    }
    else
        // execute immediately
        execute_seek_command( target, force );
}


void MediaPlayer::step(uint milisecond)
{
    // useful only when Paused
    if (!enabled_ || isPlaying() || pending_)
        return;

    if ( ( rate_ < 0.0 && position_ <= timeline_.next(0)  )
         || ( rate_ > 0.0 && position_ >= timeline_.previous(timeline_.last()) ) )
        rewind();
    else {
        // step event
        if (milisecond < media_.dt)
            milisecond = media_.dt;
        GstEvent *stepevent = gst_event_new_step (GST_FORMAT_TIME, milisecond, ABS(rate_), TRUE,  FALSE);

        // Metronome
        if (metro_sync_) {
            // busy with this delayed action
            pending_ = true;
            // delayed execution function
             std::function<void()> steplater = std::bind([](MediaPlayer *p, GstEvent *e) {
                     gst_element_send_event(p->pipeline_, e); p->pending_=false; }, this, stepevent) ;
            // Execute: sync to Metronome
            if (metro_sync_ > Metronome::SYNC_BEAT)
                Metronome::manager().executeAtPhase( steplater );
            else
                Metronome::manager().executeAtBeat( steplater );
        }
        else
            // execute immediately
            gst_element_send_event (pipeline_, stepevent);

    }
}

bool MediaPlayer::go_to(GstClockTime pos)
{
    bool ret = false;
    TimeInterval gap;
    if (pos != GST_CLOCK_TIME_NONE ) {

        GstClockTime jumpPts = pos;

        if (timeline_.getGapAt(pos, gap)) {
            // if in a gap, find closest seek target
            if (gap.is_valid()) {
                // jump in one or the other direction
                jumpPts = (rate_>0.f) ? gap.end : gap.begin;
            }
        }

        if (ABS_DIFF (position_, jumpPts) > 2 * timeline_.step() ) {
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
    GstClockTime target = CLAMP(pos, timeline_.begin(), timeline_.end());
    execute_seek_command(target);

}

void MediaPlayer::jump(uint milisecond)
{
    if (!enabled_ || !isPlaying())
        return;

    gst_element_send_event (pipeline_, gst_event_new_step (GST_FORMAT_TIME,
                                                           CLAMP(milisecond, 1, 1000) * GST_MSECOND,
                                                           ABS(rate_),
                                                           TRUE,  FALSE));

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
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    if ( !singleFrame() ) {

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
                glDeleteBuffers(2, pbo_);
                pbo_[0] = pbo_[1] = 0;
                pbo_size_ = 0;
                break;
            }

        }

        // should be good to go, wrap it up
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        pbo_index_ = 0;
        pbo_next_index_ = 1;

        // initialize decoderName once (forced update)
        decoder_name_ = "";
        Log::Info("MediaPlayer %s Uses %s decoding and OpenGL PBO texturing.", std::to_string(id_).c_str(), decoderName().c_str());
    }

    glBindTexture(GL_TEXTURE_2D, 0);
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
#ifdef USE_GL_BUFFER_SUBDATA
            glBufferSubData(GL_PIXEL_UNPACK_BUFFER, 0, pbo_size_, frame_[index].vframe.data[0]);
#else
            // update data directly on the mapped buffer
            // NB : equivalent but faster than glBufferSubData (memmove instead of memcpy ?)
            // See http://www.songho.ca/opengl/gl_pbo.html#map for more details
            glBufferData(GL_PIXEL_UNPACK_BUFFER, pbo_size_, 0, GL_STREAM_DRAW);
            // map the buffer object into client's memory
            GLubyte* ptr = (GLubyte*) glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
            if (ptr) {
                memmove(ptr, frame_[index].vframe.data[0], pbo_size_);
                // release pointer to mapping buffer
                glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
            }
#endif
            // done with PBO
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        }
        else {
            // without PBO, use standard opengl (slower)
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, media_.width, media_.height,
                            GL_RGBA, GL_UNSIGNED_BYTE, frame_[index].vframe.data[0]);
        }
        glBindTexture(GL_TEXTURE_2D, 0);
    }
}

void MediaPlayer::update()
{
    // discard
    if (failed_)
        return;

    // not ready yet
    if (!opened_) {
        if (discoverer_.valid()) {
            // try to get info from discoverer
            if (discoverer_.wait_for( std::chrono::milliseconds(4) ) == std::future_status::ready )
            {
                media_ = discoverer_.get();
                // if its ok, open the media
                if (media_.valid) {
                    if (!media_.log.empty())
                        Log::Info("'%s' : %s", uri().c_str(), media_.log.c_str());
                    if (!media_.isimage)
                        timeline_.setTiming( TimeInterval(0, media_.end), media_.dt);
                    execute_open();
                }
                else {
                    Log::Warning("'%s' : %s", uri().c_str(), media_.log.c_str());
                    Log::Warning("MediaPlayer %s Loading failed.", std::to_string(id_).c_str());
                    failed_ = true;
                }
            }
        }
        // wait next frame to display
        return;
    }

    // prevent unnecessary updates: disabled or already filled image
    if ( (!enabled_ && !force_update_) || (singleFrame() && textureindex_>0 ) )
        return;

    // local variables before trying to update
    guint read_index = 0;
    bool need_loop = false;

    // locked access to current index
    index_lock_.lock();
    // get the last frame filled from fill_frame()
    read_index = last_index_;
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
            if ( (frame_[read_index].status == PREROLL || seeking_ ) && pbo_size_ > 0)
                fill_texture(read_index);

            // free frame
            frame_[read_index].unmap();
        }

        // we just displayed a vframe : set position time to frame PTS
        position_ = frame_[read_index].position;

        // avoid reading it again
        frame_[read_index].status = INVALID;
    }

    // unkock frame after reading it
    frame_[read_index].access.unlock();

    // if already seeking (asynch)
    if (seeking_) {
        // request status update to pipeline (re-sync gst thread)
        gst_element_get_state (pipeline_, NULL, NULL, GST_CLOCK_TIME_NONE);
        // seek should be resolved next frame
        seeking_ = false;
        // do NOT do another seek yet
    }
    // otherwise check for need to seek (pipeline management)
    else if (position_ != GST_CLOCK_TIME_NONE) {
        // manage timeline:
        TimeInterval gap;
        // ensure we remain within the begin to end range
        if (position_ > timeline_.last() || position_ < timeline_.first()) {
            need_loop = true;
        }
        // test if position falls into a gap
        else if (timeline_.getGapAt(position_, gap)) {
            // if in a gap, seek to next section
            if (gap.is_valid()) {
                // jump in one or the other direction
                GstClockTime jumpPts = timeline_.step(); // round jump time to frame pts
                if ( rate_ > 0.f )
                    jumpPts *= ( gap.end / timeline_.step() ) + 1; // FWD: go to end of gap
                else
                    jumpPts *= ( gap.begin / timeline_.step() );   // BWD: go to begin of gap
                // (if not beginnig or end of timeline)
                if (jumpPts > timeline_.first() && jumpPts < timeline_.last())
                    // seek to jump PTS time
                    seek( jumpPts );
                // otherwise, we should loop
                else
                    need_loop = true;
            }
        }
    }

    // manage loop mode
    if (need_loop)
        execute_loop_command();

    force_update_ = false;
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
        if (desired_state_ == GST_STATE_PLAYING) // avoid repeated call
            play(false);
    }
}

void MediaPlayer::execute_seek_command(GstClockTime target, bool force)
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
    else if ( ABS_DIFF(target, position_) < timeline_.step()) {
        // ignore request
        return;
    }

    // seek with flush (always)
    int seek_flags = GST_SEEK_FLAG_FLUSH;

    // seek with trick mode if fast speed
    if ( ABS(rate_) > 1.5 )
        seek_flags |= GST_SEEK_FLAG_TRICKMODE;
    else
        seek_flags |= GST_SEEK_FLAG_ACCURATE;

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
    if (seek_event && gst_element_send_event(pipeline_, seek_event) ) {
        seeking_ = true;
#ifdef MEDIA_PLAYER_DEBUG
        Log::Info("MediaPlayer %s Seek %ld %.1f", std::to_string(id_).c_str(), seek_pos, rate_);
#endif
    }
    else
        Log::Info("MediaPlayer %s Seek failed", std::to_string(id_).c_str());

    // Force update
    if (force) {
        gst_element_get_state (pipeline_, NULL, NULL, GST_CLOCK_TIME_NONE);
        force_update_ = true;
    }

}


void MediaPlayer::setRate(double s)
{
    // bound to interval [-MAX_PLAY_SPEED MAX_PLAY_SPEED]
    rate_ = CLAMP(s, -MAX_PLAY_SPEED, MAX_PLAY_SPEED);
    // skip interval [-MIN_PLAY_SPEED MIN_PLAY_SPEED]
    if (ABS(rate_) < MIN_PLAY_SPEED)
        rate_ = SIGN(rate_) * MIN_PLAY_SPEED;

}

void MediaPlayer::setPlaySpeed(double s)
{
#ifdef USE_GST_PLAYBIN
    bool change_direction =  s * rate_ < 0.0;
#endif

    // Set rate to requested value (may be executed later)
    setRate(s);

    // discard if too early or not possible
    if (pipeline_ == nullptr || !media_.seekable)
        return;

    //
    // Apply rate change with gstreamer seek
    //

#ifdef USE_GST_PLAYBIN
    //  GST_SEEK_FLAG_INSTANT_RATE_CHANGE: Signals that a rate change should be
    //  applied immediately. Only valid if start/stop position
    //  are GST_CLOCK_TIME_NONE, the playback direction does not change
    //  and the seek is not flushing. (Since: 1.18)
    // if all conditions to use GST_SEEK_FLAG_INSTANT_RATE_CHANGE are met
    if ( rate_change_ == RATE_CHANGE_INSTANT && !change_direction && !media_.isimage) {

        int seek_flags = GST_SEEK_FLAG_INSTANT_RATE_CHANGE;
        seek_flags |= GST_SEEK_FLAG_TRICKMODE;

        GstEvent *seek_event = gst_event_new_seek (rate_, GST_FORMAT_TIME,
                                                   (GstSeekFlags) seek_flags,
                                                   GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE,
                                                   GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE);

        if (seek_event && gst_element_send_event(pipeline_, seek_event) )
            seeking_ = true;
        else {
            Log::Info("MediaPlayer %s; cannot perform instantaneous speed change. Change of play speed will not be smooth.", std::to_string(id_).c_str());
            rate_change_ = RATE_CHANGE_FLUSH;
        }
    }
    // Generic way is a flush seek
    // following the example from
    // https://gstreamer.freedesktop.org/documentation/tutorials/basic/playback-speed.html
    else
        execute_seek_command();

    // Set after initialization (will be used next time)
    if (rate_change_ == RATE_CHANGE_NONE)
        rate_change_ = RATE_CHANGE_INSTANT;
#else
    execute_seek_command();
#endif
}

double MediaPlayer::playSpeed() const
{
    return rate_;
}

Timeline *MediaPlayer::timeline()
{
    return &timeline_;
}

float MediaPlayer::currentTimelineFading()
{
    return timeline_.fadingAt(position_);
}

void MediaPlayer::setTimeline(const Timeline &tl)
{
    timeline_ = tl;
}

MediaInfo MediaPlayer::media() const
{
    return media_;
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
    return static_cast<double>(media_.framerate_n) / static_cast<double>(media_.framerate_d);;
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
    frame_[write_index_].unmap();

    // accept status of frame received
    frame_[write_index_].status = status;

    // a buffer is given (not EOS)
    if (buf != NULL) {

        // get the frame from buffer
        if ( !gst_video_frame_map (&frame_[write_index_].vframe, &v_frame_video_info_, buf, GST_MAP_READ ) )
        {
#ifdef MEDIA_PLAYER_DEBUG
            Log::Info("MediaPlayer %s Failed to map the video buffer", std::to_string(id_).c_str());
#endif
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
            if (timeline_.first() == GST_CLOCK_TIME_NONE) {
                timeline_.setFirst(buf->pts);
                // add a gap to show that before
                if (buf->pts > 0 && !timeline_.gapAt( buf->pts ))
                    timeline_.addGap(0, buf->pts);
            }
        }
        // full but invalid frame : will be deleted next iteration
        // (should never happen)
        else {
#ifdef MEDIA_PLAYER_DEBUG
            Log::Info("MediaPlayer %s Received an Invalid frame", std::to_string(id_).c_str());
#endif
            // free access to frame & exit
            frame_[write_index_].status = INVALID;
            frame_[write_index_].access.unlock();
            return false;
        }
    }
    // else; null buffer for EOS: give a position
    else {
        frame_[write_index_].status = EOS;
        frame_[write_index_].position = rate_ > 0.0 ? timeline_.end() : timeline_.begin();
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
    MediaPlayer *m = static_cast<MediaPlayer *>(p);
    if (m && m->opened_) {
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

        // send frames to media player only if ready
        MediaPlayer *m = static_cast<MediaPlayer *>(p);
        if (m && m->opened_) {

            // get buffer from sample
            GstBuffer *buf = gst_sample_get_buffer (sample);

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

        // send frames to media player only if ready
        MediaPlayer *m = static_cast<MediaPlayer *>(p);
        if (m && m->opened_) {

            // get buffer from sample (valid until sample is released)
            GstBuffer *buf = gst_sample_get_buffer (sample) ;

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



MediaPlayer::TimeCounter::TimeCounter(): fps(1.f)
{
    timer = g_timer_new ();
}

MediaPlayer::TimeCounter::~TimeCounter()
{
    g_free(timer);
}

void MediaPlayer::TimeCounter::tic ()
{
    const double dt = g_timer_elapsed (timer, NULL) * 1000.0;

    // ignore refresh after too little time
    if (dt > 3.0){
        // restart timer
        g_timer_start(timer);
        // calculate instantaneous framerate
        // Exponential moving averate with previous framerate to filter jitter
        fps = CLAMP( 0.5 * fps + 500.0 / dt, 0.0, 1000.0);
    }
}


//static void audio_changed_callback (GstElement *pipeline, MediaPlayer *mp)
//{
//    gint n_audio;
//    g_object_get (G_OBJECT (pipeline), "n-audio", &n_audio, NULL);
//    if ( n_audio > 0 ) {
//        Log::Info("MediaPlayer %d Audio stream muted", std::to_string( mp->id() ).c_str() );
//        gst_stream_volume_set_volume (GST_STREAM_VOLUME (pipeline), GST_STREAM_VOLUME_FORMAT_LINEAR, 0.);
//        gst_stream_volume_set_mute (GST_STREAM_VOLUME (pipeline), true);
//    }
//}
//    flags |= GST_PLAY_FLAG_AUDIO;
//    g_signal_connect ( G_OBJECT (pipeline_), "audio-changed", G_CALLBACK (audio_changed_callback), this);
