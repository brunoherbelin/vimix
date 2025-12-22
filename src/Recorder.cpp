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

#include "Recorder.h"

PNGRecorder::PNGRecorder(const std::string &basename) : FrameGrabber(), basename_(basename)
{
}

std::string PNGRecorder::init(GstCaps *caps)
{
    // ignore
    if (caps == nullptr)
        return std::string("Invalid caps");

    // create a gstreamer pipeline
    std::string description = "appsrc name=src ! videoconvert ! pngenc ! filesink name=sink";

    // parse pipeline descriptor
    GError *error = NULL;
    pipeline_ = gst_parse_launch (description.c_str(), &error);
    if (error != NULL) {
        std::string msg = std::string("PNG Capture Could not construct pipeline ") + description + "\n" + std::string(error->message);
        g_clear_error (&error);
        return msg;
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
        caps_ = gst_caps_copy( caps );
        gst_app_src_set_caps (src_, caps_);

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

void PNGRecorder::addFrame(GstBuffer *buffer, GstCaps *caps)
{
    FrameGrabber::addFrame(buffer, caps);

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
    "ProRes (Standard)",
    "ProRes (HQ 4444)",
    "WebM VP8 (Realtime)",
    "Multiple JPEG"
};

const std::vector<std::string> VideoRecorder::profile_description {
    // Control x264 encoder quality :
    // pass
    //    quant (4) – Constant Quantizer
    //    qual  (5) – Constant Quality
    // quantizer
    //   The total range is from 0 to 51, where 0 is lossless, 18 can be considered ‘visually lossless’,
    //   and 51 is terrible quality. A sane range is 18-26, and the default is 23.
    // speed-preset
    //    ultrafast (1)
    //    superfast (2)
    //    veryfast (3)
    //    faster (4)
    //    fast (5)
    "video/x-raw, format=I420 ! x264enc tune=\"zerolatency\" pass=4 quantizer=22 speed-preset=2 ! video/x-h264, profile=baseline ! h264parse ! ",
    "video/x-raw, format=Y444_10LE ! x264enc tune=\"zerolatency\" pass=4 quantizer=18 speed-preset=3 ! video/x-h264, profile=(string)high-4:4:4 ! h264parse ! ",
    // Control vah265enc encoder quality :
    //   target-usage : The target usage to control and balance the encoding speed/quality
    //                  The lower value has better quality but slower speed, the higher value has faster speed but lower quality.
    //                  Unsigned Integer. Range: 1 - 7 Default: 4 
    //   max-qp       : Maximum quantizer value for each frame
    //                  Unsigned Integer. Range: 0 - 51 Default: 51 
    //   rate-control : The desired rate control mode for the encoder
    //                            (2): cbr              - Constant Bitrate
    //                            (4): vbr              - Variable Bitrate
    //                            (16): cqp              - Constant Quantizer
    "video/x-raw, format=NV12 ! vah265enc rate-control=\"cqp\" target-usage=5 ! video/x-h265, profile=(string)main ! h265parse ! ",
    "video/x-raw, format=NV12 ! vah265enc rate-control=\"cqp\" max-qp=18  target-usage=2 ! video/x-h265, profile=(string)main ! h265parse ! ",
    // Apple ProRes encoding parameters
    //  pass
    //      cbr (0) – Constant Bitrate Encoding
    //      quant (2) – Constant Quantizer
    //      pass1 (512) – VBR Encoding - Pass 1
    //  profile
    //      0 ‘proxy’    45Mbps YUV 4:2:2
    //      1 ‘lt’       102Mbps YUV 4:2:2
    //      2 ‘standard’ 147Mbps YUV 4:2:2
    //      3 ‘hq’       220Mbps YUV 4:2:2
    //      4 ‘4444’     330Mbps YUVA 4:4:4:4
    //  quant-mat
    //      -1 auto
    //      0  proxy
    //      2  lt
    //      3  standard
    //      4  hq
    //      6  default
    "video/x-raw, format=I422_10LE ! avenc_prores_ks pass=2 bits_per_mb=8000 profile=2 quant-mat=6 quantizer=8 ! ",
    "video/x-raw, format=Y444_10LE ! avenc_prores_ks pass=2 bits_per_mb=8000 profile=4 quant-mat=6 quantizer=4 ! ",
    // VP8 WebM encoding
    //  deadline per frame (usec)
    //      0=best,
    //      1=realtime
    // see https://www.webmproject.org/docs/encoder-parameters/
    //        "vp8enc end-usage=cbr deadline=1 cpu-used=8 threads=4 target-bitrate=400000 undershoot=95 "
    //        "buffer-size=6000 buffer-initial-size=4000 buffer-optimal-size=5000 "
    //        "keyframe-max-dist=999999 min-quantizer=4 max-quantizer=50 ! ",
    "vp8enc end-usage=vbr deadline=1 cpu-used=8 threads=4 target-bitrate=400000 keyframe-max-dist=360 "
           "token-partitions=2 static-threshold=1000 min-quantizer=4 max-quantizer=20 ! ",
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
    "", "", "", ""
};

std::vector<std::string> nvidia_profile_description {
    // qp-const  Constant quantizer (-1 = from NVENC preset)
    //            Range: -1 - 51 Default: -1
    // rc-mode Rate Control Mode
    //    (0): default          - Default
    //    (1): constqp          - Constant Quantization
    //    (2): cbr              - Constant Bit Rate
    //    (3): vbr              - Variable Bit Rate
    //    (4): vbr-minqp        - Variable Bit Rate (with minimum quantization parameter, DEPRECATED)
    //    (5): cbr-ld-hq        - Low-Delay CBR, High Quality
    //    (6): cbr-hq           - CBR, High Quality (slower)
    //    (7): vbr-hq           - VBR, High Quality (slower)
    // Control nvh264enc encoder
    "nvh264enc rc-mode=1 zerolatency=true ! video/x-h264, profile=(string)main ! h264parse ! ",
    "nvh264enc rc-mode=1 qp-const=18 ! video/x-h264, profile=(string)high-4:4:4 ! h264parse ! ",
    // Control nvh265enc encoder
    "nvh265enc rc-mode=1 zerolatency=true ! video/x-h265, profile=(string)main ! h265parse ! ",
    "nvh265enc rc-mode=1 qp-const=18 ! video/x-h265, profile=(string)main ! h265parse ! ",
    "", "", "", ""
};

std::vector<std::string> vaapi_encoder = {
    "vaapih264enc",
    "vaapih264enc",
    "vaapih265enc",
    "vaapih265enc",
    "", "", "", ""
};

std::vector<std::string> vaapi_profile_description {

    // Control vaapih264enc encoder
    "vaapih264enc rate-control=cqp init-qp=26 ! video/x-h264, profile=(string)main ! h264parse ! ",
    "vaapih264enc rate-control=cqp init-qp=14 quality-level=4 keyframe-period=0 max-bframes=2 ! video/x-h264, profile=(string)high ! h264parse ! ",
    // Control vaapih265enc encoder
    "vaapih265enc ! video/x-h265, profile=(string)main ! h265parse ! ",
    "vaapih265enc rate-control=cqp init-qp=14 quality-level=4 keyframe-period=0 max-bframes=2 ! video/x-h265, profile=(string)main-444 ! h265parse ! ",
    "", "", "", ""
};

#elif GST_GL_HAVE_PLATFORM_CGL
// under CGL (Mac), gstreamer might have the VideoToolbox
std::vector<std::string> VideoRecorder::hardware_encoder = {
    "vtenc_h264_hw",
    "vtenc_h264_hw",
    "", "", "", "", "", ""
};

std::vector<std::string> VideoRecorder::hardware_profile_description {
    // Control vtenc_h264_hw encoder
    "video/x-raw, format=I420 ! vtenc_h264_hw realtime=1 allow-frame-reordering=0 ! h264parse ! ",
    "video/x-raw, format=UYVY ! vtenc_h264_hw realtime=1 allow-frame-reordering=0 quality=0.9 ! h264parse ! ",
    "", "", "", "", "", ""
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
    // Encoding in GPU is really beneficial only with gstreamer gst-gl
#ifdef USE_GST_OPENGL_SYNC_HANDLER
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
#endif
}

std::string VideoRecorder::init(GstCaps *caps)
{
    // ignore
    if (caps == nullptr)
        return std::string("Invalid caps");

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
        (int) hardware_encoder.size() > 0 &&
        GstToolkit::has_feature(hardware_encoder[Settings::application.record.profile])) {
        // glupload: system memory → GLMemory (in GStreamer's thread)
        // glcolorconvert: GPU color conversion (RGBA → NV12 for VAAPI, passthrough for NVIDIA)
        description += "glupload ! glcolorconvert ! ";
        Log::Info("Video Recording with glupload & GPU color conversion");
    } else
#endif
    {
        // CPU path: use regular videoconvert
        description += "videoconvert ! ";
    }

    description += "queue ! ";
    if (Settings::application.record.profile < 0 || Settings::application.record.profile >= DEFAULT)
        Settings::application.record.profile = H264_STANDARD;

    // test for a hardware accelerated encoder
    if (Settings::application.render.gpu_decoding && (int) hardware_encoder.size() > 0 &&
            GstToolkit::has_feature(hardware_encoder[Settings::application.record.profile]) ) {

        description += hardware_profile_description[Settings::application.record.profile];
        Log::Info("Video Recording with hardware accelerated encoder (%s)", description.c_str());
    }
    // revert to software encoder
    else {
        description += profile_description[Settings::application.record.profile];
        Log::Info("Video Recording with software encoder (%s)", description.c_str());
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

                Log::Info("Video Recording with audio (%s)", Audio::manager().pipeline(current_audio).c_str());

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

        // specify recorder framerate in the given caps
        GstCaps *tmp = gst_caps_copy( caps );
        GValue v = G_VALUE_INIT;
        g_value_init (&v, GST_TYPE_FRACTION);
        gst_value_set_fraction (&v, framerate_preset_value[Settings::application.record.framerate_mode], 1);
        gst_caps_set_value(tmp, "framerate", &v);
        g_value_unset (&v);

        // instruct src to use the caps
        caps_ = gst_caps_copy( tmp );
        gst_app_src_set_caps (src_, caps_);
        gst_caps_unref (tmp);

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

    return std::string("Video Recording starting ") + profile_name[Settings::application.record.profile];
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
        Log::Notify("Video Recording %s is ready.", filename_.c_str());
    }
    else
        Settings::application.recentRecordings.remove(filename_);
}

std::string VideoRecorder::info(bool extended) const
{
    if (extended)
        return filename();

    if (initialized_ && !active_ && !endofstream_)
        return "Saving file...";

    return FrameGrabber::info();
}
