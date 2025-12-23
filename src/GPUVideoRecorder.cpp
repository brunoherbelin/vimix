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

#include "gst/gl/gstglformat.h"

#include <glad/glad.h>
#include <gst/gl/gl.h>
#include <gst/video/video.h>
#include <gst/gstcaps.h>

#include "GPUVideoRecorder.h"
#include "RenderingManager.h"
#include "Settings.h"
#include "MediaPlayer.h"
#include "Toolkit/GstToolkit.h"
#include "Toolkit/SystemToolkit.h"
#include "Log.h"

const char* GPUVideoRecorder::profile_name[GPUVideoRecorder::PROFILE_COUNT] = {
    "NVIDIA H264 (Realtime)",
    "NVIDIA H264 (HQ)",
    "NVIDIA H265 (Realtime)",
    "NVIDIA H265 (HQ)",
    "VAAPI H264 (Realtime)",
    "VAAPI H264 (HQ)",
    "VAAPI H265 (Realtime)",
    "VAAPI H265 (HQ)"
};

const char* GPUVideoRecorder::profile_encoder[GPUVideoRecorder::PROFILE_COUNT] = {
    "nvh264enc",
    "nvh264enc",
    "nvh265enc",
    "nvh265enc",
    "vaapih264enc",
    "vaapih264enc",
    "vaapih265enc",
    "vaapih265enc"
};

const gint GPUVideoRecorder::framerate_preset[3] = { 15, 25, 30 };


GPUVideoRecorder::GPUVideoRecorder(const std::string &basename)
    : FrameGrabber(),
      gl_context_(nullptr), gl_display_(nullptr),
      width_(0), height_(0), 
      profile_(NVENC_H264_REALTIME), basename_(basename)
{
}

GPUVideoRecorder::~GPUVideoRecorder()
{
    if (active_)
        stop();

    if (src_)
        gst_object_unref(src_);
    if (caps_)
        gst_caps_unref(caps_);
    if (gl_context_)
        gst_object_unref(gl_context_);
    if (gl_display_)
        gst_object_unref(gl_display_);
    if (timer_)
        gst_object_unref(timer_);
    if (pipeline_) {
        gst_element_set_state(pipeline_, GST_STATE_NULL);
        gst_object_unref(pipeline_);
    }
}

bool GPUVideoRecorder::isEncoderAvailable(Profile profile)
{
    return GstToolkit::has_feature(profile_encoder[profile]);
}

std::string GPUVideoRecorder::buildPipeline(Profile profile)
{
    std::string pipeline = "appsrc name=src ! glcolorconvert name=glclcvt ! ";

    // Build encoder-specific pipeline
    switch (profile) {
        case NVENC_H264_REALTIME:
            pipeline += "nvh264enc rc-mode=constqp zerolatency=true ! "
                       "video/x-h264, profile=main ! h264parse ! ";
            break;
        case NVENC_H264_HQ:
            pipeline += "nvh264enc rc-mode=constqp qp-const=18 ! "
                       "video/x-h264, profile=high ! h264parse ! ";
            break;
        case NVENC_H265_REALTIME:
            pipeline += "nvh265enc rc-mode=constqp zerolatency=true ! "
                       "video/x-h265, profile=main ! h265parse ! ";
            break;
        case NVENC_H265_HQ:
            pipeline += "nvh265enc rc-mode=constqp qp-const=18 ! "
                       "video/x-h265, profile=main-10 ! h265parse ! ";
            break;
        case VAAPI_H264_REALTIME:
            pipeline += "vaapih264enc rate-control=cqp init-qp=26 ! "
                       "video/x-h264, profile=main ! h264parse ! ";
            break;
        case VAAPI_H264_HQ:
            pipeline += "vaapih264enc rate-control=cqp init-qp=16 ! "
                       "video/x-h264, profile=high ! h264parse ! ";
            break;
        case VAAPI_H265_REALTIME:
            pipeline += "vaapih265enc rate-control=cqp init-qp=26 ! "
                       "video/x-h265, profile=main ! h265parse ! ";
            break;
        case VAAPI_H265_HQ:
            pipeline += "vaapih265enc rate-control=cqp init-qp=16 ! "
                       "video/x-h265, profile=high ! h265parse ! ";
            break;
        default:
            break;
    }

    // Add muxer and filesink
    pipeline += "qtmux ! filesink name=sink";

    return pipeline;
}

bool GPUVideoRecorder::isAvailable()
{
    // Check for at least one available GPU encoder
    for (int i = 0; i < PROFILE_COUNT; ++i) {
        if (isEncoderAvailable(static_cast<Profile>(i))) {
            return true;
        }
    }
    return false;
}

std::string GPUVideoRecorder::init(GstCaps *caps)
{
    // ignore
    if (caps == nullptr){
        return ("GPUVideoRecorder: Invalid Caps");
    }

    // set profile from settings
    if (Settings::application.record.profile >= 0 && Settings::application.record.profile < 4 ) {

        // try nvidia encoder 
        profile_ = static_cast<Profile>(Settings::application.record.profile);

        // test if hardware encoder is available
        if (!isEncoderAvailable(profile_)) {

            // try vaapi if nvidia encoder not available
            profile_ = static_cast<Profile>(Settings::application.record.profile + 4);

            // test if hardware encoder is available
            if (!isEncoderAvailable(profile_)) {
                return("GPUVideoRecorder: No GPU Encoder available (nvdec or vaapi).");
            }
        }
    } 
    else {
        return "GPUVideoRecorder: profile not available for GPU encoder (accepts only H264 and H265).";
    }

    // Validate GL context sharing is set up
    if (!Rendering::manager().global_gl_context || !Rendering::manager().global_display) {
        return "GPUVideoRecorder: OpenGL context sharing not initialized";
    }

    // Build pipeline
    std::string pipeline_desc = buildPipeline(profile_);

    // Parse pipeline
    GError *error = nullptr;
    pipeline_ = gst_parse_launch(pipeline_desc.c_str(), &error);
    if (error != nullptr) {
        std::string msg = std::string("GPUVideoRecorder: Could not construct pipeline ") + pipeline_desc + "\n" + std::string(error->message);
        g_clear_error(&error);
        return msg;
    }
    else  {
        Log::Info("GPUVideoRecorder pipeline:: %s", pipeline_desc.c_str());
    }

    // Generate filename
    if (Settings::application.record.naming_mode == 0)
        filename_ = SystemToolkit::filename_sequential(Settings::application.record.path, basename_, "mov");
    else
        filename_ = SystemToolkit::filename_dateprefix(Settings::application.record.path, basename_, "mov");

    // Configure filesink
    GstElement *sink = gst_bin_get_by_name(GST_BIN(pipeline_), "sink");
    if (sink) {
        g_object_set(G_OBJECT(sink), "location", filename_.c_str(), "sync", FALSE, nullptr);
        gst_object_unref(sink);
    } else {
        return "GPUVideoRecorder: Failed to find filesink element";
    }

    // Configure appsrc
    src_ = GST_APP_SRC(gst_bin_get_by_name(GST_BIN(pipeline_), "src"));
    if (!src_) {
        return "GPUVideoRecorder: Failed to find appsrc element";
    }

    // Set appsrc properties
    g_object_set(G_OBJECT(src_),
                 "is-live", TRUE,
                 "format", GST_FORMAT_TIME,
                 "do-timestamp", FALSE,  // We provide timestamps
                 nullptr);

    gst_app_src_set_stream_type(src_, GST_APP_STREAM_TYPE_STREAM);
    gst_app_src_set_latency(src_, -1, 0);
    gst_app_src_set_max_bytes(src_, 0);  // No buffering limit

    // Check and store caps dimensions
    GstStructure *structure = gst_caps_get_structure(caps, 0);
    gst_structure_get_int(structure, "width", &width_);
    gst_structure_get_int(structure, "height", &height_);    
    if (width_ <= 0 || height_ <= 0) {
        return "GPUVideoRecorder: Invalid video dimensions in caps";
    }

    // keep frame duration
    gint fps = MAXI(framerate_preset[Settings::application.record.framerate_mode], 15);
    frame_duration_ = gst_util_uint64_scale_int (1, GST_SECOND, fps);

    // specify recorder framerate in the given caps
    GstCaps *tmp = gst_caps_copy( caps );
    GValue v = G_VALUE_INIT;
    g_value_init (&v, GST_TYPE_FRACTION);
    gst_value_set_fraction (&v, fps, 1);
    gst_caps_set_value(tmp, "framerate", &v);
    g_value_unset (&v);
    
    // Create GLMemory caps from input caps
    caps_ = gst_caps_copy( tmp );
    GstCapsFeatures *features = gst_caps_features_new(GST_CAPS_FEATURE_MEMORY_GL_MEMORY, nullptr);
    gst_caps_set_features(caps_, 0, features);

    // instruct src to use the caps
    gst_app_src_set_caps(src_, caps_);
    gst_caps_unref (tmp);

    // Set callbacks
    GstAppSrcCallbacks callbacks;
    callbacks.need_data = GPUVideoRecorder::clbck_need_data;
    callbacks.enough_data = GPUVideoRecorder::clbck_enough_data;
    callbacks.seek_data = nullptr;
    gst_app_src_set_callbacks(src_, &callbacks, this, nullptr);

    // Set up bus handler
    GstBus *bus = gst_element_get_bus(pipeline_);
    gst_bus_set_sync_handler(bus, bus_sync_handler, this, nullptr);
    gst_object_unref(bus);

    // Start pipeline
    GstStateChangeReturn ret = gst_element_set_state(pipeline_, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        return "GPUVideoRecorder: Failed to start pipeline";
    }

    // Get GStreamer's GL context (it will be created when pipeline starts)
    // We'll get it on first frame when the context is ready

    // Initialize timer
    timer_ = gst_system_clock_obtain();
    timer_firstframe_ = 0;  // Will be set on first frame
    timestamp_ = 0;
    frame_count_ = 0;

    initialized_ = true;
    active_ = true;
    accept_buffer_ = false;
    finished_ = false;

    Log::Info("GPUVideoRecorder recording started: %s (%s)", filename_.c_str(), profile_name[profile_]);

    return "";
}

void GPUVideoRecorder::perform_texture_transfer(GstGLContext *context, gpointer data)
{
    TextureTransferData *transfer_data = static_cast<TextureTransferData *>(data);
    GPUVideoRecorder *grabber = transfer_data->grabber;
    GstBuffer *buffer = transfer_data->buffer;
    guint vimix_texture = transfer_data->vimix_texture_id;

    // Get the GLMemory from the buffer
    GstMemory *mem = gst_buffer_peek_memory(buffer, 0);
    if (!gst_is_gl_memory(mem)) {
        Log::Warning("GPU Recording: Buffer does not contain GLMemory");
        return;
    }

    GstGLMemory *gl_mem = (GstGLMemory *)mem;
    guint gst_texture = gst_gl_memory_get_texture_id(gl_mem);

    // Perform FBO blit to transfer texture
    GLuint fbo;
    glGenFramebuffers(1, &fbo);

    // Bind source texture (from vimix) to read framebuffer
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                          GL_TEXTURE_2D, vimix_texture, 0);

    // Bind destination texture (GStreamer GLMemory) to draw framebuffer
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT1,
                          GL_TEXTURE_2D, gst_texture, 0);

    // Set read/draw buffers
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glDrawBuffer(GL_COLOR_ATTACHMENT1);

    // Blit
    glBlitFramebuffer(0, 0, grabber->width_, grabber->height_,
                     0, 0, grabber->width_, grabber->height_,
                     GL_COLOR_BUFFER_BIT, GL_NEAREST);

    // Cleanup
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);

    // Check for errors
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        Log::Warning("GPU Recording: OpenGL error during texture transfer: 0x%x", err);
    }
}

void GPUVideoRecorder::addFrame(guint texture_id, GstCaps *caps)
{
    if (caps == nullptr)
        return;

    if (!initialized_) {
        // init on first frame
        std::string msg = init(caps);
        if (!msg.empty()) {
            finished_ = true;
            Log::Warning("GPUVideoRecorder initialization failed: %s", msg.c_str());
            return;
        }
    }

    // stop if an incompatilble caps given after initialization
    gint width, height;
    GstStructure *structure = gst_caps_get_structure(caps, 0);
    gst_structure_get_int(structure, "width", &width);
    gst_structure_get_int(structure, "height", &height);    
    if (width_ != width || height_ != height) {
        Log::Warning("GPUVideoRecorder: interrupted because the resolution changed");
        stop();
        return;
    }

    // start processing
    if (!active_ || !accept_buffer_ || pause_)
        return;

    // current time
    GstClockTime now = gst_clock_get_time(timer_);

    // Initialize timer on first frame
    if (timer_firstframe_ == 0) 
        timer_firstframe_ = now;

    // if returning from pause, time of pause was stored
    if (timer_pauseframe_ > 0) {
        // compute duration of the pausing time and add to total pause duration
        pause_duration_ += gst_clock_get_time(timer_) - timer_pauseframe_;

        // // sync audio packets
        // GstElement *audiosync = GST_ELEMENT_CAST(gst_bin_get_by_name(GST_BIN(pipeline_), "audiosync"));
        // if (audiosync)
        //     g_object_set(G_OBJECT(audiosync), "ts-offset", -timer_pauseframe_, NULL);

        // reset pause frame time
        timer_pauseframe_ = 0;
    }

    // skip frame if arriving too early (except first frame at timestamp 0)
    if ( timestamp_ > 0 && (now - timer_firstframe_ - pause_duration_ - timestamp_) < (frame_duration_ - (frame_duration_/10)) ) {
        return; 
    }

    // calculate current timestamp
    timestamp_ = now - timer_firstframe_ - pause_duration_;

    // Get or create GStreamer GL context on first frame
    if (!gl_context_) {
        // Query the pipeline for GL context
        GstElement *glcolorconvert = gst_bin_get_by_name(GST_BIN(pipeline_), "glclcvt");
        if (glcolorconvert) {
            g_object_get(glcolorconvert, "context", &gl_context_, nullptr);
            gst_object_unref(glcolorconvert);
        }

        if (!gl_context_) {
            Log::Warning("GPUVideoRecorder: Could not get GL context from pipeline");
            return;
        }
    }

    // Allocate GLMemory buffer
    GstGLMemoryAllocator *allocator = gst_gl_memory_allocator_get_default(gl_context_);
    if (!allocator) {
        Log::Warning("GPUVideoRecorder: Failed to get GL memory allocator");
        return;
    }

    // Extract video info from caps
    GstVideoInfo v_info;
    if (!gst_video_info_from_caps(&v_info, caps_)) {
        Log::Warning("GPUVideoRecorder: Failed to parse video info from caps");
        return;
    }

    GstGLVideoAllocationParams *params = gst_gl_video_allocation_params_new(
        gl_context_,
        nullptr,  // allocation params
        &v_info,
        0,  // plane
        nullptr,  // valign
        GST_GL_TEXTURE_TARGET_2D,
        GST_GL_RGB8
    );

    GstGLBaseMemory *gl_mem = gst_gl_base_memory_alloc(
        (GstGLBaseMemoryAllocator *)allocator,
        (GstGLAllocationParams *)params
    );

    gst_gl_allocation_params_free((GstGLAllocationParams *)params);

    GstMemory *mem = (GstMemory *)gl_mem;

    if (!mem) {
        Log::Warning("GPUVideoRecorder: Failed to allocate GL memory");
        return;
    }

    // Create buffer and attach memory
    GstBuffer *buffer = gst_buffer_new();
    gst_buffer_append_memory(buffer, mem);

    // Transfer texture from vimix to GStreamer in GL thread
    TextureTransferData transfer_data;
    transfer_data.grabber = this;
    transfer_data.vimix_texture_id = texture_id;
    transfer_data.buffer = buffer;

    gst_gl_context_thread_add(gl_context_, perform_texture_transfer, &transfer_data);

    // Set buffer timestamp
    GST_BUFFER_PTS(buffer) = timestamp_;
    GST_BUFFER_DURATION(buffer) = frame_duration_;

    //Push buffer to appsrc
    // gst_buffer_ref(buffer);
    GstFlowReturn ret = gst_app_src_push_buffer(src_, buffer);
    if (ret != GST_FLOW_OK) {
        Log::Warning("GPUVideoRecorder: Failed to push buffer: %s", gst_flow_get_name(ret));
        if (ret == GST_FLOW_EOS || ret == GST_FLOW_FLUSHING) {
            active_ = false;
        }
    }

    frame_count_++;
}

void GPUVideoRecorder::stop()
{
    if (!active_)
        return;

    active_ = false;

    // Send EOS
    if (pipeline_) {
        gst_element_send_event(pipeline_, gst_event_new_eos());
        endofstream_ = true;
    }

    Log::Info("GPUVideoRecorder: %llu frames recorded", (unsigned long long)frame_count_);
}

uint64_t GPUVideoRecorder::duration() const
{
    return GST_TIME_AS_MSECONDS(timestamp_);
}

void GPUVideoRecorder::terminate()
{
    // remember and inform if valid
    std::string uri = GstToolkit::filename_to_uri(filename_);
    MediaInfo media = MediaPlayer::UriDiscoverer(uri);
    if (media.valid && !media.isimage) {
        Settings::application.recentRecordings.push(filename_);
        Log::Notify("Video Recording %s is ready.", filename_.c_str());
    }
    else
        Settings::application.recentRecordings.remove(filename_);
}

std::string GPUVideoRecorder::info(bool extended) const
{
    if (extended) {
        std::string info = "Recorded ";
        info += std::to_string(frame_count_) + " frames\n";
        info +=  profile_name[profile_];
        return info;
    }

    if (!initialized_)
        return "Initializing";
    if (initialized_ && !active_ && endofstream_)
        return "Saving file...";    
    if (active_)
        return GstToolkit::time_to_string(timestamp_);
    
    return "Inactive";
}

void GPUVideoRecorder::clbck_need_data(GstAppSrc *, guint, gpointer user_data)
{
    GPUVideoRecorder *grabber = static_cast<GPUVideoRecorder *>(user_data);
    if (grabber)
        grabber->accept_buffer_ = true;
}

void GPUVideoRecorder::clbck_enough_data(GstAppSrc *, gpointer user_data)
{
    GPUVideoRecorder *grabber = static_cast<GPUVideoRecorder *>(user_data);
    if (grabber)
        grabber->accept_buffer_ = false;
}

GstBusSyncReply GPUVideoRecorder::bus_sync_handler(GstBus *, GstMessage *msg, gpointer user_data)
{
    GPUVideoRecorder *grabber = static_cast<GPUVideoRecorder *>(user_data);

    // Handle GL context requests
    if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_NEED_CONTEXT) {
        const gchar *contextType;
        gst_message_parse_context_type(msg, &contextType);

        // Provide GL display context
        if (!g_strcmp0(contextType, GST_GL_DISPLAY_CONTEXT_TYPE) &&
            Rendering::manager().global_display) {
            GstContext *displayContext = gst_context_new(GST_GL_DISPLAY_CONTEXT_TYPE, TRUE);
            gst_context_set_gl_display(displayContext, Rendering::manager().global_display);
            gst_element_set_context(GST_ELEMENT(msg->src), displayContext);
            gst_context_unref(displayContext);
        }

        // Provide GL app context
        if (!g_strcmp0(contextType, "gst.gl.app_context") &&
            Rendering::manager().global_gl_context) {
            GstContext *appContext = gst_context_new("gst.gl.app_context", TRUE);
            GstStructure *structure = gst_context_writable_structure(appContext);
            gst_structure_set(structure, "context", GST_TYPE_GL_CONTEXT,
                            Rendering::manager().global_gl_context, nullptr);
            gst_element_set_context(GST_ELEMENT(msg->src), appContext);
            gst_context_unref(appContext);
        }
    }

    // Handle errors
    if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ERROR) {
        GError *error;
        gst_message_parse_error(msg, &error, nullptr);
        Log::Warning("GPU Recording Error: %s", error->message);
        g_error_free(error);

        if (grabber)
            grabber->active_ = false;
    }

    // Handle EOS
    if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_EOS) {
        if (grabber) {
            grabber->finished_ = true;
            Log::Notify("GPU Recording ready: %s", grabber->filename_.c_str());
        }
    }

    return GST_BUS_DROP;
}

