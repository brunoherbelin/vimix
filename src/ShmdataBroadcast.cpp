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

#include <sstream>
#include <vector>
#include <regex>


// gstreamer
#include <gst/gstformat.h>
#include <gst/video/video.h>
#include <gst/app/gstappsrc.h>
#include <gst/pbutils/pbutils.h>

#include "Log.h"
#include "Toolkit/GstToolkit.h"
#include "Toolkit/SystemToolkit.h"

#include "ShmdataBroadcast.h"

// Test command shmdata
// gst-launch-1.0 --gst-plugin-path=/usr/local/lib/gstreamer-1.0/ shmdatasrc socket-path=/tmp/shmdata_vimix0 ! videoconvert ! autovideosink

// Test command shmsrc
// gst-launch-1.0 shmsrc socket-path=/tmp/shmdata_vimix0 is-live=true ! video/x-raw, format=RGB, framerate=30/1, width=1152, height=720 ! videoconvert ! autovideosink

// OBS receiver
// shmsrc socket-path=/tmp/shmdata_vimix0 is-live=true ! video/x-raw, format=RGB, framerate=30/1, width=1152, height=720 ! videoconvert ! video.
// !!!  OBS needs access to shm & filesystem
// sudo flatpak override --device=shm com.obsproject.Studio
// sudo flatpak override --filesystem=/tmp com.obsproject.Studio

std::vector<std::string> shm_sink_ = {
    "shmsink",
    "shmdatasink"
};

bool ShmdataBroadcast::available(Method m)
{
    // test for availability once on first run
    static bool _shm_available = false, _shmdata_available = false, _tested = false;
    if (!_tested) {
        _shm_available = GstToolkit::has_feature(shm_sink_[SHM_SHMSINK]);
        _shmdata_available = GstToolkit::has_feature(shm_sink_[SHM_SHMDATASINK]);
        _tested = true;
    }

    if (m == SHM_SHMSINK)
        return _shm_available;
    else if (m == SHM_SHMDATASINK)
        return _shmdata_available;
    else
        return _shm_available | _shmdata_available;
}

ShmdataBroadcast::ShmdataBroadcast(Method m, const std::string &socketpath): FrameGrabber(), method_(m), socket_path_(socketpath)
{
    frame_duration_ = gst_util_uint64_scale_int (1, GST_SECOND, SHMDATA_FPS);  // fixed 30 FPS

    // default socket path
    if (socket_path_.empty())
        socket_path_ = SHMDATA_DEFAULT_PATH;

    // default to SHMSINK / ignore SHM_SHMDATASINK if not available
    if (!GstToolkit::has_feature(shm_sink_[SHM_SHMDATASINK]) || m == SHM_SHMDATAANY)
        method_ = SHM_SHMSINK;
}

std::string ShmdataBroadcast::init(GstCaps *caps)
{
    if (!ShmdataBroadcast::available())
        return std::string("Shared Memory Broadcast : Not available (missing shmsink or shmdatasink plugin)");

    // ignore
    if (caps == nullptr)
        return std::string("Shared Memory Broadcast : Invalid caps");

    // create a gstreamer pipeline
    std::string description = "appsrc name=src ! queue ! ";

    // complement pipeline with sink
    description += shm_sink_[method_] + " name=sink";

    // parse pipeline descriptor
    GError *error = NULL;
    pipeline_ = gst_parse_launch (description.c_str(), &error);
    if (error != NULL) {
        std::string msg = std::string("Shared Memory Broadcast : Could not construct pipeline ") + description + "\n" + std::string(error->message);
        g_clear_error (&error);
        return msg;
    }

    // setup shm sink
    g_object_set (G_OBJECT (gst_bin_get_by_name (GST_BIN (pipeline_), "sink")),
                  "socket-path", socket_path_.c_str(),
                  "wait-for-connection", FALSE,
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
        gst_value_set_fraction (&v, SHMDATA_FPS, 1);  // fixed 30 FPS
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
        return std::string("Shared Memory Broadcast : Failed to configure frame grabber ") + shm_sink_[method_];
    }

    // start
    GstStateChangeReturn ret = gst_element_set_state (pipeline_, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        return std::string("Shared Memory Broadcast : Failed to start frame grabber.");
    }

    // all good
    initialized_ = true;

    return std::string("Shared Memory Broadcast with '") + shm_sink_[method_] + "' started on " + socket_path_;
}


void ShmdataBroadcast::terminate()
{
    // end src
    gst_app_src_end_of_stream (src_);

    // force finished
    endofstream_ = true;
    active_ = false;

    // delete socket
    SystemToolkit::remove_file(socket_path_);

    Log::Notify("Shared Memory terminated after %s s.",
                GstToolkit::time_to_string(duration_).c_str());
}


std::string ShmdataBroadcast::gst_pipeline() const
{
    std::string pipeline;

    pipeline += (method_ == SHM_SHMDATASINK) ? "shmdatasrc" : "shmsrc";
    pipeline += " socket-path=";
    pipeline += socket_path_;

    if (method_ == SHM_SHMSINK){
        pipeline += " is-live=true";
        pipeline += " ! ";
        pipeline += std::string( gst_caps_to_string(caps_) );
        pipeline = std::regex_replace(pipeline, std::regex("\\(int\\)"), "");
        pipeline = std::regex_replace(pipeline, std::regex("\\(fraction\\)"), "");
        pipeline = std::regex_replace(pipeline, std::regex("\\(string\\)"), "");
    }

    return pipeline;
}

std::string ShmdataBroadcast::info(bool extended) const
{
    std::ostringstream ret;

    if (extended) {
        ret << gst_pipeline();
    }
    else {
        if (!initialized_)
            ret << "Shared Memory starting..";
        else if (active_)
            ret << "Shared Memory " << socket_path_;
        else
            ret << "Shared Memory terminated";
    }

    return ret.str();
}
