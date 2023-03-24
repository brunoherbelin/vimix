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

#include <thread>
#include<algorithm> // for copy() and assign()
#include<iterator> // for back_inserter

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
#include "GstToolkit.h"
#include "SystemToolkit.h"
#include "Log.h"

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

    // start pipeline
    GstStateChangeReturn ret = gst_element_set_state (pipeline_, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        return std::string("PNG Capture : Failed to start frame grabber.");
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
    "H264 (High 4:4:4)",
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
    "video/x-raw, format=Y444_10LE ! x264enc pass=4 quantizer=18 speed-preset=3 ! video/x-h264, profile=(string)high-4:4:4 ! h264parse ! ",
    // Control x265 encoder quality :
    // NB: apparently x265 only accepts I420 format :(
    // speed-preset
    //    superfast (2)
    //    veryfast (3)
    //    faster (4)
    //    fast (5)
    // Tune
    //   psnr (1)
    //   ssim (2) DEFAULT
    //   grain (3)
    //   zerolatency (4)  Encoder latency is removed
    //   fastdecode (5)
    //   animation (6) optimize the encode quality for animation content without impacting the encode speed
    // crf Quality-controlled variable bitrate [0 51]
    //   default 28
    //   24 for x265 should be visually transparent; anything lower will probably just waste file size
    "video/x-raw, format=I420 ! x265enc tune=2 speed-preset=2 option-string=\"crf=24\" ! video/x-h265, profile=(string)main ! h265parse ! ",
    "video/x-raw, format=I420 ! x265enc tune=6 speed-preset=2 option-string=\"crf=12\" ! video/x-h265, profile=(string)main ! h265parse ! ",
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
    "video/x-raw, format=RGBA ! nvh264enc rc-mode=1 zerolatency=true ! video/x-h264, profile=(string)main ! h264parse ! ",
    "video/x-raw, format=RGBA ! nvh264enc rc-mode=1 qp-const=18 ! video/x-h264, profile=(string)high-4:4:4 ! h264parse ! ",
    // Control nvh265enc encoder
    "video/x-raw, format=RGBA ! nvh265enc rc-mode=1 zerolatency=true ! video/x-h265, profile=(string)main-10 ! h265parse ! ",
    "video/x-raw, format=RGBA ! nvh265enc rc-mode=1 qp-const=18 ! video/x-h265, profile=(string)main-444 ! h265parse ! ",
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
    "video/x-raw, format=NV12 ! vaapih264enc rate-control=cqp init-qp=26 ! video/x-h264, profile=(string)main ! h264parse ! ",
    "video/x-raw, format=NV12 ! vaapih264enc rate-control=cqp init-qp=14 quality-level=4 keyframe-period=0 max-bframes=2 ! video/x-h264, profile=(string)high ! h264parse ! ",
    // Control vaapih265enc encoder
    "video/x-raw, format=NV12 ! vaapih265enc ! video/x-h265, profile=(string)main ! h265parse ! ",
    "video/x-raw, format=NV12 ! vaapih265enc rate-control=cqp init-qp=14 quality-level=4 keyframe-period=0 max-bframes=2 ! video/x-h265, profile=(string)main-444 ! h265parse ! ",
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
}

std::string VideoRecorder::init(GstCaps *caps)
{
    // ignore
    if (caps == nullptr)
        return std::string("Invalid caps");

    // apply settings
    buffering_size_ = MAX( MIN_BUFFER_SIZE, buffering_preset_value[Settings::application.record.buffering_mode]);
    frame_duration_ = gst_util_uint64_scale_int (1, GST_SECOND, framerate_preset_value[Settings::application.record.framerate_mode]);
    timestamp_on_clock_ = Settings::application.record.priority_mode < 1;

    // create a gstreamer pipeline
    std::string description = "appsrc name=src ! videoconvert ! ";
    if (Settings::application.record.profile < 0 || Settings::application.record.profile >= DEFAULT)
        Settings::application.record.profile = H264_STANDARD;

    // test for a hardware accelerated encoder
    if (Settings::application.render.gpu_decoding && (int) hardware_encoder.size() > 0 &&
            GstToolkit::has_feature(hardware_encoder[Settings::application.record.profile]) ) {

        description += hardware_profile_description[Settings::application.record.profile];
        Log::Info("Video Recording using hardware accelerated encoder (%s)", hardware_encoder[Settings::application.record.profile].c_str());
    }
    // revert to software encoder
    else
        description += profile_description[Settings::application.record.profile];

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
    else if( Settings::application.record.profile == VP8) {
        // if sequencial file naming
        if (Settings::application.record.naming_mode == 0 )
            filename_ = SystemToolkit::filename_sequential(Settings::application.record.path, basename_, "webm");
        // or prefixed with date
        else
            filename_ = SystemToolkit::filename_dateprefix(Settings::application.record.path, basename_, "webm");

        description += "webmmux ! filesink name=sink";
    }
    else {
        // if sequencial file naming
        if (Settings::application.record.naming_mode == 0 )
            filename_ = SystemToolkit::filename_sequential(Settings::application.record.path, basename_, "mov");
        // or prefixed with date
        else
            filename_ = SystemToolkit::filename_dateprefix(Settings::application.record.path, basename_, "mov");

        description += "qtmux ! filesink name=sink";
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
                  "sync", FALSE,
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
        GValue v = { 0, };
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

    // start recording
    GstStateChangeReturn ret = gst_element_set_state (pipeline_, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        return std::string("Video Recording : Failed to start frame grabber.");
    }

    // all good
    initialized_ = true;

    return std::string("Video Recording started ") + profile_name[Settings::application.record.profile];

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

    // remember and inform
    Settings::application.recentRecordings.push(filename_);
    Log::Notify("Video Recording %s is ready.", filename_.c_str());
}

std::string VideoRecorder::info() const
{
    if (initialized_ && !active_ && !endofstream_)
        return "Saving file...";

    return FrameGrabber::info();
}
