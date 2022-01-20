/*
 * This file is part of vimix - video live mixer
 *
 * **Copyright** (C) 2019-2022 Bruno Herbelin <bruno.herbelin@gmail.com>
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

#include <thread>
#include <chrono>

//  Desktop OpenGL function loader
#include <glad/glad.h>

#include "defines.h"
#include "Log.h"
#include "Resource.h"
#include "Visitor.h"
#include "SystemToolkit.h"
#include "BaseToolkit.h"

#include "Stream.h"

#ifndef NDEBUG
#define STREAM_DEBUG
#endif


Stream::Stream()
{
    // create unique id
    id_ = BaseToolkit::uniqueId();

    description_ = "undefined";
    pipeline_ = nullptr;
    opened_ = false;
    enabled_ = true;
    desired_state_ = GST_STATE_PAUSED;
    position_ = GST_CLOCK_TIME_NONE;

    width_ = -1;
    height_ = -1;
    single_frame_ = false;
    live_ = false;
    failed_ = false;

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
    textureinitialized_ = false;
}

Stream::~Stream()
{
    Stream::close();

    // cleanup opengl texture
    if (textureindex_)
        glDeleteTextures(1, &textureindex_);

    // cleanup picture buffer
    if (pbo_[0])
        glDeleteBuffers(2, pbo_);
}

void Stream::accept(Visitor& v) {
    v.visit(*this);
}

guint Stream::texture() const
{
    if (textureindex_ == 0)
        return Resource::getTextureBlack();

    return textureindex_;
}

GstFlowReturn callback_stream_discoverer (GstAppSink *sink, gpointer p)
{
    GstFlowReturn ret = GST_FLOW_OK;

    // blocking read pre-roll sample
    GstSample *sample = gst_app_sink_pull_preroll(sink);
    if (sample != NULL) {
        // access info structure
        StreamInfo *info = static_cast<StreamInfo *>(p);
        // get caps of the sample
        GstVideoInfo v_frame_video_info_;
        GstCaps *caps = gst_sample_get_caps(sample);
        if (gst_video_info_from_caps (&v_frame_video_info_, caps)) {
            // fill the info
            info->width = v_frame_video_info_.width;
            info->height = v_frame_video_info_.height;
            // release info to let StreamDiscoverer go forward
            info->discovered.notify_all();
        }
    }
    else
        ret = GST_FLOW_FLUSHING;

    gst_sample_unref (sample);

    return ret;
}

StreamInfo StreamDiscoverer(const std::string &description, guint w, guint h)
{
    // the stream info to return
    StreamInfo info(w, h);

    // no valid info, run a test pipeline to discover the size of the stream
    if ( !info.valid() ) {
        // complete the pipeline description with an appsink (to add a callback)
        std::string _description = description;
        _description += " ! appsink name=sink";

        // try to launch the pipeline
        GError *error = NULL;
        GstElement *_pipeline = gst_parse_launch (_description.c_str(), &error);
        if (error == NULL) {

            // some sanity config
            gst_pipeline_set_auto_flush_bus( GST_PIPELINE(_pipeline), true);

            // get the appsink
            GstElement *sink = gst_bin_get_by_name (GST_BIN (_pipeline), "sink");
            if (sink) {

                // add a preroll callback
                GstAppSinkCallbacks callbacks;
                callbacks.new_preroll = callback_stream_discoverer;
                gst_app_sink_set_callbacks (GST_APP_SINK(sink), &callbacks, &info, NULL);

                // start to play the pipeline
                gst_element_set_state (_pipeline, GST_STATE_PLAYING);

                // wait for the callback_stream_discoverer to return, no more than 4 sec
                std::mutex mtx;
                std::unique_lock<std::mutex> lck(mtx);
                if ( info.discovered.wait_for(lck,std::chrono::seconds(TIMEOUT))  == std::cv_status::timeout)
                    info.message = "Time out";
            }

            // stop and delete pipeline
            GstStateChangeReturn ret = gst_element_set_state (_pipeline, GST_STATE_NULL);
            if (ret == GST_STATE_CHANGE_ASYNC)
                gst_element_get_state (_pipeline, NULL, NULL, GST_CLOCK_TIME_NONE);
            gst_object_unref (_pipeline);

        }
        else
            info.message = error->message;
    }
    // at this point, the info should be filled
    return info;
}

void Stream::open(const std::string &gstreamer_description, guint w, guint h)
{
    if ( w != width_ || h != height_ )
        textureinitialized_ = false;

    // set gstreamer pipeline source
    description_ = gstreamer_description;

    // close before re-openning
    if (isOpen())
        close();

    // open when ready
    discoverer_ = std::async(StreamDiscoverer, description_, w, h);

}


std::string Stream::description() const
{
    return description_;
}

void Stream::execute_open()
{
    // reset
    opened_ = false;

    // Add custom app sink to the gstreamer pipeline
    std::string description = description_;
    description += " ! appsink name=sink";

    // parse pipeline descriptor
    GError *error = NULL;
    pipeline_ = gst_parse_launch (description.c_str(), &error);
    if (error != NULL) {
        fail(std::string("Could not construct pipeline: ") + error->message + "\n" + description);
        g_clear_error (&error);
        return;
    }
    g_object_set(G_OBJECT(pipeline_), "name", std::to_string(id_).c_str(), NULL);
    gst_pipeline_set_auto_flush_bus( GST_PIPELINE(pipeline_), true);

    // GstCaps *caps = gst_static_caps_get (&frame_render_caps);
    std::string capstring = "video/x-raw,format=RGBA,width="+ std::to_string(width_) +
            ",height=" + std::to_string(height_);
    GstCaps *caps = gst_caps_from_string(capstring.c_str());
    if (!caps || !gst_video_info_from_caps (&v_frame_video_info_, caps)) {
        fail("Could not configure video frame info");
        return;
    }

    // setup appsink
    GstElement *sink = gst_bin_get_by_name (GST_BIN (pipeline_), "sink");
    if (!sink) {
        fail("Could not configure pipeline sink.");
        return;
    }

    // instruct sink to use the required caps
    gst_app_sink_set_caps (GST_APP_SINK(sink), caps);

    // Instruct appsink to drop old buffers when the maximum amount of queued buffers is reached.
    gst_app_sink_set_max_buffers( GST_APP_SINK(sink), 30);
    gst_app_sink_set_drop (GST_APP_SINK(sink), true);

    // set the callbacks
    GstAppSinkCallbacks callbacks;
    callbacks.new_preroll = callback_new_preroll;
    if (single_frame_) {
        callbacks.eos = NULL;
        callbacks.new_sample = NULL;
        Log::Info("Stream %s contains a single frame", std::to_string(id_).c_str());
    }
    else {
        callbacks.eos = callback_end_of_stream;
        callbacks.new_sample = callback_new_sample;
    }
    gst_app_sink_set_callbacks (GST_APP_SINK(sink), &callbacks, this, NULL);
    gst_app_sink_set_emit_signals (GST_APP_SINK(sink), false);

    // set to desired state (PLAY or PAUSE)
    live_ = false;
    GstStateChangeReturn ret = gst_element_set_state (pipeline_, desired_state_);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        fail(std::string("Could not open ") + description_);
        return;
    }
    else if (ret == GST_STATE_CHANGE_NO_PREROLL) {
        Log::Info("Stream %s is a live stream", std::to_string(id_).c_str());
        live_ = true;
    }

    // instruct the sink to send samples synched in time if not live source
    gst_base_sink_set_sync (GST_BASE_SINK(sink), !live_);

    // done with refs
    gst_object_unref (sink);
    gst_caps_unref (caps);

    // all good
    Log::Info("Stream %s Opened '%s' (%d x %d)", std::to_string(id_).c_str(), description.c_str(), width_, height_);
    opened_ = true;

    // launch a timeout to check on open status
    std::thread( timeout_initialize, this ).detach();
}

void Stream::fail(const std::string &message)
{
    Log::Warning("Stream %s %s.", std::to_string(id_).c_str(), message.c_str() );
    failed_ = true;
}

void Stream::timeout_initialize(Stream *str)
{
    // wait for the condition that source was initialized successfully in
    std::mutex mtx;
    std::unique_lock<std::mutex> lck(mtx);
    if ( str->initialized_.wait_for(lck,std::chrono::seconds(TIMEOUT)) == std::cv_status::timeout )
        // if waited more than TIMEOUT, its dead :(
        str->fail("Failed to initialize");
}

bool Stream::isOpen() const
{
    return opened_;
}

bool Stream::failed() const
{
    return failed_;
}

void Stream::Frame::unmap()
{
    if ( full )
        gst_video_frame_unmap(&vframe);
    full = false;
}

void Stream::close()
{
    // not openned?
    if (!opened_) {
        // wait for loading to finish
        if (discoverer_.valid())
            discoverer_.wait();
        // nothing else to change
        return;
    }

    // un-ready
    opened_ = false;
    textureinitialized_ = false;

    // clean up GST
    if (pipeline_ != nullptr) {
        // force flush
        gst_element_send_event(pipeline_, gst_event_new_seek (1.0, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH,
                    GST_SEEK_TYPE_SET, 0, GST_SEEK_TYPE_END, 0) );
        gst_element_get_state (pipeline_, NULL, NULL, 1000000);
        // force end
        GstStateChangeReturn ret = gst_element_set_state (pipeline_, GST_STATE_NULL);
        if (ret == GST_STATE_CHANGE_ASYNC)
            gst_element_get_state (pipeline_, NULL, NULL, 1000000);
        gst_object_unref (pipeline_);
        pipeline_ = nullptr;
    }

    // cleanup eventual remaining frame memory
    for(guint i = 0; i < N_FRAME; ++i){
        frame_[i].access.lock();
        frame_[i].unmap();
        frame_[i].access.unlock();
    }
    write_index_ = 0;
    last_index_ = 0;

}


guint Stream::width() const
{
    return width_;
}

guint Stream::height() const
{
    return height_;
}

float Stream::aspectRatio() const
{
    return static_cast<float>(width_) / static_cast<float>(height_);
}

void Stream::enable(bool on)
{
    if ( !opened_ || pipeline_ == nullptr || !textureinitialized_)
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
        if (ret == GST_STATE_CHANGE_FAILURE)
            fail("Failed to enable");

    }
}

bool Stream::enabled() const
{
    return enabled_;
}

bool Stream::singleFrame() const
{
    return single_frame_;
}

bool Stream::live() const
{
    return live_;
}

void Stream::play(bool on)
{
    // ignore if disabled, and cannot play an image
    if (!enabled_ || single_frame_)
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

    // all ready, apply state change immediately
    GstStateChangeReturn ret = gst_element_set_state (pipeline_, desired_state_);
    if (ret == GST_STATE_CHANGE_FAILURE)
        fail("Failed to play");

#ifdef STREAM_DEBUG
    else if (on)
        Log::Info("Stream %s Start", std::to_string(id_).c_str());
    else
        Log::Info("Stream %s Stop", std::to_string(id_).c_str());
#endif

    // activate live-source
    if (live_)
        gst_element_get_state (pipeline_, NULL, NULL, GST_CLOCK_TIME_NONE);

}

bool Stream::isPlaying(bool testpipeline) const
{
    // image cannot play
    if (single_frame_)
        return false;

    // if not ready yet, answer with requested state
    if ( !testpipeline || pipeline_ == nullptr || !enabled_)
        return desired_state_ == GST_STATE_PLAYING;

    // if ready, answer with actual state
    GstState state;
    gst_element_get_state (pipeline_, &state, NULL, GST_CLOCK_TIME_NONE);
    return state == GST_STATE_PLAYING;
}

void Stream::rewind()
{
    if ( pipeline_ == nullptr )
        return;

    GstEvent *seek_event = gst_event_new_seek (1.0, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH,
                                               GST_SEEK_TYPE_SET, 0, GST_SEEK_TYPE_END, 0);
    gst_element_send_event(pipeline_, seek_event);
}

GstClockTime Stream::position()
{
    if (position_ == GST_CLOCK_TIME_NONE && pipeline_ != nullptr) {
        gint64 p = GST_CLOCK_TIME_NONE;
        if ( gst_element_query_position (pipeline_, GST_FORMAT_TIME, &p) )
            position_ = p;
    }

    return position_;
}

void Stream::init_texture(guint index)
{
    glActiveTexture(GL_TEXTURE0);
    if (textureindex_)
        glDeleteTextures(1, &textureindex_);
    glGenTextures(1, &textureindex_);
    glBindTexture(GL_TEXTURE_2D, textureindex_);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, width_, height_);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width_, height_,
                    GL_RGBA, GL_UNSIGNED_BYTE, frame_[index].vframe.data[0]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    if (!single_frame_) {

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

#ifdef STREAM_DEBUG
        Log::Info("Stream %s Use Pixel Buffer Object texturing.", std::to_string(id_).c_str());
#endif
    }
    glBindTexture(GL_TEXTURE_2D, 0);

    // done
    textureinitialized_ = true;
    initialized_.notify_all();
}


void Stream::fill_texture(guint index)
{
    // is this the first frame ?
    if ( !textureinitialized_ || textureindex_ < 1)
    {
        // initialize texture
        init_texture(index);
    }

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
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width_, height_,
                        GL_RGBA, GL_UNSIGNED_BYTE, frame_[index].vframe.data[0]);
    }

    glBindTexture(GL_TEXTURE_2D, 0);

}

void Stream::update()
{
    // discard
    if (failed_)
        return;

    // not ready yet
    if (!opened_){
        if (discoverer_.valid()) {
            // try to get info from discoverer
            if (discoverer_.wait_for( std::chrono::milliseconds(4) ) == std::future_status::ready )
            {
                // get info
                StreamInfo i(discoverer_.get());
                if (i.valid()) {
                    // got all info needed for openning !
                    width_ = i.width;
                    height_ = i.height;
                    execute_open();
                }
                // invalid info; fail
                else
                    fail("Could not create stream: " + i.message);
            }
        }
        // wait next frame to display
        return;
    }

    // prevent unnecessary updates: already filled image
    if (single_frame_ && textureinitialized_)
        return;

    // local variables before trying to update
    guint read_index = 0;
    bool need_loop = false;

    // locked access to current index
    index_lock_.lock();
    // get the last frame filled from fill_frame()
    read_index = last_index_;
    // Do NOT miss and jump directly to a pre-roll
    for (guint i = 0; i < N_FRAME; ++i) {
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

    if (need_loop) {
        // stop on end of stream
        play(false);
    }
}

double Stream::updateFrameRate() const
{
    return timecount_.frameRate();
}


// CALLBACKS

bool Stream::fill_frame(GstBuffer *buf, FrameStatus status)
{
//    Log::Info("Stream fill frame");

    // Do NOT overwrite an unread EOS
    if ( frame_[write_index_].status == EOS )
        write_index_ = (write_index_ + 1) % N_FRAME;

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
            Log::Info("Stream %s Failed to map the video buffer", std::to_string(id_).c_str());
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

        }
        // full but invalid frame : will be deleted next iteration
        // (should never happen)
        else {
#ifdef STREAM_DEBUG
            Log::Info("Stream %s Received an Invalid frame", std::to_string(id_).c_str());
#endif
            frame_[write_index_].status = INVALID;            
            frame_[write_index_].access.unlock();
            return false;
        }
    }
    // else; null buffer for EOS: give a position
    else {
        frame_[write_index_].status = EOS;
#ifdef STREAM_DEBUG
        Log::Info("Stream %s Reached End Of Stream", std::to_string(id_).c_str());
#endif
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
    write_index_ = (write_index_ + 1) % N_FRAME;

    // calculate actual FPS of update
    timecount_.tic();

    return true;
}

void Stream::callback_end_of_stream (GstAppSink *, gpointer p)
{
    Stream *m = static_cast<Stream *>(p);
    if (m && m->opened_) {
        m->fill_frame(NULL, Stream::EOS);
    }
}

GstFlowReturn Stream::callback_new_preroll (GstAppSink *sink, gpointer p)
{
    GstFlowReturn ret = GST_FLOW_OK;

    // blocking read pre-roll samples
    GstSample *sample = gst_app_sink_pull_preroll(sink);

    // if got a valid sample
    if (sample != NULL) {
        // send frames to media player only if ready
        Stream *m = static_cast<Stream *>(p);
        if (m && m->opened_) {

            // get buffer from sample
            GstBuffer *buf = gst_sample_get_buffer (sample);

            // fill frame from buffer
            if ( !m->fill_frame(buf, Stream::PREROLL) )
                ret = GST_FLOW_ERROR;
        }
    }
    else
        ret = GST_FLOW_FLUSHING;

    // release sample
    gst_sample_unref (sample);

    return ret;
}

GstFlowReturn Stream::callback_new_sample (GstAppSink *sink, gpointer p)
{
    GstFlowReturn ret = GST_FLOW_OK;

//    if (gst_app_sink_is_eos (sink))
//        Log::Info("callback_new_sample got EOS");

    // non-blocking read new sample
    GstSample *sample = gst_app_sink_pull_sample(sink);

    // if got a valid sample
    if (sample != NULL && !gst_app_sink_is_eos (sink)) {

        // send frames to media player only if ready
        Stream *m = static_cast<Stream *>(p);
        if (m && m->opened_) {

            // get buffer from sample (valid until sample is released)
            GstBuffer *buf = gst_sample_get_buffer (sample) ;

            // fill frame with buffer
            if ( !m->fill_frame(buf, Stream::SAMPLE) )
                ret = GST_FLOW_ERROR;
        }
    }
    else
        ret = GST_FLOW_FLUSHING;

    // release sample
    gst_sample_unref (sample);

    return ret;
}



Stream::TimeCounter::TimeCounter()
{
    timer = g_timer_new ();
}

Stream::TimeCounter::~TimeCounter()
{
    g_free(timer);
}

void Stream::TimeCounter::tic ()
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
