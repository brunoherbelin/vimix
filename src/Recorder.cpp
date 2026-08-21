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


const char* VideoRecorder::profile_name[VideoRecorder::DEFAULT] = {
    "H264 (Realtime)",
    "H264 (HQ)",
    "H265 (Realtime)",
    "H265 (HQ)",
    "ProRes (Realtime)",
    "ProRes (HQ)",
    "WebM VP9 (Realtime)",
    "Multiple JPEG"
};


// There are two profiles of encoding: RT and HQ
// RT : "Real-time", Should encode live at 60 fps a 1080p video ; quality is at the highest considering this constraint.
// HQ : "High Quality", Should encode a 1080p video with visually lossless quality; encoding speed is less a constraint, but should still be live at 30fps
// 
// For each profile, several encoders are available, depending on the platform and the hardware acceleration available. 
// The user can select the encoder to use in the settings. Typically videos are encoded in H264 or H265
// Usual sofware encoders are x264 and x265. Fallback h264 encoder is openh264 if x264 is not available.
// If available, hardware accelerated encoders are used: NVENC (NVIDIA) or VAAPI (Intel/AMD) deoending on the platform and the GPU. 
// On MacOS, VideoToolbox is used for encoding.

std::vector<std::string> VideoRecorder::profile_description {
    "x264enc pass=qual quantizer=20 speed-preset=veryfast bframes=2 key-int-max=30 bitrate=25000 ! video/x-h264, profile=(string)main ! h264parse ! ",
    "x264enc pass=qual quantizer=18 speed-preset=medium bframes=3 key-int-max=30 bitrate=50000 ! video/x-h264, profile=(string)high ! h264parse ! ",
    "x265enc speed-preset=superfast tune=0 key-int-max=30 option-string=\"crf=20:vbv-maxrate=25000:vbv-bufsize=25000\" ! video/x-h265, profile=(string)main ! h265parse ! ",
    "x265enc speed-preset=medium tune=0 key-int-max=30 option-string=\"crf=18:vbv-maxrate=50000:vbv-bufsize=50000\" ! video/x-h265, profile=(string)main-444 ! h265parse ! ",
    "avenc_prores_ks pass=quant quantizer=8 profile=standard quant-mat=default threads=0 vendor=apl0 ! ",
    "avenc_prores_ks pass=quant quantizer=4 profile=hq quant-mat=default threads=0 vendor=apl0 ! ",
    "vp9enc end-usage=cq cq-level=24 target-bitrate=25000000 \
         deadline=1 cpu-used=6 lag-in-frames=0 keyframe-max-dist=30 threads=4 row-mt=true tile-columns=2 ! ",
    // JPEG encoding
    "jpegenc idct-method=float ! "
};


#if GST_GL_HAVE_PLATFORM_GLX

// under GLX (Linux), gstreamer might have nvidia or vaapi encoders
// the hardware encoder will be filled at first instanciation of VideoRecorder
std::vector<std::string> VideoRecorder::hardware_encoder;
std::vector<std::string> VideoRecorder::hardware_profile_description;

std::vector<std::string> nvidia_encoder = {
    "nvh264enc",
    "nvh264enc",
    "nvh265enc",
    "nvh265enc",
    "", "", "", 
    "nvjpegenc"
};

std::vector<std::string> nvidia_profile_description {
    // nvh264enc encoder
    "nvh264enc rc-mode=constqp preset=p4 bframes=2 gop-size=30 qp-const-i=23 qp-const-p=25 qp-const-b=27 ! video/x-h264, profile=(string)main ! h264parse ! ",
    "nvh264enc rc-mode=constqp preset=p6 bframes=3 rc-lookahead=16 b-adapt=true gop-size=30 ! video/x-h264, profile=(string)high ! h264parse ! ",
    // nvh265enc encoder
    "nvh265enc rc-mode=constqp preset=p4 bframes=2 gop-size=30 qp-const-i=23 qp-const-p=25 qp-const-b=27 ! video/x-h265, profile=(string)main ! h265parse ! ",
    "nvh265enc rc-mode=constqp preset=p6 bframes=3 rc-lookahead=16 b-adapt=true gop-size=30 qp-const-i=21 qp-const-p=23 qp-const-b=25 ! video/x-h265, profile=(string)main-444 ! h265parse ! ",
    "", "", "", 
    "nvjpegenc quality=85 ! "
};

std::vector<std::string> vaapi_encoder = {
    "vah264enc",
    "vah264enc",
    "vah265enc",
    "vah265enc",
    "", "", "", 
    "vajpegenc"
};

std::vector<std::string> vaapi_profile_description {
    // vah264enc encoder
    "vah264enc rate-control=cqp target-usage=2 key-int-max=30 qpi=24 qpp=26 qpb=28 ! video/x-h264 ! h264parse ! ",
    "vah264enc rate-control=cqp target-usage=4 key-int-max=30 qpi=22 qpp=24 qpb=26 ! video/x-h264 ! h264parse ! ",
    // vah265enc encoder
    "vah265enc rate-control=cqp target-usage=2 key-int-max=30 qpi=25 qpp=27 qpb=29 ! video/x-h265 ! h265parse ! ",
    "vah265enc rate-control=cqp target-usage=2 key-int-max=30 qpi=23 qpp=25 qpb=27 ! video/x-h265 ! h265parse ! ",
    "", "", "", 
    "vajpegenc quality=85 ! "
};

#elif GST_GL_HAVE_PLATFORM_CGL
// under CGL (Mac), gstreamer might have the VideoToolbox
std::vector<std::string> VideoRecorder::hardware_encoder = {
    "vtenc_h264_hw",
    "vtenc_h264_hw",
    "vtenc_h265_hw", 
    "vtenc_h265_hw", 
    "vtenc_prores", 
    "vtenc_prores", 
    "", ""
};

std::vector<std::string> VideoRecorder::hardware_profile_description {
    // Control vtenc_h264_hw encoder
    "vtenc_h264_hw realtime=1 allow-frame-reordering=0 quality=0.5 ! h264parse ! ",
    "vtenc_h264_hw realtime=1 allow-frame-reordering=0 quality=0.9 ! h264parse ! ",
    "vtenc_h265_hw realtime=1 allow-frame-reordering=0 quality=0.5 ! h265parse ! ",
    "vtenc_h265_hw realtime=1 allow-frame-reordering=0 quality=0.9 ! h265parse ! ",
    "vtenc_prores  realtime=1 allow-frame-reordering=0 quality=0.4 ! ", 
    "vtenc_prores  realtime=1 allow-frame-reordering=0 quality=0.9 ! ", 
    "", ""
};

#else
// in other platforms, no hardware encoder
std::vector<std::string> VideoRecorder::hardware_encoder;
std::vector<std::string> VideoRecorder::hardware_profile_description;
#endif



const char*   VideoRecorder::buffering_preset_name[6]  = { "Minimum", "100 MB", "200 MB", "500 MB", "1 GB", "2 GB" };
const guint64 VideoRecorder::buffering_preset_value[6] = { MIN_BUFFER_SIZE, 104857600, 209715200, 524288000, 1073741824, 2147483648 };

const char*   VideoRecorder::framerate_preset_name[3]  = { "15 FPS", "25 FPS", "30 FPS" };
const gint    VideoRecorder::framerate_preset_value[3] = { 15, 25, 30 };


VideoRecorder::VideoRecorder(const std::string &basename) : FrameGrabber(), basename_(basename)
{
    // first run initialization of hardware encoders in linux
#if GST_GL_HAVE_PLATFORM_GLX
    if (hardware_encoder.size() < 1) {
        // test nvidia encoder
        if ( GstToolkit::has_feature(nvidia_encoder[0] ) )   {
            // consider that if first nvidia encoder is valid, all others should also be available
            hardware_encoder.assign(nvidia_encoder.begin(), nvidia_encoder.end());
            hardware_profile_description.assign(nvidia_profile_description.begin(), nvidia_profile_description.end());
        }
        // test vaapi encoder
        else if ( GstToolkit::has_feature(vaapi_encoder[0] ) ) {
            hardware_encoder.assign(vaapi_encoder.begin(), vaapi_encoder.end());
            hardware_profile_description.assign(vaapi_profile_description.begin(), vaapi_profile_description.end());
        }
    }
#endif 

    if (!GstToolkit::has_feature("x264enc")) {
        // fallback if x264 is not available
        if (GstToolkit::has_feature("openh264enc")) {
            profile_description[0] = "openh264enc rate-control=quality qp-min=27 qp-max=27 gop-size=30 complexity=low "
                "slice-mode=auto multi-thread=0 enable-frame-skip=false ! video/x-h264 ! h264parse ! ";
            profile_description[1] = "openh264enc rate-control=quality qp-min=25 qp-max=25 gop-size=30 complexity=medium "
                "slice-mode=auto multi-thread=0 enable-frame-skip=false ! video/x-h264 ! h264parse ! ";
        }
        // disable h264 encoders if none available
        else {
            profile_description[0] = "";
            profile_description[1] = "";
        }
    }
    // disable h265 encoders if not available
    if (!GstToolkit::has_feature("x265enc")) {
        profile_description[2] = "";
        profile_description[3] = "";
    }

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

    // create a gstreamer pipeline
    std::string description = "appsrc name=src ! ";

#ifdef USE_GST_OPENGL_SYNC_HANDLER
    // Use glupload + glcolorconvert for hardware encoders
    // This uploads system memory to GPU and does color conversion in GPU shader
    if (Settings::application.render.gpu_decoding &&
        Settings::application.render.gst_glmemory_context &&
        (int) hardware_encoder.size() > 0 &&
        GstToolkit::has_feature("glupload") &&  
        GstToolkit::has_feature("glcolorconvert") &&  
        GstToolkit::has_feature("gltransformation") &&  
        GstToolkit::has_feature(hardware_encoder[Settings::application.record.profile])) {
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
    if (Settings::application.record.profile < 0 || Settings::application.record.profile >= DEFAULT)
        Settings::application.record.profile = H264_STANDARD;

    // test for a hardware accelerated encoder
    if (Settings::application.render.gpu_decoding && (int) hardware_encoder.size() > 0 &&
            GstToolkit::has_feature(hardware_encoder[Settings::application.record.profile]) ) {

        description += hardware_profile_description[Settings::application.record.profile];
        Log::Info("Video Recording : hardware accelerated encoder (%s)", description.c_str());
    }
    // revert to software encoder
    else {
        description += profile_description[Settings::application.record.profile];
        Log::Info("Video Recording : software encoder (%s)", description.c_str());
    }

    // setup muxer and prepare filename
    if( Settings::application.record.profile == JPEG_MULTI) {
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
                if ( Settings::application.record.profile == VP8)
                    description += "opusenc ! opusparse ! queue ! ";
                else
                    description += "avenc_aac ! aacparse ! queue ! ";

                Log::Info("Video Recording : audio (%s)", Audio::manager().pipeline(current_audio).c_str());
            }
        }

        if ( Settings::application.record.profile == VP8) {
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

    return std::string("Video Recording : starting ") + profile_name[Settings::application.record.profile];
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
        info += std::string(profile_name[Settings::application.record.profile]);
        return info;
    }

    if (initialized_ && !active_ && !endofstream_)
        return "Saving file...";

    return FrameGrabber::info();
}
