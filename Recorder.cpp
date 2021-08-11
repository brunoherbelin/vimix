#include <thread>

//  Desktop OpenGL function loader
#include <glad/glad.h>

// standalone image loader
#include <stb_image.h>
#include <stb_image_write.h>

// gstreamer
#include <gst/gstformat.h>
#include <gst/video/video.h>

#include "Settings.h"
#include "GstToolkit.h"
#include "defines.h"
#include "SystemToolkit.h"
#include "FrameBuffer.h"
#include "Log.h"

#include "Recorder.h"

PNGRecorder::PNGRecorder() : FrameGrabber()
{
}

void PNGRecorder::init(GstCaps *caps)
{
    // ignore
    if (caps == nullptr)
        return;

    // create a gstreamer pipeline
    std::string description = "appsrc name=src ! videoconvert ! pngenc ! filesink name=sink";

    // parse pipeline descriptor
    GError *error = NULL;
    pipeline_ = gst_parse_launch (description.c_str(), &error);
    if (error != NULL) {
        Log::Warning("PNG Capture Could not construct pipeline %s:\n%s", description.c_str(), error->message);
        g_clear_error (&error);
        finished_ = true;
        return;
    }

    // verify location path (path is always terminated by the OS dependent separator)
    std::string path = SystemToolkit::path_directory(Settings::application.record.path);
    if (path.empty())
        path = SystemToolkit::home_path();
    filename_ = path + "vimix_" + SystemToolkit::date_time_string() + ".png";

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
        Log::Warning("PNG Capture Could not configure source");
        finished_ = true;
        return;
    }

    // start pipeline
    GstStateChangeReturn ret = gst_element_set_state (pipeline_, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        Log::Warning("PNG Capture Could not record %s", filename_.c_str());
        finished_ = true;
        return;
    }

    // all good
    Log::Info("PNG Capture started.");

    // start recording !!
    active_ = true;
}

void PNGRecorder::terminate()
{
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
    "H265 (HQ Animation)",
    "ProRes (Standard)",
    "ProRes (HQ 4444)",
    "WebM VP8 (2MB/s)",
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
    //    veryfast (3)
    //    faster (4)
    //    fast (5)
#ifndef APPLE
    //    "video/x-raw, format=I420 ! x264enc pass=4 quantizer=26 speed-preset=3 threads=4 ! video/x-h264, profile=baseline ! h264parse ! ",
    "video/x-raw, format=I420 ! x264enc tune=\"zerolatency\" pass=4 threads=4 ! video/x-h264, profile=baseline ! h264parse ! ",
#else
    "video/x-raw, format=I420 ! vtenc_h264_hw realtime=1 allow-frame-reordering=0 ! h264parse ! ",
#endif
    "video/x-raw, format=Y444_10LE ! x264enc pass=4 quantizer=16 speed-preset=4 threads=4 ! video/x-h264, profile=(string)high-4:4:4 ! h264parse ! ",
    // Control x265 encoder quality :
    // NB: apparently x265 only accepts I420 format :(
    // speed-preset
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
    // default 28
    // 24 for x265 should be visually transparent; anything lower will probably just waste file size
    "video/x-raw, format=I420 ! x265enc tune=4 speed-preset=3 ! video/x-h265, profile=(string)main ! h265parse ! ",
    "video/x-raw, format=I420 ! x265enc tune=6 speed-preset=4 option-string=\"crf=24\" ! video/x-h265, profile=(string)main ! h265parse ! ",
    // Apple ProRes encoding parameters
    //  pass
    //      cbr (0) – Constant Bitrate Encoding
    //      quant (2) – Constant Quantizer
    //      pass1 (512) – VBR Encoding - Pass 1
    //  profile
    //      0 ‘proxy’
    //      1 ‘lt’
    //      2 ‘standard’
    //      3 ‘hq’
    //      4 ‘4444’
    "video/x-raw, format=I422_10LE ! avenc_prores_ks pass=2 profile=2 quantizer=26 ! ",
    "video/x-raw, format=Y444_10LE ! avenc_prores_ks pass=2 profile=4 quantizer=12 ! ",
    // VP8 WebM encoding
    "vp8enc end-usage=vbr cpu-used=8 max-quantizer=35 deadline=100000 target-bitrate=200000 keyframe-max-dist=360 token-partitions=2 static-threshold=100 ! ",
    "jpegenc ! "
};

// Too slow
//// WebM VP9 encoding parameters
//// https://www.webmproject.org/docs/encoder-parameters/
//// https://developers.google.com/media/vp9/settings/vod/
//"vp9enc end-usage=vbr end-usage=vbr cpu-used=3 max-quantizer=35 target-bitrate=200000 keyframe-max-dist=360 token-partitions=2 static-threshold=1000 ! "

// FAILED
// x265 encoder quality
//       string description = "appsrc name=src ! videoconvert ! "
//               "x265enc tune=4 speed-preset=2 option-string='crf=28' ! h265parse ! "
//               "qtmux ! filesink name=sink";


const char*   VideoRecorder::buffering_preset_name[6]  = { "30 MB", "100 MB", "200 MB", "500 MB", "1 GB", "2 GB" };
const guint64 VideoRecorder::buffering_preset_value[6] = { MIN_BUFFER_SIZE, 104857600, 209715200, 524288000, 1073741824, 2147483648 };

const char*   VideoRecorder::framerate_preset_name[3]  = { "15 FPS", "25 FPS", "30 FPS" };
const gint    VideoRecorder::framerate_preset_value[3] = { 15, 25, 30 };


VideoRecorder::VideoRecorder() : FrameGrabber()
{

}

void VideoRecorder::init(GstCaps *caps)
{
    // ignore
    if (caps == nullptr)
        return;

    // apply settings
    buffering_size_ = MAX( MIN_BUFFER_SIZE, buffering_preset_value[Settings::application.record.buffering_mode]);
    frame_duration_ = gst_util_uint64_scale_int (1, GST_SECOND, framerate_preset_value[Settings::application.record.framerate_mode]);
    timestamp_on_clock_ = Settings::application.record.priority_mode < 1;

    // create a gstreamer pipeline
    std::string description = "appsrc name=src ! videoconvert ! ";
    if (Settings::application.record.profile < 0 || Settings::application.record.profile >= DEFAULT)
        Settings::application.record.profile = H264_STANDARD;
    description += profile_description[Settings::application.record.profile];

    // verify location path (path is always terminated by the OS dependent separator)
    std::string path = SystemToolkit::path_directory(Settings::application.record.path);
    if (path.empty())
        path = SystemToolkit::home_path();

    // setup filename & muxer
    if( Settings::application.record.profile == JPEG_MULTI) {
        std::string folder = path + "vimix_" + SystemToolkit::date_time_string();
        filename_ = SystemToolkit::full_filename(folder, "%05d.jpg");
        if (SystemToolkit::create_directory(folder))
            description += "multifilesink name=sink";
    }
    else if( Settings::application.record.profile == VP8) {
        filename_ = path + "vimix_" + SystemToolkit::date_time_string() + ".webm";
        description += "webmmux ! filesink name=sink";
    }
    else {
        filename_ = path + "vimix_" + SystemToolkit::date_time_string() + ".mov";
        description += "qtmux ! filesink name=sink";
    }

    // parse pipeline descriptor
    GError *error = NULL;
    pipeline_ = gst_parse_launch (description.c_str(), &error);
    if (error != NULL) {
        Log::Info("Video Recording : Could not construct pipeline %s\n%s", description.c_str(), error->message);
        Log::Warning("Video Recording : Failed to initiate GStreamer.");
        g_clear_error (&error);
        finished_ = true;
        return;
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
//                      "do-timestamp", TRUE,
                      NULL);

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
        Log::Warning("Video Recording : Failed to configure frame grabber.");
        finished_ = true;
        return;
    }

    // start recording
    GstStateChangeReturn ret = gst_element_set_state (pipeline_, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        Log::Warning("Video Recording : Failed to start frame grabber.");
        finished_ = true;
        return;
    }

    // all good
    Log::Info("Video Recording started (%s)", profile_name[Settings::application.record.profile]);

    // start recording !!
    active_ = true;
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

    Log::Notify("Video Recording %s is ready.", filename_.c_str());
}

std::string VideoRecorder::info() const
{
    if (active_)
        return FrameGrabber::info();
    else if (!endofstream_)
        return "Saving file...";
    else
        return "...";
}
