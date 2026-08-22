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


//  Desktop OpenGL function loader
#include <glad/glad.h>

// standalone image loader
#include <stb_image.h>
#include <stb_image_write.h>

// gstreamer
#include <gst/gl/gl.h>
#include <gst/gstformat.h>
#include <gst/video/video.h>

#include "Settings.h"
#include "Toolkit/GstToolkit.h"
#include "Toolkit/SystemToolkit.h"
#include "MediaPlayer.h"
#include "Log.h"
#include "Audio.h"
#include "gst/gstcaps.h"

#include "Recorder.h"

PNGRecorder::PNGRecorder(const std::string &basename) : FrameGrabber(), basename_(basename)
{
}

std::string PNGRecorder::init(GstCaps *read_caps, GstCaps *write_caps)
{
    // ignore
    if (read_caps == nullptr)
        return std::string("Invalid caps");

    // create a gstreamer pipeline
    std::string description = "appsrc name=src ! videoconvert ! videoscale ! capsfilter name=capf ! pngenc ! filesink name=sink";

    // parse pipeline descriptor
    GError *error = NULL;
    pipeline_ = gst_parse_launch (description.c_str(), &error);
    if (error != NULL) {
        std::string msg = std::string("PNG Capture Could not construct pipeline ") + description + "\n" + std::string(error->message);
        g_clear_error (&error);
        return msg;
    }

    // setup capsfilter for scaling to write_caps
    GstElement *capsfilter = gst_bin_get_by_name (GST_BIN (pipeline_), "capf");
    if (capsfilter) {
        g_object_set (G_OBJECT (capsfilter), "caps", write_caps, NULL);
        gst_object_unref (capsfilter);
    }

    // construct filename:
    // if sequencial file naming
    if (Settings::application.record.naming_mode == 0 )
        filename_ = SystemToolkit::filename_sequential(Settings::application.record.path, basename_, "png");
    // or prefixed with date
    else
        filename_ = SystemToolkit::filename_dateprefix(Settings::application.record.path, basename_, "png");

    // setup file sink
    g_object_set (G_OBJECT (gst_bin_get_by_name (GST_BIN (pipeline_), "sink")),
                  "location", filename_.c_str(),
                  "sync", FALSE,
                  NULL);

    // setup custom app source
    src_ = GST_APP_SRC( gst_bin_get_by_name (GST_BIN (pipeline_), "src") );
    if (src_) {

        g_object_set (G_OBJECT (src_),
                      "is-live", TRUE,
                      NULL);

        // configure stream
        gst_app_src_set_stream_type( src_, GST_APP_STREAM_TYPE_STREAM);
        gst_app_src_set_latency( src_, -1, 0);

        // Direct encoding (no buffering)
        gst_app_src_set_max_bytes( src_, 0 );

        // instruct src to use the required caps
        read_caps_ = gst_caps_copy( read_caps );
        gst_app_src_set_caps (src_, read_caps_);

        // setup callbacks
        GstAppSrcCallbacks callbacks;
        callbacks.need_data = FrameGrabber::callback_need_data;
        callbacks.enough_data = FrameGrabber::callback_enough_data;
        callbacks.seek_data = NULL; // stream type is not seekable
        gst_app_src_set_callbacks (src_, &callbacks, this, NULL);

    }
    else {
        return std::string("PNG Capture : Failed to configure frame grabber.");
    }
 

    // all good
    initialized_ = true;

    return std::string("PNG Capture started ");
}

void PNGRecorder::terminate()
{
    // remember and inform
    Settings::application.recentRecordings.push(filename_);
    Log::Notify("PNG Capture %s is ready.", filename_.c_str());
}

void PNGRecorder::addFrame(GstBuffer *buffer, GstCaps *read_caps, GstCaps *write_caps)
{
    FrameGrabber::addFrame(buffer, read_caps, write_caps);

    // PNG Recorder specific :
    // stop after one frame
    if (frame_count_ > 0) {
        stop();
    }
}


const char*   VideoRecorder::buffering_preset_name[6]  = { "Minimum", "100 MB", "200 MB", "500 MB", "1 GB", "2 GB" };
const guint64 VideoRecorder::buffering_preset_value[6] = { MIN_BUFFER_SIZE, 104857600, 209715200, 524288000, 1073741824, 2147483648 };

const char*   VideoRecorder::framerate_preset_name[3]  = { "15 FPS", "25 FPS", "30 FPS" };
const gint    VideoRecorder::framerate_preset_value[3] = { 15, 25, 30 };


VideoRecorder::VideoRecorder(const std::string &basename) : FrameGrabber(), basename_(basename)
{
}

std::string VideoRecorder::init(GstCaps *read_caps, GstCaps *write_caps)
{
    // ignore
    if (read_caps == nullptr || write_caps == nullptr)
        return std::string("Invalid caps");

    // specify recorder framerate in the read caps
    read_caps_ = gst_caps_copy( read_caps );
    GValue v = G_VALUE_INIT;
    g_value_init (&v, GST_TYPE_FRACTION);
    gst_value_set_fraction (&v, framerate_preset_value[Settings::application.record.framerate_mode], 1);
    gst_caps_set_value(read_caps_, "framerate", &v);
    g_value_unset (&v);

    // set write caps
    write_caps_ = gst_caps_copy( write_caps );        

    // apply settings
    buffering_size_ = MAX( MIN_BUFFER_SIZE, buffering_preset_value[Settings::application.record.buffering_mode]);
    frame_duration_ = gst_util_uint64_scale_int (1, GST_SECOND, MAXI(framerate_preset_value[Settings::application.record.framerate_mode], 15));
    timestamp_on_clock_ = Settings::application.record.priority_mode < 1;
    keyframe_count_ = framerate_preset_value[Settings::application.record.framerate_mode];

    // clamp profile and resolve its hardware encoder pipeline (if any) once,
    // used both to decide the GL upload path below and the encoder itself
    if (Settings::application.record.profile < 0 || Settings::application.record.profile >= GstToolkit::DEFAULT)
        Settings::application.record.profile = GstToolkit::H264_RT;
    GstToolkit::Profile profile = (GstToolkit::Profile) Settings::application.record.profile;
    std::string hardware_pipeline = GstToolkit::getHardwareEncodingPipeline(profile);

    // create a gstreamer pipeline
    std::string description = "appsrc name=src ! ";

#ifdef USE_GST_OPENGL_SYNC_HANDLER
    // Use glupload + glcolorconvert for hardware encoders
    // This uploads system memory to GPU and does color conversion in GPU shader
    if (Settings::application.render.gpu_decoding &&
        Settings::application.render.gst_glmemory_context &&
        !hardware_pipeline.empty() &&
        GstToolkit::has_feature("glupload") &&
        GstToolkit::has_feature("glcolorconvert") &&
        GstToolkit::has_feature("gltransformation")) {
        // glupload: system memory → GLMemory (in GStreamer's thread)
        // glcolorconvert: GPU color conversion (RGBA → NV12 for VAAPI, passthrough for NVIDIA)
        description += "glupload ! glcolorconvert ! gltransformation ! capsfilter name=capf ! ";
        // specify that write caps are in GLMemory
        GstCapsFeatures *features = gst_caps_features_new(GST_CAPS_FEATURE_MEMORY_GL_MEMORY, nullptr);
        gst_caps_set_features(write_caps_, 0, features);
        Log::Info("Video Recording : using pure openGL pipeline.");
    } else
#endif
    {
        // CPU path: use regular videoconvert
        description += "videoconvert n-threads=0 ! videoscale ! capsfilter name=capf ! ";
    }

    description += "queue ! ";

    // test for a hardware accelerated encoder
    if (Settings::application.render.gpu_decoding && !hardware_pipeline.empty()) {
        description += hardware_pipeline;
        Log::Info("Video Recording : hardware accelerated encoder (%s)", description.c_str());
    }
    // revert to software encoder
    else {
        description += GstToolkit::getEncodingPipeline(profile);
        Log::Info("Video Recording : software encoder (%s)", description.c_str());
    }

    // setup muxer and prepare filename
    if( Settings::application.record.profile == GstToolkit::JPEG_MULTI) {
        std::string folder = SystemToolkit::filename_dateprefix(Settings::application.record.path, basename_, "");
        if (SystemToolkit::create_directory(folder)) {
            filename_ = SystemToolkit::full_filename(folder, "%05d.jpg");
            description += "multifilesink name=sink";
        }
        else
            return std::string("Video Recording : Failed to create folder ") + folder;
    }
    else {

        // Add Audio to pipeline
        if ( Settings::application.accept_audio &&
            !Settings::application.record.audio_device.empty()) {
            // ensure the Audio manager has the device specified in settings
            int current_audio = Audio::manager().index(Settings::application.record.audio_device);
            if (current_audio > -1) {
                description += "mux. ";
                description += Audio::manager().pipeline(current_audio);
                description += " ! audio/x-raw ! audioconvert ! audioresample ! ";
                description += "identity name=audiosync ! ";
                // select encoder depending on codec
                if ( Settings::application.record.profile == GstToolkit::VPX_RT)
                    description += "opusenc ! opusparse ! queue ! ";
                else
                    description += "avenc_aac ! aacparse ! queue ! ";

                Log::Info("Video Recording : audio (%s)", Audio::manager().pipeline(current_audio).c_str());
            }
        }

        if ( Settings::application.record.profile == GstToolkit::VPX_RT) {
            // if sequencial file naming
            if (Settings::application.record.naming_mode == 0 )
                filename_ = SystemToolkit::filename_sequential(Settings::application.record.path, basename_, "webm");
            // or prefixed with date
            else
                filename_ = SystemToolkit::filename_dateprefix(Settings::application.record.path, basename_, "webm");

            description += "webmmux name=mux ! filesink name=sink";
        }
        else {
            // if sequencial file naming
            if (Settings::application.record.naming_mode == 0 )
                filename_ = SystemToolkit::filename_sequential(Settings::application.record.path, basename_, "mov");
            // or prefixed with date
            else
                filename_ = SystemToolkit::filename_dateprefix(Settings::application.record.path, basename_, "mov");

            description += "qtmux name=mux ! filesink name=sink";
        }
    }

    // parse pipeline descriptor
    GError *error = NULL;
    pipeline_ = gst_parse_launch (description.c_str(), &error);
    if (error != NULL) {
        std::string msg = std::string("Video Recording : Could not construct pipeline ") + description + "\n" + std::string(error->message);
        g_clear_error (&error);
        return msg;
    }

    // setup video capsfilter for sink
    GstElement *capsfilter = gst_bin_get_by_name (GST_BIN (pipeline_), "capf");
    if (capsfilter) {
        g_object_set (G_OBJECT (capsfilter), "caps", write_caps_, NULL);
        gst_object_unref (capsfilter);
    }

    // setup file sink
    g_object_set (G_OBJECT (gst_bin_get_by_name (GST_BIN (pipeline_), "sink")),
                  "location", filename_.c_str(),
                  "sync", TRUE,
                  NULL);

    // setup custom app source
    src_ = GST_APP_SRC( gst_bin_get_by_name (GST_BIN (pipeline_), "src") );
    if (src_) {

        g_object_set (G_OBJECT (src_),
                      "is-live", TRUE,
                      "format", GST_FORMAT_TIME,
                      NULL);

        if (timestamp_on_clock_)
            g_object_set (G_OBJECT (src_),"do-timestamp", TRUE,NULL);

        // configure stream
        gst_app_src_set_stream_type( src_, GST_APP_STREAM_TYPE_STREAM);
        gst_app_src_set_latency( src_, -1, 0);

        // Set buffer size
        gst_app_src_set_max_bytes( src_, buffering_size_);

        // instruct src to use the caps
        gst_app_src_set_caps (src_, read_caps_);

        // setup callbacks
        GstAppSrcCallbacks callbacks;
        callbacks.need_data = FrameGrabber::callback_need_data;
        callbacks.enough_data = FrameGrabber::callback_enough_data;
        callbacks.seek_data = NULL; // stream type is not seekable
        gst_app_src_set_callbacks (src_, &callbacks, this, NULL);

    }
    else {
        return std::string("Video Recording : Failed to configure frame grabber.");
    }

    // Enforce a system clock for the recording pipeline
    // (this allows keeping pipeline in synch when recording both
    //  video and audio - the automatic clock default chooses either
    //  the video or the audio source, which cause synch problems)
    gst_pipeline_use_clock( GST_PIPELINE(pipeline_), gst_system_clock_obtain());

    // all good
    initialized_ = true;

    return std::string("Video Recording : starting ") + GstToolkit::profile_name[Settings::application.record.profile];
}

void VideoRecorder::terminate()
{
    // stop the pipeline (again)
    gst_element_set_state (pipeline_, GST_STATE_NULL);

    // statistics on expected number of frames
    guint64 N = MAX( (guint64) duration_ / (guint64) frame_duration_, frame_count_);
    float loss = 100.f * ((float) (N - frame_count_) ) / (float) N;
    Log::Info("Video Recording : %ld frames captured in %s (aming for %ld, %.0f%% lost)",
              frame_count_, GstToolkit::time_to_string(duration_, GstToolkit::TIME_STRING_READABLE).c_str(), N, loss);

    // warn user if more than 10% lost
    if (loss > 10.f) {
        if (timestamp_on_clock_)
            Log::Warning("Video Recording lost %.0f%% of frames: framerate could not be maintained at %ld FPS.", loss, GST_SECOND / frame_duration_);
        else
            Log::Warning("Video Recording lost %.0f%% of frames: video is only %s long.",
                         loss, GstToolkit::time_to_string(timestamp_, GstToolkit::TIME_STRING_READABLE).c_str());
        Log::Info("Video Recording : try a lower resolution / a lower framerate / a larger buffer size / a faster codec.");
    }

    // remember and inform if valid
    std::string uri = GstToolkit::filename_to_uri(filename_);
    MediaInfo media = MediaPlayer::UriDiscoverer(uri);
    if (media.valid && !media.isimage) {
        Settings::application.recentRecordings.push(filename_);
        Log::Notify("Video Recording : %s is ready.", filename_.c_str());
    }
    else
        Settings::application.recentRecordings.remove(filename_);
}

std::string VideoRecorder::info(bool extended) const
{
    if (extended) {
        std::string info = "Recorded ";
        info += std::to_string(frame_count_) + " frames\n";
        info += std::to_string(buffering()) + "% Buffer used\n";
        info += std::string(GstToolkit::profile_name[Settings::application.record.profile]);
        return info;
    }

    if (initialized_ && !active_ && !endofstream_)
        return "Saving file...";

    return FrameGrabber::info();
}
