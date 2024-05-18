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

#include <vector>
#include <sstream>
#include <iostream>

// gstreamer
#include <gst/gstformat.h>
#include <gst/video/video.h>
#include <gst/app/gstappsrc.h>
#include <gst/pbutils/pbutils.h>

#include "Log.h"
#include "GstToolkit.h"
#include "Settings.h"

#include "VideoBroadcast.h"

#ifndef NDEBUG
#define BROADCAST_DEBUG
#endif

std::string VideoBroadcast::srt_sink_;
std::string VideoBroadcast::srt_encoder_;

std::vector< std::string > srt_sink_alternatives_ {
    "srtsink",
    "srtserversink"
};

std::vector< std::pair<std::string, std::string> > srt_encoder_alternatives_ {
    {"nvh264enc", "nvh264enc zerolatency=true rc-mode=cbr-ld-hq bitrate=4000 ! "},
    {"vaapih264enc", "vaapih264enc rate-control=cqp init-qp=26 ! "},
    {"vtenc_h264_hw", "vtenc_h264_hw realtime=1 allow-frame-reordering=0 ! "},
    {"x264enc", "x264enc tune=zerolatency ! "}
};

bool VideoBroadcast::available()
{
    // test for installation on first run
    static bool _tested = false;
    if (!_tested) {
        srt_sink_.clear();
        srt_encoder_.clear();

        for (auto config = srt_sink_alternatives_.cbegin();
             config != srt_sink_alternatives_.cend() && srt_sink_.empty(); ++config) {
            if ( GstToolkit::has_feature(*config) ) {
                srt_sink_ = *config;
            }
        }

        if (!srt_sink_.empty())
        {
            if (Settings::application.render.gpu_decoding) {
                for (auto config = srt_encoder_alternatives_.cbegin();
                     config != srt_encoder_alternatives_.cend() && srt_encoder_.empty(); ++config) {
                    if ( GstToolkit::has_feature(config->first) ) {
                        srt_encoder_ = config->second;
                        if (config->first != srt_encoder_alternatives_.back().first)
                            Log::Info("Video Broadcast uses hardware-accelerated encoder (%s)", config->first.c_str());
                    }
                }
            }
            // disabled hardware accelerated encoding
            else {
                srt_encoder_ = srt_encoder_alternatives_.back().second;
            }
        }
        else
            Log::Info("Video SRT Broadcast not available.");

        // perform test only once
        _tested = true;
    }

    // video broadcast is installed if both srt and h264 are available
    return (!srt_sink_.empty() && !srt_encoder_.empty());
}

VideoBroadcast::VideoBroadcast(int port): FrameGrabber(), port_(port)
{
    frame_duration_ = gst_util_uint64_scale_int (1, GST_SECOND, BROADCAST_FPS);  // fixed 30 FPS
    if (port_ < 1000)
        port_ = BROADCAST_DEFAULT_PORT;
}

std::string VideoBroadcast::init(GstCaps *caps)
{
    if (!VideoBroadcast::available())
        return std::string("Video Broadcast : Not available (missing SRT or H264)");

    // ignore
    if (caps == nullptr)
        return std::string("Video Broadcast : Invalid caps");

    // create a gstreamer pipeline
    std::string description = "appsrc name=src ! videoconvert ! ";

    // complement pipeline with encoder
    description += VideoBroadcast::srt_encoder_;
    description += "video/x-h264, profile=high ! queue ! h264parse config-interval=-1 ! mpegtsmux alignment=7 ! ";

    // complement pipeline with sink
    description += VideoBroadcast::srt_sink_ + " name=sink";

    // parse pipeline descriptor
    GError *error = NULL;
    pipeline_ = gst_parse_launch (description.c_str(), &error);
    if (error != NULL) {
        std::string msg = std::string("Video Broadcast : Could not construct pipeline ") + description + "\n" + std::string(error->message);
        g_clear_error (&error);
        return msg;
    }

    // setup SRT streaming sink properties (uri, latency)
    g_object_set (G_OBJECT (gst_bin_get_by_name (GST_BIN (pipeline_), "sink")),
                  "localport", port_,
                  "latency", 200,
                  "mode", 2,
                  "wait-for-connection", false,
                  NULL);

    // setup custom app source
    src_ = GST_APP_SRC( gst_bin_get_by_name (GST_BIN (pipeline_), "src") );
    if (src_) {

        g_object_set (G_OBJECT (src_),
                      "is-live", TRUE,
                      "format", GST_FORMAT_TIME,
                      NULL);

        // configure stream
        gst_app_src_set_stream_type( src_, GST_APP_STREAM_TYPE_STREAM);
        gst_app_src_set_latency( src_, -1, 0);

        // Set buffer size
        gst_app_src_set_max_bytes( src_, buffering_size_ );

        // specify streaming framerate in the given caps
        GstCaps *tmp = gst_caps_copy( caps );
        GValue v = G_VALUE_INIT;
        g_value_init (&v, GST_TYPE_FRACTION);
        gst_value_set_fraction (&v, BROADCAST_FPS, 1);  // fixed 30 FPS
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
        return std::string("Video Broadcast : Failed to configure frame grabber.");
    }

    // start
    GstStateChangeReturn ret = gst_element_set_state (pipeline_, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        return std::string("Video Broadcast : Failed to start frame grabber.");
    }

    // all good
    initialized_ = true;

    return std::string("Video Broadcast started SRT on port ") + std::to_string(port_);
}

void VideoBroadcast::terminate()
{
    // send EOS
    gst_app_src_end_of_stream (src_);

    Log::Notify("Video Broadcast terminated after %s s.",
                GstToolkit::time_to_string(duration_).c_str());
}

void VideoBroadcast::stop ()
{
    // stop recording
    FrameGrabber::stop ();

    // force finished
    endofstream_ = true;
    active_ = false;
}

std::string VideoBroadcast::info() const
{
    std::ostringstream ret;

    if (!initialized_)
        ret << "SRT starting..";
    else if (active_)
        ret << "SRT Broadcast on port " << port_;
    else
        ret << "SRT terminated";

    return ret.str();
}


// Alternatives
//
// RTP streaming of H264 directly on UDP
// SEND:
// gst-launch-1.0 v4l2src ! videoconvert !  x264enc pass=qual quantizer=20 tune=zerolatency ! rtph264pay !  udpsink host=127.0.0.1  port=1234
// RECEIVE:
// gst-launch-1.0 udpsrc port=1234 !  "application/x-rtp, payload=127" ! rtph264depay ! avdec_h264 !  videoconvert  ! autovideosink


// RIST streaming of H264 with static payload Mpeg RTP
// SEND:
// gst-launch-1.0 v4l2src ! videoconvert ! x264enc pass=qual quantizer=20 tune=zerolatency ! mpegtsmux ! rtpmp2tpay ! ristsink address=0.0.0.0 port=7072
// gst-launch-1.0 v4l2src ! videoconvert ! x264enc ! mpegtsmux ! rtpmp2tpay ! ristsink bonding-addresses="127.0.0.1:7070,127.0.0.1:7072"

// RECEIVE:
// gst-launch-1.0 ristsrc port=7072 ! rtpmp2tdepay ! tsdemux ! decodebin !  videoconvert  ! autovideosink

//   {"ristsink", "ristsink port=XXXX name=sink"},
//{"nvh264enc", "nvh264enc zerolatency=true rc-mode=cbr-ld-hq bitrate=4000 ! video/x-h264, profile=(string)high ! rtph264pay ! queue ! "},
//{"vaapih264enc", "vaapih264enc rate-control=cqp init-qp=26 ! video/x-h264, profile=high ! rtph264pay ! queue ! "},
//{"x264enc", "x264enc tune=zerolatency ! video/x-h264, profile=high ! rtph264pay ! "}
