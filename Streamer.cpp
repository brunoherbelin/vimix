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

#include "Streamer.h"


const char* VideoStreamer::profile_name[VideoStreamer::DEFAULT] = {
    "UDP (JPEG)"
};

const std::vector<std::string> VideoStreamer::profile_description {

    "video/x-raw, format=I420,framerate=30/1 ! jpegenc ! rtpjpegpay ! udpsink host=127.0.0.1 port=500"
};


VideoStreamer::VideoStreamer(): FrameGrabber(), frame_buffer_(nullptr), width_(0), height_(0),
        recording_(false), accept_buffer_(false), pipeline_(nullptr), src_(nullptr), timestamp_(0)
{

    // configure fix parameter
    frame_duration_ = gst_util_uint64_scale_int (1, GST_SECOND, 30);  // 30 FPS
    timeframe_ = 2 * frame_duration_;
}

VideoStreamer::~VideoStreamer()
{
    if (src_ != nullptr)
        gst_object_unref (src_);
    if (pipeline_ != nullptr) {
        gst_element_set_state (pipeline_, GST_STATE_NULL);
        gst_object_unref (pipeline_);
    }

    if (pbo_[0] > 0)
        glDeleteBuffers(2, pbo_);
}

