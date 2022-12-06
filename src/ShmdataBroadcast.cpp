#include <sstream>
#include <iostream>

// gstreamer
#include <gst/gstformat.h>
#include <gst/video/video.h>
#include <gst/app/gstappsrc.h>
#include <gst/pbutils/pbutils.h>

#include "Log.h"
#include "Settings.h"
#include "GstToolkit.h"

#include "ShmdataBroadcast.h"

// Test command
// gst-launch-1.0 --gst-plugin-path=/usr/local/lib/gstreamer-1.0/ shmdatasrc socket-path=/tmp/vimix-feed ! videoconvert ! autovideosink

bool ShmdataBroadcast::available()
{
    static bool _available = false;

    // test for availability once on first run
    static bool _tested = false;
    if (!_tested)
        _available = GstToolkit::has_feature("shmdatasink");

    return _available;
}

ShmdataBroadcast::ShmdataBroadcast(const std::string &socketpath): FrameGrabber(), socket_path_(socketpath), stopped_(false)
{
    frame_duration_ = gst_util_uint64_scale_int (1, GST_SECOND, DEFAULT_GRABBER_FPS);  // fixed 30 FPS

    socket_path_ += std::to_string(Settings::application.instance_id);
}

std::string ShmdataBroadcast::init(GstCaps *caps)
{
    if (!ShmdataBroadcast::available())
        return std::string("Shmdata Broadcast : Not available (missing shmdatasink plugin)");

    // ignore
    if (caps == nullptr)
        return std::string("Shmdata Broadcast : Invalid caps");

    // create a gstreamer pipeline
    std::string description = "appsrc name=src ! queue ! shmdatasink socket-path=XXXX name=sink";

    // change the placeholder to include the broadcast port
    std::string::size_type xxxx = description.find("XXXX");
    if (xxxx != std::string::npos)
        description.replace(xxxx, 4, socket_path_);
    else
        return std::string("Shmdata Broadcast : Failed to configure shared memory socket path.");

    // parse pipeline descriptor
    GError *error = NULL;
    pipeline_ = gst_parse_launch (description.c_str(), &error);
    if (error != NULL) {
        std::string msg = std::string("Shmdata Broadcast : Could not construct pipeline ") + description + "\n" + std::string(error->message);
        g_clear_error (&error);
        return msg;
    }

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
        gst_value_set_fraction (&v, DEFAULT_GRABBER_FPS, 1);  // fixed 30 FPS
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
    // send EOS
    gst_app_src_end_of_stream (src_);

    Log::Notify("Shared Memory terminated after %s s.",
                GstToolkit::time_to_string(duration_).c_str());
}

void ShmdataBroadcast::stop ()
{
    // stop recording
    FrameGrabber::stop ();

    // force finished
    endofstream_ = true;
    active_ = false;
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
