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
    "MJPEG RTP (UDP)",
    "MPEG4 RTP (UDP)",
    "H264  RTP (UDP)"
};

const std::vector<std::string> VideoStreamer::profile_description {

    "video/x-raw, format=I420 ! jpegenc ! rtpjpegpay ! udpsink name=sink",
    "video/x-raw, format=I420 ! avenc_mpeg4 ! rtpmp4vpay config-interval=3 ! udpsink name=sink",
    "video/x-raw, format=I420 ! x264enc pass=4 quantizer=26 speed-preset=3 threads=4 ! rtph264pay ! udpsink name=sink"
};


const std::vector<std::string> VideoStreamer::receiver_example {

    "gst-launch-1.0 udpsrc port=5000 ! application/x-rtp,encoding-name=JPEG,payload=26 ! rtpjpegdepay ! jpegdec ! autovideosink",
    "video/x-raw, format=I420 ! avenc_mpeg4 ! rtpmp4vpay config-interval=3 ! udpsink name=sink",
    "gst-launch-1.0 -v udpsrc port=5000 caps=\"application/x-rtp, media=(string)video, clock-rate=(int)90000, encoding-name=(string)H264, payload=(int)96\" ! rtph264depay ! decodebin ! videoconvert ! autovideosink"
};

VideoStreamer::VideoStreamer(): FrameGrabber(), frame_buffer_(nullptr), width_(0), height_(0),
        streaming_(false), accept_buffer_(false), pipeline_(nullptr), src_(nullptr), timestamp_(0)
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


void VideoStreamer::addFrame (FrameBuffer *frame_buffer, float dt)
{    
    // ignore
    if (frame_buffer == nullptr)
        return;


    // first frame for initialization
   if (frame_buffer_ == nullptr) {

       // set frame buffer as input
       frame_buffer_ = frame_buffer;

       // define stream properties
       width_ = frame_buffer_->width();
       height_ = frame_buffer_->height();
       size_ = width_ * height_ * (frame_buffer_->use_alpha() ? 4 : 3);

       // create PBOs
       glGenBuffers(2, pbo_);
       glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo_[1]);
       glBufferData(GL_PIXEL_PACK_BUFFER, size_, NULL, GL_STREAM_READ);
       glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo_[0]);
       glBufferData(GL_PIXEL_PACK_BUFFER, size_, NULL, GL_STREAM_READ);

       // create a gstreamer pipeline
       std::string description = "appsrc name=src ! videoconvert ! ";

       if (Settings::application.stream.profile < 0 || Settings::application.stream.profile >= DEFAULT)
           Settings::application.stream.profile = UDP_MJPEG;
       description += profile_description[Settings::application.stream.profile];

       Settings::application.stream.ip = "127.0.0.1";
//       Settings::application.stream.port = 1000;

       // parse pipeline descriptor
       GError *error = NULL;
       pipeline_ = gst_parse_launch (description.c_str(), &error);
       if (error != NULL) {
           Log::Warning("VideoStreamer Could not construct pipeline %s:\n%s", description.c_str(), error->message);
           g_clear_error (&error);
           finished_ = true;
           return;
       }

       // setup streaming sink
       g_object_set (G_OBJECT (gst_bin_get_by_name (GST_BIN (pipeline_), "sink")),
                     "host", "127.0.0.1",
                     "port", Settings::application.stream.port,
                     NULL);

       // setup custom app source
       src_ = GST_APP_SRC( gst_bin_get_by_name (GST_BIN (pipeline_), "src") );
       if (src_) {

           g_object_set (G_OBJECT (src_),
                         "stream-type", GST_APP_STREAM_TYPE_STREAM,
                         "is-live", TRUE,
                         "format", GST_FORMAT_TIME,
                         //                     "do-timestamp", TRUE,
                         NULL);

           // Direct encoding (no buffering)
           gst_app_src_set_max_bytes( src_, 0 );

           // instruct src to use the required caps
           GstCaps *caps = gst_caps_new_simple ("video/x-raw",
                                "format", G_TYPE_STRING, frame_buffer_->use_alpha() ? "RGBA" : "RGB",
                                "width",  G_TYPE_INT, width_,
                                "height", G_TYPE_INT, height_,
                                "framerate", GST_TYPE_FRACTION, 30, 1,
                                NULL);
           gst_app_src_set_caps (src_, caps);
           gst_caps_unref (caps);

           // setup callbacks
           GstAppSrcCallbacks callbacks;
           callbacks.need_data = callback_need_data;
           callbacks.enough_data = callback_enough_data;
           callbacks.seek_data = NULL; // stream type is not seekable
           gst_app_src_set_callbacks (src_, &callbacks, this, NULL);

       }
       else {
           Log::Warning("VideoStreamer Could not configure capture source");
           finished_ = true;
           return;
       }

       // start recording
       GstStateChangeReturn ret = gst_element_set_state (pipeline_, GST_STATE_PLAYING);
       if (ret == GST_STATE_CHANGE_FAILURE) {
           Log::Warning("VideoStreamer failed");
           finished_ = true;
           return;
       }

       // all good
       Log::Info("VideoStreamer start (%s %d x %d)", profile_name[Settings::application.record.profile], width_, height_);

       // start streaming !!
       streaming_ = true;

   }

   // frame buffer changed ?
   else if (frame_buffer_ != frame_buffer) {

       // if an incompatilble frame buffer given: stop recorder
       if ( frame_buffer->width() != width_ ||
            frame_buffer->height() != height_ ||
            frame_buffer->use_alpha() != frame_buffer_->use_alpha()) {

           stop();
           Log::Warning("Streaming interrupted: new session (%d x %d) incompatible with recording (%d x %d)", frame_buffer->width(), frame_buffer->height(), width_, height_);
       }
       else {
           // accepting a new frame buffer as input
           frame_buffer_ = frame_buffer;
       }
   }

   // store a frame if recording is active
   if (streaming_ && size_ > 0)
   {
       // calculate dt in ns
       timeframe_ +=  gst_gdouble_to_guint64( dt * 1000000.f);

       // if time is passed one frame duration (with 10% margin)
       // and if the encoder accepts data
       if ( timeframe_ > frame_duration_ - 3000000 && accept_buffer_) {

           // set buffer target for writing in a new frame
           glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo_[pbo_index_]);

#ifdef USE_GLREADPIXEL
           // get frame
           frame_buffer->readPixels();
#else
           glBindTexture(GL_TEXTURE_2D, frame_buffer->texture());
           glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
#endif

           // update case ; alternating indices
           if ( pbo_next_index_ != pbo_index_ ) {

               // set buffer target for saving the frame
               glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo_[pbo_next_index_]);

               // new buffer
               GstBuffer *buffer = gst_buffer_new_and_alloc (size_);

               // set timing of buffer
               buffer->pts = timestamp_;
               buffer->duration = frame_duration_;

               // map gst buffer into a memory  WRITE target
               GstMapInfo map;
               gst_buffer_map (buffer, &map, GST_MAP_WRITE);

               // map PBO pixels into a memory READ pointer
               unsigned char* ptr = (unsigned char*) glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);

               // transfer pixels from PBO memory to buffer memory
               if (NULL != ptr)
                   memmove(map.data, ptr, size_);

               // un-map
               glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
               gst_buffer_unmap (buffer, &map);

               // push
    //           Log::Info("VideoRecorder push data %ld", buffer->pts);
               gst_app_src_push_buffer (src_, buffer);
               // NB: buffer will be unrefed by the appsrc

               // next timestamp
               timestamp_ += frame_duration_;
           }

           glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

           // alternate indices
           pbo_next_index_ = pbo_index_;
           pbo_index_ = (pbo_index_ + 1) % 2;

           // restart frame counter
           timeframe_ = 0;
       }

   }
   // did the recording terminate with sink receiving end-of-stream ?
   else
   {
       // Wait for EOS message
       GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline_));
       GstMessage *msg = gst_bus_poll(bus, GST_MESSAGE_EOS, GST_TIME_AS_USECONDS(1));

       if (msg) {
//           Log::Info("received EOS");
           // stop the pipeline
           GstStateChangeReturn ret = gst_element_set_state (pipeline_, GST_STATE_NULL);
           if (ret == GST_STATE_CHANGE_FAILURE)
               Log::Warning("VideoStreamer Could not stop");
           else
               Log::Notify("Stream finished after %s s.", GstToolkit::time_to_string(timestamp_).c_str());

           finished_ = true;
       }
   }


}

void VideoStreamer::stop ()
{
    // send end of stream
    gst_app_src_end_of_stream (src_);
//    Log::Info("VideoRecorder push EOS");

    // stop recording
    streaming_ = false;
}

std::string VideoStreamer::info()
{
    if (streaming_)
        return GstToolkit::time_to_string(timestamp_);
    else
        return "Closing stream...";
}


double VideoStreamer::duration()
{
    return gst_guint64_to_gdouble( GST_TIME_AS_MSECONDS(timestamp_) ) / 1000.0;
}

// appsrc needs data and we should start sending
void VideoStreamer::callback_need_data (GstAppSrc *, guint , gpointer p)
{
//    Log::Info("H264Recording callback_need_data");
    VideoStreamer *rec = (VideoStreamer *)p;
    if (rec) {
        rec->accept_buffer_ = rec->streaming_ ? true : false;
    }
}

// appsrc has enough data and we can stop sending
void VideoStreamer::callback_enough_data (GstAppSrc *, gpointer p)
{
//    Log::Info("H264Recording callback_enough_data");
    VideoStreamer *rec = (VideoStreamer *)p;
    if (rec) {
        rec->accept_buffer_ = false;
    }
}
