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
#include <iostream>

// gstreamer
#include <gst/gstformat.h>
#include <gst/video/video.h>
#include <gst/app/gstappsrc.h>
#include <gst/pbutils/pbutils.h>

#include "Log.h"
#include "GstToolkit.h"

#include "ShmdataBroadcast.h"

// Test command
// gst-launch-1.0 --gst-plugin-path=/usr/local/lib/gstreamer-1.0/ shmdatasrc socket-path=/tmp/shmdata_vimix0 ! videoconvert ! autovideosink

std::string ShmdataBroadcast::shmdata_sink_ = "shmdatasink";

bool ShmdataBroadcast::available()
{
    // test for availability once on first run
    static bool _available = false, _tested = false;
    if (!_tested) {
        _available = GstToolkit::has_feature(ShmdataBroadcast::shmdata_sink_);
        _tested = true;
    }

    return _available;
}

ShmdataBroadcast::ShmdataBroadcast(const std::string &socketpath): FrameGrabber(), socket_path_(socketpath)
{
    frame_duration_ = gst_util_uint64_scale_int (1, GST_SECOND, SHMDATA_FPS);  // fixed 30 FPS

    if (socket_path_.empty())
        socket_path_ = SHMDATA_DEFAULT_PATH;
}

std::string ShmdataBroadcast::init(GstCaps *caps)
{
    if (!ShmdataBroadcast::available())
        return std::string("Shmdata Broadcast : Not available (missing shmdatasink plugin)");

    // ignore
    if (caps == nullptr)
        return std::string("Shmdata Broadcast : Invalid caps");

    // create a gstreamer pipeline
    std::string description = "appsrc name=src ! queue ! ";

    // complement pipeline with sink
    description += ShmdataBroadcast::shmdata_sink_ + " name=sink";

    // parse pipeline descriptor
    GError *error = NULL;
    pipeline_ = gst_parse_launch (description.c_str(), &error);
    if (error != NULL) {
        std::string msg = std::string("Shmdata Broadcast : Could not construct pipeline ") + description + "\n" + std::string(error->message);
        g_clear_error (&error);
        return msg;
    }

    // setup shm sink
    g_object_set (G_OBJECT (gst_bin_get_by_name (GST_BIN (pipeline_), "sink")),
                  "socket-path", socket_path_.c_str(),
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
        GValue v = { 0, };
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
        return std::string("Shmdata Broadcast : Failed to configure frame grabber.");
    }

    // start
    GstStateChangeReturn ret = gst_element_set_state (pipeline_, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        return std::string("Shmdata Broadcast : Failed to start frame grabber.");
    }

    // all good
    initialized_ = true;

    return std::string("Shared Memory with Shmdata started on ") + socket_path_;
}


void ShmdataBroadcast::terminate()
{
    // force finished
    endofstream_ = true;
    active_ = false;

    Log::Notify("Shared Memory terminated after %s s.",
                GstToolkit::time_to_string(duration_).c_str());
}


std::string ShmdataBroadcast::info() const
{
    std::ostringstream ret;

    if (!initialized_)
        ret << "Shmdata starting..";
    else if (active_)
        ret << "Shmdata " << socket_path_;
    else
        ret << "Shmdata terminated";

    return ret.str();
}
