#include "MediaPlayer.h"

// vmix
#include "defines.h"
#include "Log.h"
#include "RenderingManager.h"
#include "ResourceManager.h"
#include "UserInterfaceManager.h"
#include "GstToolkit.h"

//  Desktop OpenGL function loader
#include <glad/glad.h>  

// GStreamer
#include <gst/gl/gl.h>
#include <gst/gstformat.h>
#include <gst/pbutils/gstdiscoverer.h>
#include <gst/app/gstappsink.h>

// opengl texture
static GLuint tex_index_black = 0;
static GstStaticCaps gl_render_caps = GST_STATIC_CAPS ("video/x-raw(memory:GLMemory),format=RGBA,texture-target=2D");
static GstStaticCaps frame_render_caps = GST_STATIC_CAPS ("video/x-raw,format=RGB");

GLuint blackTexture()
{
    // generate texture (once) & clear
    if (tex_index_black == 0) {
        glGenTextures(1, &tex_index_black);
        glBindTexture( GL_TEXTURE_2D, tex_index_black);
        unsigned char clearColor[3] = {0};
        // texture with one black pixel
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, clearColor);
    }

    return tex_index_black;
}

MediaPlayer::MediaPlayer(string name) : id(name)
{
    uri = "undefined";
    ready = false;
    seekable = false;
    isimage = false;
    pipeline = nullptr;
    discoverer = nullptr;

    width = 640;
    height = 480;
    position = GST_CLOCK_TIME_NONE;
    duration = GST_CLOCK_TIME_NONE;
    start_position = GST_CLOCK_TIME_NONE;
    frame_duration = GST_CLOCK_TIME_NONE;
    desired_state = GST_STATE_PAUSED;
    rate = 1.0;
    framerate = 1.0;
    loop = LoopMode::LOOP_REWIND;
    need_loop = false;
    v_frame_is_full = false;

    textureindex = 0;    
}

MediaPlayer::~MediaPlayer()
{
    Close();
    // g_free(v_frame);
}

void MediaPlayer::Bind() 
{
    glBindTexture(GL_TEXTURE_2D, Texture());   
}

guint MediaPlayer::Texture() const 
{
    if (textureindex == 0)
        return blackTexture();

    return textureindex;   
}

void MediaPlayer::Open(string uri)
{
    // set uri to open
    this->uri = uri;

    // reset
    ready = false;

    /* Instantiate the Discoverer */
    GError *err = NULL;
    discoverer = gst_discoverer_new (5 * GST_SECOND, &err);
    if (!discoverer) {
        Log::Info("Error creating discoverer instance: %s\n", err->message);
        g_clear_error (&err);
        return;
    }

    // set callback for filling in information into this MediaPlayer 
    g_signal_connect (discoverer, "discovered", G_CALLBACK (callback_discoverer_process), this);
    // set callback when finished discovering
    g_signal_connect (discoverer, "finished", G_CALLBACK (callback_discoverer_finished), this);
    
    // start discoverer
    gst_discoverer_start(discoverer);
    // Add the request to process asynchronously the URI 
    if (!gst_discoverer_discover_uri_async (discoverer, uri.c_str())) {
        UserInterface::Warning("Failed to start discovering URI '%s'\n", uri.c_str());
        g_object_unref (discoverer);
        discoverer = nullptr;
    }

    // and wait for discoverer to finish...
}


void MediaPlayer::execute_open() 
{
    // build string describing pipeline
    string description = "uridecodebin uri=" + uri + " name=decoder ! videoconvert ! "
                         "video/x-raw,format=RGB ! appsink name=sink";

    // parse pipeline descriptor
    GError *error = NULL;
    pipeline = gst_parse_launch (description.c_str(), &error);
    if (error != NULL) {
        UserInterface::Warning("Could not construct pipeline %s:\n%s\n", description.c_str(), error->message);
        g_clear_error (&error);
        return;
    }
    g_object_set(G_OBJECT(pipeline), "name", id.c_str(), NULL);

    // GstCaps *caps = gst_static_caps_get (&frame_render_caps);    
    string capstring = "video/x-raw,format=RGB,width="+ std::to_string(width) + ",height=" + std::to_string(height);
    GstCaps *caps = gst_caps_from_string(capstring.c_str());
    if (!gst_video_info_from_caps (&v_frame_video_info, caps)) {
        UserInterface::Warning("%s: Could not configure MediaPlayer video frame info\n", gst_element_get_name(pipeline));
        return;
    }

    // setup appsink
    GstElement *sink = gst_bin_get_by_name (GST_BIN (pipeline), "sink");
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
        UserInterface::Warning("%s: Could not configure MediaPlayer sink\n", gst_element_get_name(pipeline));
        return;
    }
    gst_caps_unref (caps);
    
    // capture bus signals to force a unique opengl context for all GST elements 
    //Rendering::LinkPipeline(GST_PIPELINE (pipeline));

    // set to desired state (PLAY or PAUSE)
    GstStateChangeReturn ret = gst_element_set_state (pipeline, desired_state);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        UserInterface::Warning("%s: Failed to open media %s \n%s\n", gst_element_get_name(pipeline), uri.c_str(), discoverer_message.str().c_str());
    }
    else {
        Log::Info("%s: Media Player openned %s\n", gst_element_get_name(pipeline), uri.c_str());

        // all good
        ready = true;
    }

    discoverer_message.clear();
}

bool MediaPlayer::isOpen() const
{
    return ready;
}

void MediaPlayer::Close()
{
    // stop discovering stream
    if (discoverer != nullptr) {
        gst_discoverer_stop (discoverer);
        g_object_unref (discoverer);
        discoverer = nullptr;
    }

    if (!ready) 
        return;

    /* clean up GST */
    if (pipeline != nullptr) {
        gst_element_set_state (pipeline, GST_STATE_NULL);
        gst_object_unref (pipeline);
        pipeline = nullptr;
    }

    // nothing to display
    textureindex = blackTexture();

    // un-ready the media player
    ready = false;
}


GstClockTime MediaPlayer::Duration() 
{
    if (duration == GST_CLOCK_TIME_NONE && pipeline != nullptr) {
        gint64 d = GST_CLOCK_TIME_NONE;
        if ( gst_element_query_duration(pipeline, GST_FORMAT_TIME, &d) ) 
            duration = d;
    }

    return duration;
}

GstClockTime MediaPlayer::FrameDuration() 
{
    return frame_duration;
}

guint MediaPlayer::Width() const
{
    return width;
}

guint MediaPlayer::Height() const
{
    return height;
}

float MediaPlayer::AspectRatio() const
{
    return static_cast<float>(width) / static_cast<float>(height);
}

GstClockTime MediaPlayer::Position()
{
    GstClockTime pos = position;
    
    if (pos == GST_CLOCK_TIME_NONE && pipeline != nullptr) {
        gint64 p = GST_CLOCK_TIME_NONE;
        if ( gst_element_query_position (pipeline, GST_FORMAT_TIME, &p) )
            pos = p;
    }
    
    return pos - start_position;
}

void MediaPlayer::Play(bool on)
{
    if (isimage)
        return;

    // request state 
    desired_state = on ? GST_STATE_PLAYING : GST_STATE_PAUSED;

    // if not ready yet, the requested state will be handled later
    if ( pipeline == nullptr  )
        return;

    // requesting to play, but stopped at end of stream : rewind first !
    if ( desired_state == GST_STATE_PLAYING) {
        if ( ( rate>0.0 ? duration - Position() : Position() ) < 2 * frame_duration ) 
            Rewind();
    }

    // all ready, apply state change immediately
    GstStateChangeReturn ret = gst_element_set_state (pipeline, desired_state);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        UserInterface::Warning("Failed to start up Media %s\n", gst_element_get_name(pipeline));
    }
    else if (on)
        Log::Info("Start Media %s\n", gst_element_get_name(pipeline));
    else
        Log::Info("Stop Media %s\n", gst_element_get_name(pipeline));
}

bool MediaPlayer::isPlaying() const
{
    // image cannot play
    if (isimage)
        return false;

    // if not ready yet, answer with requested state
    if ( pipeline == nullptr )
        return desired_state == GST_STATE_PLAYING;

    // if ready, answer with actual state
    GstState state;
    gst_element_get_state (pipeline, &state, NULL, GST_CLOCK_TIME_NONE);
    return state == GST_STATE_PLAYING;

    // return desired_state == GST_STATE_PLAYING;
}


MediaPlayer::LoopMode MediaPlayer::Loop() const
{
    return loop;
}
    
void MediaPlayer::setLoop(MediaPlayer::LoopMode mode)
{
    loop = mode;
}

void MediaPlayer::Rewind()
{
    if (!seekable)
        return;

    if (rate > 0.0)
        // playing forward, loop to begin
        execute_seek_command(0);
    else
        // playing backward, loop to end
        execute_seek_command(duration);
}


void MediaPlayer::SeekNextFrame()
{
    // useful only when Paused
    if (isPlaying())
        return;

    if ( loop != LOOP_NONE) {
        // eventually loop if mode allows
        if ( ( rate>0.0 ? duration - Position() : Position() ) <  2 * frame_duration ) 
                need_loop = true;
    }

    // step 
    gst_element_send_event (pipeline, gst_event_new_step (GST_FORMAT_BUFFERS, 1, ABS(rate), TRUE,  FALSE));
    
}

void MediaPlayer::SeekTo(GstClockTime pos)
{
    if (!seekable)
        return;

    GstClockTime target = CLAMP(pos, 0, duration);
    execute_seek_command(target);
}

void MediaPlayer::FastForward()
{
    if (!seekable)
        return;

    double step = SIGN(rate) * 0.01 * static_cast<double>(duration);
    GstClockTime target = Position() + static_cast<GstClockTime>(step);

    // manage loop
    if ( target > duration ) {
        if (loop == LOOP_NONE)
            target = duration;
        else 
            target = target - duration;
    }

    SeekTo(target);
}


void MediaPlayer::Update()
{
    // discard 
    if (!ready) return;

    // done discovering stream
    if (discoverer != nullptr) {
        gst_discoverer_stop (discoverer);
        g_object_unref (discoverer);
        discoverer = nullptr;
    }

    // apply texture
    if (v_frame_is_full) {
        if (textureindex==0) {
            glActiveTexture(GL_TEXTURE0);
            glGenTextures(1, &textureindex);
            glBindTexture(GL_TEXTURE_2D, textureindex);
	        glPixelStorei(GL_UNPACK_ALIGNMENT, 3);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 
                         0, GL_RGB, GL_UNSIGNED_BYTE, v_frame.data[0]);
        }
        else
        {
            glBindTexture(GL_TEXTURE_2D, textureindex);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, 
                            GL_RGB, GL_UNSIGNED_BYTE, v_frame.data[0]);
        }        

        // sync with callback_pull_last_sample_video 
        v_frame_is_full = false;
    }

    // manage loop mode
    if (need_loop && !isimage) {
        if (loop == LOOP_NONE)
            Play(false);
        else 
            execute_loop_command();

        need_loop = false;
    }

}

void MediaPlayer::execute_loop_command()
{
    if ( pipeline == nullptr)  return;

    if (loop==LOOP_REWIND) {
        Rewind();
    } 
    else if (loop==LOOP_BIDIRECTIONAL) {
        rate *= - 1.f;
        execute_seek_command();
    }

}

void MediaPlayer::execute_seek_command(GstClockTime target)
{
    if ( pipeline == nullptr || !seekable)  
        return;

    GstEvent *seek_event = nullptr;

    // seek position : default to target
    GstClockTime seek_pos = target;
    // seek speed rate
    gdouble seek_rate = rate;
    // seek with flush (always)
    int seek_flags = GST_SEEK_FLAG_FLUSH;

    // no target given
    if (target == GST_CLOCK_TIME_NONE) 
        // create seek event with current position (rate changed ?)
        seek_pos = position;
    // target is given but useless
    else if (target == Position()) 
        // ignore request
        return;
    
    // seek with trick mode if fast speed
    if ( ABS(seek_rate) > 2.0 )
        seek_flags |= GST_SEEK_FLAG_TRICKMODE | GST_SEEK_FLAG_TRICKMODE_NO_AUDIO;

    // create seek event depending on direction
    if (seek_rate > 0) {
        seek_event = gst_event_new_seek (seek_rate, GST_FORMAT_TIME, (GstSeekFlags) seek_flags, 
            GST_SEEK_TYPE_SET, seek_pos, GST_SEEK_TYPE_END, 0);
    } else {
        seek_event = gst_event_new_seek (seek_rate, GST_FORMAT_TIME, (GstSeekFlags) seek_flags, 
            GST_SEEK_TYPE_SET, 0, GST_SEEK_TYPE_SET, seek_pos);
    }

    /* Send the event */
    if (seek_event && !gst_element_send_event(pipeline, seek_event) )
        Log::Info("Seek failed in Media %s\n", gst_element_get_name(pipeline));
    else
    {
        Log::Info("Seek Media %s %ld %f\n", gst_element_get_name(pipeline), seek_pos, seek_rate);
    }
        

}

void MediaPlayer::SetPlaySpeed(double s)
{
    if (isimage)
        return;

    // bound to interval [-MAX_PLAY_SPEED MAX_PLAY_SPEED] 
    rate = CLAMP(s, -MAX_PLAY_SPEED, MAX_PLAY_SPEED);
    // skip interval [-MIN_PLAY_SPEED MIN_PLAY_SPEED]
    if (ABS(rate) < MIN_PLAY_SPEED)
        rate = SIGN(rate) * MIN_PLAY_SPEED;
        
    // apply with seek
    execute_seek_command();
}

double MediaPlayer::PlaySpeed() const
{
    return rate;
}


double MediaPlayer::FrameRate() const
{
    return framerate;
}

double MediaPlayer::UpdateFrameRate() const
{
    return timecount.framerate();
}

// CALLBACKS

bool MediaPlayer::fill_v_frame(GstBuffer *buf)
{
    // always empty frame before filling it again
    if (v_frame.buffer)
        gst_video_frame_unmap(&v_frame);

    // get the frame from buffer
    if ( !gst_video_frame_map (&v_frame, &v_frame_video_info, buf, GST_MAP_READ ) ) {
        Log::Info("Failed to map the video buffer");
        return false;
    }

    // validate frame format
    if( GST_VIDEO_INFO_IS_RGB(&(v_frame).info) && GST_VIDEO_INFO_N_PLANES(&(v_frame).info) == 1) {

        // validate time
        if (position != buf->pts) {

            // got a new RGB frame !
            v_frame_is_full = true;

            // get presentation time stamp
            position = buf->pts;

            // set start position (i.e. pts of first frame we got)
            if (start_position == GST_CLOCK_TIME_NONE) 
                start_position = position;
        }

    }

    return true;
}

GstFlowReturn MediaPlayer::callback_get_last_sample_video (GstElement *bin, MediaPlayer *m)
{   
    GstFlowReturn ret = GST_FLOW_OK;
    
    if (m && !m->v_frame_is_full) {
        // get last sample (non blocking)
        GstSample *sample = nullptr;
        g_object_get (bin, "last-sample", &sample, NULL);

        if (sample != nullptr) {

            GstBuffer *buf;
            buf = gst_buffer_ref ( gst_sample_get_buffer (sample) );
            gst_sample_unref (sample);

            if ( !m->fill_v_frame(buf) )
                ret = GST_FLOW_ERROR;
            gst_buffer_unref (buf);

        }
    }

    return ret;
}


GstFlowReturn MediaPlayer::callback_pull_sample_video (GstElement *bin, MediaPlayer *m)
{
    GstFlowReturn ret = GST_FLOW_OK;

    if (m && !m->v_frame_is_full) {

        // get last sample (non blocking)
        GstSample *sample = nullptr;
        g_object_get (bin, "last-sample", &sample, NULL);        

        // if got a valid sample
        if (sample != nullptr) {

            // get buffer from sample
            GstBuffer *buf = gst_buffer_ref ( gst_sample_get_buffer (sample) );

            // fill frame from buffer
            if ( !m->fill_v_frame(buf) )
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

        // keep update time (i.e. actual FPS of update)
        m->timecount.tic();
    }

    return ret;
}

void MediaPlayer::callback_end_of_video (GstElement *bin, MediaPlayer *m)
{
    if (m) {
        // reached end of stream (eos) : might need to loop !
        m->need_loop = true;
    }
}

void MediaPlayer::callback_discoverer_process (GstDiscoverer *discoverer, GstDiscovererInfo *info, GError *err, MediaPlayer *m)
{
    // handle general errors
    const gchar *uri = gst_discoverer_info_get_uri (info);
    GstDiscovererResult result = gst_discoverer_info_get_result (info);
    switch (result) {
        case GST_DISCOVERER_URI_INVALID:
        // g_print ("Invalid URI '%s'\n", uri);
            m->discoverer_message << "Invalid URI: " << uri;
        break;
        case GST_DISCOVERER_ERROR:
        // g_print ("Discoverer error: %s\n", err->message);
            m->discoverer_message << "Error: " << err->message;
        break;
        case GST_DISCOVERER_TIMEOUT:
            m->discoverer_message << "Time out";
        break;
        case GST_DISCOVERER_BUSY:
            m->discoverer_message << "Busy";
        break;
        case GST_DISCOVERER_MISSING_PLUGINS:
        {
            const GstStructure *s = gst_discoverer_info_get_misc (info);
            gchar *str = gst_structure_to_string (s);
            m->discoverer_message << "Missing plugin " << str;
            g_free (str);
        }
        break;
        case GST_DISCOVERER_OK:
        break;
    }
    // no error, handle information found
    if ( result == GST_DISCOVERER_OK && m) {

        // look for video stream at that uri
        bool foundvideostream = false;
        GList *streams = gst_discoverer_info_get_video_streams(info);
        GList *tmp;
        for (tmp = streams; tmp && !foundvideostream; tmp = tmp->next ) {
            GstDiscovererStreamInfo *tmpinf = (GstDiscovererStreamInfo *) tmp->data;
            if ( GST_IS_DISCOVERER_VIDEO_INFO(tmpinf) )
            {
                GstDiscovererVideoInfo* vinfo = GST_DISCOVERER_VIDEO_INFO(tmpinf);
                m->width = gst_discoverer_video_info_get_width(vinfo);
                m->height = gst_discoverer_video_info_get_height(vinfo);
                m->isimage = gst_discoverer_video_info_is_image(vinfo);
                if ( !m->isimage ) {
                    m->duration = gst_discoverer_info_get_duration (info);
                    m->seekable = gst_discoverer_info_get_seekable (info);
                    guint frn = gst_discoverer_video_info_get_framerate_num(vinfo);
                    guint frd = gst_discoverer_video_info_get_framerate_denom(vinfo);
                    m->framerate = static_cast<double>(frn) / static_cast<double>(frd);
                    m->frame_duration = (GST_SECOND * static_cast<guint64>(frd)) / (static_cast<guint64>(frn));
                }
                foundvideostream = true;
            }
        }
        gst_discoverer_stream_info_list_free (streams);

        if (!foundvideostream) {
            m->discoverer_message << "No video stream.";
        }
    
    }
}

void MediaPlayer::callback_discoverer_finished(GstDiscoverer *discoverer, MediaPlayer *m)
{
    // finished the discoverer : finalize open status
    if (m)
        m->execute_open();
}

TimeCounter::TimeCounter() {
    current_time = gst_util_get_timestamp ();
    last_time = gst_util_get_timestamp();
    nbFrames = 0;
    fps = 0.f;
}

void TimeCounter::tic ()
{
    current_time = gst_util_get_timestamp ();
    nbFrames++ ;
    if ((current_time - last_time) >= GST_SECOND)
    {
        last_time = current_time;
        fps = 0.2f * fps + 0.8f * static_cast<float>(nbFrames);
        nbFrames = 0;
    }
}

float TimeCounter::framerate() const
{
    return fps;
}

int TimeCounter::framecount() const
{
    return nbFrames;
}





    // /* TESTING  */


// static void cb_new_pad (GstElement* decodebin, GstPad* pad, GstElement* target)
// {
//     GstPad* target_pad = gst_element_get_static_pad (target, "sink");
//     if (!target_pad)
//         g_warning ("target has no sink\n");

//     //only link once
//     if (GST_PAD_IS_LINKED (target_pad))
//     {
//         gst_object_unref (target_pad);
//         g_warning ("target not free\n");
//         return;
//     }

//     GstCaps* caps = gst_pad_get_current_caps (pad);
//     GstStructure* str = gst_caps_get_structure (caps, 0);

//     if (!g_strrstr (gst_structure_get_name (str), "video"))
//     {
//         gst_caps_unref (caps);
//         gst_object_unref (target_pad);
//         g_warning ("decodebin not a video %s\n", gst_structure_get_name (str));
//         return;
//     }
//     gst_caps_unref (caps);

//     GstPadLinkReturn ret = gst_pad_link (pad, target_pad);
//     if (ret != GST_PAD_LINK_OK)
//         g_warning ("Failed to link decodebin %s with target %s ! %d\n", gst_pad_get_name(pad), gst_pad_get_name(target_pad), ret);
            
// }

    // GstElement* videosrc = gst_element_factory_make ("filesrc", "filesrc0");
    // GstElement* decodebin = gst_element_factory_make ("decodebin", "decodebin0");
    // // GstElement* convertbin = gst_element_factory_make ("videoconvert", "converterbin0");
    // GstElement* convertbin = gst_element_factory_make ("glupload", "converterbin0");
    // // GstElement* glimagesink  = gst_element_factory_make ("glimagesink", "glimagesink0");
    // GstElement* glimagesink  = gst_element_factory_make ("glimagesinkelement", "glimagesink0");
    // // GstElement* glimagesink  = gst_element_factory_make ("fakesink", "glimagesink0");

    // if (!videosrc || !decodebin || !convertbin || !glimagesink)
    // {
    //     UserInterface::Error ("A Gstreamer element could not be found \n");
    //     return 1;
    // }
    // /* configure elements */
    // g_object_set(G_OBJECT(videosrc), "num-buffers", 800, NULL);
    // g_object_set(G_OBJECT(videosrc), "location", video_location.c_str(), NULL);
    // g_signal_connect(G_OBJECT(decodebin), "drained", G_CALLBACK (endVideoCallback), NULL);

    // // glimagesink
    // // g_signal_connect(G_OBJECT(glimagesink), "client-draw", G_CALLBACK (drawCallback), NULL);

    // // glimagesinkelement
    // // g_signal_connect(G_OBJECT(glimagesink), "client-reshape", G_CALLBACK (scaleTextureCallback), NULL);
    // g_signal_connect(G_OBJECT(glimagesink), "client-draw", G_CALLBACK (textureCallback), NULL);
    // // // g_object_set(G_OBJECT(glimagesink), "show-preroll-frame", TRUE, NULL);
    // g_object_set(G_OBJECT(glimagesink), "sync", TRUE, NULL);
    // g_object_set(G_OBJECT(glimagesink), "handle-events", FALSE, NULL);
    
    // // fakesink
    // // g_signal_connect(G_OBJECT(glimagesink), "handoff", G_CALLBACK (fakesinkCallback), NULL);
    // // g_object_set(G_OBJECT(glimagesink), "signal-handoffs", TRUE, NULL);
    // // g_object_set(G_OBJECT(glimagesink), "sync", TRUE, NULL);

    // /* add elements */
    // gst_bin_add_many (GST_BIN (pipeline), videosrc, decodebin, convertbin, glimagesink, NULL);

    // /* link elements */
	// if (!gst_element_link (videosrc, decodebin) )
	// // if (!gst_element_link_pads (videosrc, "src", decodebin, "sink") )
    // {
    //     g_warning ("Failed to link videosrc to decodebin !\n");
    //     return -1;
    // }

    // g_signal_connect (decodebin, "pad-added", G_CALLBACK (cb_new_pad), convertbin);

    // if(!gst_element_link (convertbin, glimagesink))
    // {
    //     g_warning("Failed to link convertbin to glimagesink!\n") ;
    //     return -1 ;
    // }


// static gboolean drawTextureCallback (GstElement *object, guint texture,  guint width, guint height, GstElement *pipeline)
// {
//     //printStreamPosition(pipeline);

//     texture_gst = texture;
//     //std::cerr << "Texture " << width << " x " << height << std::endl;
    
//     return TRUE;
// }

// static void fakesinkCallback (GstElement *bin, GstBuffer  buffer, GstPad  pad, GstElement *pipeline)
// {
//     static GstClockTime current_time;
//     static GstClockTime last_time = gst_util_get_timestamp();
//     static gint nbFrames = 0;

//     current_time = gst_util_get_timestamp ();
//     nbFrames++ ;
    
//     if ((current_time - last_time) >= GST_SECOND)
//     {
//         //std::cerr << nbFrames << " frame / s" <<  std::endl;

//       //  GstElement *pipeline = (GstElement*)data;

//         last_time = current_time;
//         nbFrames = 0;

//         // if (!gst_element_seek (pipeline, 1.0, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH,
//         //                         GST_SEEK_TYPE_SET, 0,
//         //                         GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE)) {
//         //     g_print ("Seek failed!\n");
//         // }

//     }

// }

// static void endVideoCallback (GstElement *bin, GstElement *pipeline)
// {
//     g_warning ("end of stream %s\n", gst_element_get_name(pipeline));

//     printStreamPosition(pipeline);

// }

    // video

    // gchar *descr;
    // // descr = g_strdup_printf ("videotestsrc pattern=ball ! video/x-raw,format=RGBA ! glimagesink");
    // // descr = g_strdup_printf ("videotestsrc ! glupload ! glfilterapp name=capture ! fakesink name=sink");
    // descr = g_strdup_printf ("uridecodebin uri=file:///home/bhbn/Videos/glmixervideo180324111104.mp4 name=decoder ! videoconvert ! "
    //                          "video/x-raw,format=RGBA ! glupload ! glfilterapp name=capture ! fakesink name=sink");
    // g_print ("pipeline: %s\n", descr);

    // GError *error = NULL;
    // GstElement *pipeline = gst_parse_launch (descr, &error);
    // if (error != NULL) {
    //     g_print ("could not construct pipeline: %s\n", error->message);
    //     g_clear_error (&error);
    //     exit (-1);
    // }
    // g_object_set(G_OBJECT(pipeline), "name", "vmixpipeline", NULL);

    // GstElement *capture = gst_bin_get_by_name (GST_BIN (pipeline), "capture");
    // if (capture)
    //     g_signal_connect(G_OBJECT(capture), "client-draw", G_CALLBACK (drawTextureCallback), pipeline);
    // GstElement *sink = gst_bin_get_by_name (GST_BIN (pipeline), "sink");
    // if (sink) {
    //     g_signal_connect(G_OBJECT(sink), "handoff", G_CALLBACK (fakesinkCallback), pipeline);
    //     g_object_set(G_OBJECT(sink), "signal-handoffs", TRUE, NULL);
    //     g_object_set(G_OBJECT(sink), "sync", TRUE, NULL);
    // }
    // GstElement *decoder = gst_bin_get_by_name (GST_BIN (pipeline), "decoder");
    // if (decoder)
    //     g_signal_connect(G_OBJECT(decoder), "drained", G_CALLBACK (endVideoCallback), pipeline);
    //     // g_signal_connect(G_OBJECT(decoder), "about-to-finish", G_CALLBACK (endVideoCallback), pipeline);

    // // g_timeout_add (200, (GSourceFunc) printStreamPosition, pipeline);

    // // /* WORKING PIPELINE PURE OGL */
    // // GstElement* pipeline = gst_pipeline_new ("pipeline");
    // // GstElement *src = gst_element_factory_make ("gltestsrc", NULL);

    // // // GstElement *sink = gst_element_factory_make ("glimagesinkelement", NULL);
    // // // g_signal_connect(G_OBJECT(sink), "client-draw", G_CALLBACK (drawCallback), NULL);
    // // // g_object_set(G_OBJECT(sink), "show-preroll-frame", FALSE, NULL);
    // // // g_object_set(G_OBJECT(sink), "handle-events", FALSE, NULL);

    // // GstElement *capture = gst_element_factory_make ("glfilterapp", NULL);
    // // g_signal_connect(G_OBJECT(capture), "client-draw", G_CALLBACK (drawTextureCallback), NULL);
    // // GstElement* sink  = gst_element_factory_make ("fakesink", NULL);
    // // g_signal_connect(G_OBJECT(sink), "handoff", G_CALLBACK (fakesinkCallback), NULL);
    // // g_object_set(G_OBJECT(sink), "signal-handoffs", TRUE, NULL);
    // // g_object_set(G_OBJECT(sink), "sync", TRUE, NULL);

    // // gst_bin_add_many (GST_BIN (pipeline), src, capture, sink, NULL);
    // // gst_element_link_many (src, capture, sink, NULL);


    // // capture bus signals to force a unique opengl context for all GST elements 
    // Rendering::LinkPipeline(GST_PIPELINE (pipeline));

    // /* run */
    // GstStateChangeReturn ret = gst_element_set_state (pipeline, GST_STATE_PLAYING);
    // if (ret == GST_STATE_CHANGE_FAILURE)
    // {
    //     g_warning ("Failed to start up pipeline!\n");

    //     return -1;
    // }

        // string description = "uridecodebin uri=" + uri + " name=decoder ! videoconvert ! "
    //                      "video/x-raw,format=RGBA ! glupload ! glfilterapp name=capture ! appsink name=sink";

    // GError *error = NULL;
    // pipeline = gst_parse_launch (description.c_str(), &error);
    // if (error != NULL) {
    //     ImGuiToolkit::Log("Could not construct pipeline %s:\n%s\n", description.c_str(), error->message);
    //     g_clear_error (&error);
    //     return false;
    // }
    // g_object_set(G_OBJECT(pipeline), "name", id.c_str(), NULL);

    // GstElement *capture = gst_bin_get_by_name (GST_BIN (pipeline), "capture");
    // if (capture) {
    //     g_signal_connect(G_OBJECT(capture), "client-draw", G_CALLBACK (textureCallback), this);
    // }

    // GstElement *sink = gst_bin_get_by_name (GST_BIN (pipeline), "sink");
    // if (sink) {
    //     GstCaps *caps = gst_static_caps_get (&render_caps);

    //     g_object_set (sink, "emit-signals", TRUE, "enable-last-sample", TRUE, "sync", TRUE, "wait-on-eos", FALSE, "caps", caps, NULL);
    //     g_signal_connect(G_OBJECT(sink), "new-sample", G_CALLBACK (sampleCallback), this);
    //     g_signal_connect(G_OBJECT(sink), "new-preroll", G_CALLBACK (sampleCallback), this);
    //     g_signal_connect(G_OBJECT(sink), "eos", G_CALLBACK (endMediaCallback), this);
        
    //     gst_object_unref (sink);
    //     gst_caps_unref (caps);
    // }

    // // capture bus signals to force a unique opengl context for all GST elements 
    // Rendering::LinkPipeline(GST_PIPELINE (pipeline));

    // /* set to PAUSED to make the first frame arrive in the sink */
    // GstStateChangeReturn ret = gst_element_set_state (pipeline, GST_STATE_PAUSED);
    // if (ret == GST_STATE_CHANGE_FAILURE) {
    //     ImGuiToolkit::Log("%s: Failed to open media %s\n", gst_element_get_name(pipeline), uri.c_str());
    //     return false;
    // }
