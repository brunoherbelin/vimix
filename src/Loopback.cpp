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

#include <vector>
#include <sstream>
#include <iostream>
#include <stdexcept>

// gstreamer
#include <gst/gstformat.h>
#include <gst/video/video.h>
#include <gst/app/gstappsrc.h>
#include <gst/pbutils/pbutils.h>

#include "defines.h"
#include "GstToolkit.h"
#include "SystemToolkit.h"
#include "Log.h"

#include "Loopback.h"

std::string Loopback::loopback_sink_;

std::vector< std::pair<std::string, std::string> > loopback_sink_alternatives_ {
    {"v4l2sink", "videorate ! video/x-raw,format=RGB,framerate=30/1 ! v4l2sink"}
    // TODO alternative sink for OSX? Windows?
};

bool Loopback::available()
{
    // test for availability once on first run
    static bool _tested = false;
    if (!_tested) {

        loopback_sink_.clear();

        for (auto config = loopback_sink_alternatives_.cbegin();
             config != loopback_sink_alternatives_.cend() && loopback_sink_.empty(); ++config) {
            if ( GstToolkit::has_feature(config->first) ) {
                loopback_sink_ = config->second;
            }
        }

        _tested = true;
    }

    return !loopback_sink_.empty();
}

Loopback::Loopback(int deviceid) : FrameGrabber(), loopback_device_(deviceid)
{
    frame_duration_ = gst_util_uint64_scale_int (1, GST_SECOND, LOOPBACK_FPS);  // fixed 30 FPS

    if ( !SystemToolkit::file_exists(device_name()) )
        throw std::runtime_error("Loopback system shall be initialized first.");
}

std::string Loopback::device_name() const
{
    return std::string("/dev/video") + std::to_string(loopback_device_);
}

std::string Loopback::init(GstCaps *caps)
{
    if (!Loopback::available())
        return std::string("Loopback : Not available");

    // ignore
    if (caps == nullptr)
        return std::string("Loopback : Invalid caps");

    // create a gstreamer pipeline
    std::string description = "appsrc name=src ! videoconvert ! ";

    // complement pipeline with sink
    description += Loopback::loopback_sink_ + " name=sink";

    // parse pipeline descriptor
    GError *error = NULL;
    pipeline_ = gst_parse_launch (description.c_str(), &error);
    if (error != NULL) {
        std::string msg = std::string("Loopback : Could not construct pipeline ") + description + "\n" + std::string(error->message);
        g_clear_error (&error);
        return msg;
    }

    // setup device sink
    g_object_set (G_OBJECT (gst_bin_get_by_name (GST_BIN (pipeline_), "sink")),
                  "device", device_name().c_str(),
                  "async", TRUE,
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
        gst_value_set_fraction (&v, LOOPBACK_FPS, 1); // fixed FPS
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
        gst_app_src_set_callbacks( src_, &callbacks, this, NULL);

    }
    else {
        return std::string("Loopback : Could not configure source.");
    }

    // start recording
    GstStateChangeReturn ret = gst_element_set_state (pipeline_, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        return std::string("Loopback : Could not open ") + device_name();
    }

    // all good
    initialized_ = true;

    return std::string("Loopback started on ") + device_name();
}

void Loopback::terminate()
{
    // send EOS
    gst_app_src_end_of_stream (src_);

    Log::Notify("Loopback to %s terminated.", device_name().c_str());
}

void Loopback::stop ()
{
    // stop recording
    FrameGrabber::stop ();

    // force finished
    endofstream_ = true;
    active_ = false;
}

std::string Loopback::info() const
{
    std::ostringstream ret;

    if (!initialized_)
        ret << "Loopback starting..";
    else if (active_)
        ret << "V4L2 Loopback on " << device_name();
    else
        ret << "Loopback terminated";

    return ret.str();
}

/**
 *
 * Linux video 4 linux loopback device
 *
 * 1) Linux system has to have the v4l2loopback package
 * See documentation at https://github.com/umlaeute/v4l2loopback
 *
 * $ sudo -A apt install v4l2loopback-dkms
 *
 * 2) User (sudo) has to install a v4l2loopback
 *
 * $ sudo -A modprobe v4l2loopback exclusive_caps=1 video_nr=10
 *
 * 3) But to do that, the user has to enter sudo passwd
 *
 * The command line above should be preceeded by
 *   export SUDO_ASKPASS="/tmp/mysudo.sh"
 *
 * where mysudo.sh contains the following:
 *   #!/bin/bash
 *   zenity --password --title=Authentication
 *
 * 4) Optionaly, we can set the dynamic properties of the stream
 *
 * $ sudo v4l2loopback-ctl set-caps "RGBA:640x480" /dev/video10
 * $ sudo v4l2loopback-ctl set-fps 30 /dev/video10
 *
 * 5) Finally, the gstreamer pipeline can write into v4l2sink
 *
 * gst-launch-1.0 videotestsrc ! v4l2sink device=/dev/video10
 *
 *
 * Useful command lines for debug
 * $ v4l2-ctl --all -d 10
 * $ gst-launch-1.0 v4l2src device=/dev/video10 ! videoconvert ! autovideosink
 * $ gst-launch-1.0 videotestsrc ! v4l2sink device=/dev/video10
 */
