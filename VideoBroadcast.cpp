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
#include "NetworkToolkit.h"

#include "VideoBroadcast.h"

#ifndef NDEBUG
#define BROADCAST_DEBUG
#endif


VideoBroadcast::VideoBroadcast(NetworkToolkit::BroadcastProtocol proto, int port): FrameGrabber(), protocol_(proto), port_(port), stopped_(false)
{
    frame_duration_ = gst_util_uint64_scale_int (1, GST_SECOND, BROADCAST_FPS);  // fixed 30 FPS

}

std::string VideoBroadcast::init(GstCaps *caps)
{
    // ignore
    if (caps == nullptr)
        return std::string("Invalid caps");

    // create a gstreamer pipeline
    std::string description = "appsrc name=src ! videoconvert ! queue ! ";

    // choose pipeline for protocol
    if (protocol_ == NetworkToolkit::BROADCAST_DEFAULT)
        protocol_ = NetworkToolkit::BROADCAST_SRT;
    description += NetworkToolkit::broadcast_pipeline[protocol_];

    // setup streaming pipeline
    if (protocol_ == NetworkToolkit::BROADCAST_SRT) {
        // change the pipeline to include the broadcast port
        std::string::size_type xxxx = description.find("XXXX");
        if (xxxx != std::string::npos)
            description = description.replace(xxxx, 4, std::to_string(port_));
        else
            return std::string("Video Broadcast : Failed to configure broadcast port.");
    }

    // parse pipeline descriptor
    GError *error = NULL;
    pipeline_ = gst_parse_launch (description.c_str(), &error);
    if (error != NULL) {
        std::string msg = std::string("Video Broadcast : Could not construct pipeline ") + description + "\n" + std::string(error->message);
        g_clear_error (&error);
        return msg;
    }

    // setup streaming sink
    if (protocol_ == NetworkToolkit::BROADCAST_SRT) {
        g_object_set (G_OBJECT (gst_bin_get_by_name (GST_BIN (pipeline_), "sink")),
                      "latency", 500,
                      NULL);
    }

    // TODO Configure options

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

    return std::string("Video Broadcast started.");
}



void VideoBroadcast::terminate()
{
    // send EOS
    gst_app_src_end_of_stream (src_);

    Log::Notify("Broadcast terminated after %s s.",
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
        ret << "Starting";
    else if (active_) {
        ret << NetworkToolkit::broadcast_protocol_label[protocol_];
        ret << " ( Port " << port_ << " )";
    }
    else
        ret <<  "Terminated";

    return ret.str();
}
