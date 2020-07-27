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

// use glReadPixel or glGetTextImage ?
// read pixels & pbo should be the fastest
// https://stackoverflow.com/questions/38140527/glreadpixels-vs-glgetteximage
#define USE_GLREADPIXEL

using namespace std;

Recorder::Recorder() : finished_(false), pbo_index_(0), pbo_next_index_(0), size_(0)
{
    pbo_[0] = pbo_[1] = 0;
}

PNGRecorder::PNGRecorder() : Recorder()
{
    std::string path = SystemToolkit::path_directory(Settings::application.record.path);
    if (path.empty())
        path = SystemToolkit::home_path();

    filename_ = path + SystemToolkit::date_time_string() + "_vimix.png";

}

// Thread to perform slow operation of saving to file
void save_png(std::string filename, unsigned char *data, uint w, uint h, uint c)
{
    // got data to save ?
    if (data) {
        // save file
        stbi_write_png(filename.c_str(), w, h, c, data, w * c);
        // notify
        Log::Notify("Capture %s ready (%d x %d %d)", filename.c_str(), w, h, c);
        // done
        free(data);
    }
}

void PNGRecorder::addFrame(FrameBuffer *frame_buffer, float)
{
    // ignore
    if (frame_buffer == nullptr)
        return;

    // get what is needed from frame buffer
    uint w = frame_buffer->width();
    uint h = frame_buffer->height();
    uint c = frame_buffer->use_alpha() ? 4 : 3;

    // first iteration: initialize and get frame
    if (size_ < 1)
    {
        // init size
        size_ = w * h * c;

        // create PBO
        glGenBuffers(2, pbo_);

        // set writing PBO
        glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo_[0]);
        glBufferData(GL_PIXEL_PACK_BUFFER, size_, NULL, GL_STREAM_READ);

#ifdef USE_GLREADPIXEL
        // get frame
        frame_buffer->readPixels();
#else
        glBindTexture(GL_TEXTURE_2D, frame_buffer->texture());
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
#endif
    }
    // second iteration; get frame and save file
    else {

        // set reading PBO
        glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo_[0]);

        // get pixels
        unsigned char* ptr = (unsigned char*) glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
        if (NULL != ptr) {
            // prepare memory buffer0
            unsigned char * data = (unsigned char*) malloc(size_);
            // transfer frame to data
            memmove(data, ptr, size_);
            // save in separate thread
            std::thread(save_png, filename_, data, w, h, c).detach();
        }
        // unmap
        glUnmapBuffer(GL_PIXEL_PACK_BUFFER);

        // ok done
        glDeleteBuffers(2, pbo_);

        // recorded one frame
        finished_ = true;
    }

    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

//    unsigned char * data = (unsigned char*) malloc(size);
//    GLenum format = frame_buffer->use_alpha() ? GL_RGBA : GL_RGB;
//    glGetTextureSubImage( frame_buffer->texture(), 0, 0, 0, 0, w, h, 1, format, GL_UNSIGNED_BYTE, size, data);

}

const char* VideoRecorder::profile_name[4] = { "H264 (low)", "H264 (high)", "Apple ProRes 4444", "WebM VP9" };
const std::vector<std::string> VideoRecorder::profile_description {
    // Control x264 encoder quality :
    // pass=4
    //    quant (4) – Constant Quantizer
    //    qual  (5) – Constant Quality
    // quantizer=23
    //   The total range is from 0 to 51, where 0 is lossless, 18 can be considered ‘visually lossless’,
    //   and 51 is terrible quality. A sane range is 18-26, and the default is 23.
    // speed-preset=3
    //    veryfast (3)
    //    faster (4)
    //    fast (5)
    "x264enc pass=4 quantizer=23 speed-preset=3 ! video/x-h264, profile=baseline ! h264parse ! ",
    "x264enc pass=5 quantizer=18 speed-preset=4 ! video/x-h264, profile=high ! h264parse ! ",
    // Apple ProRes encoding parameters
    //  pass=2
    //       cbr (0) – Constant Bitrate Encoding
    //      quant (2) – Constant Quantizer
    //      pass1 (512) – VBR Encoding - Pass 1
    "avenc_prores bitrate=60000 pass=2 ! ",
    // WebM VP9 encoding parameters
    // https://www.webmproject.org/docs/encoder-parameters/
    // https://developers.google.com/media/vp9/settings/vod/
    "vp9enc end-usage=vbr end-usage=vbr cpu-used=3 max-quantizer=35 target-bitrate=200000 keyframe-max-dist=360 token-partitions=2 static-threshold=1000 ! "

};

// FAILED
// x265 encoder quality
//       string description = "appsrc name=src ! videoconvert ! "
//               "x265enc tune=4 speed-preset=2 option-string='crf=28' ! h265parse ! "
//               "qtmux ! filesink name=sink";


VideoRecorder::VideoRecorder() : Recorder(), frame_buffer_(nullptr), width_(0), height_(0),
    recording_(false), pipeline_(nullptr), src_(nullptr), timestamp_(0), timeframe_(0), accept_buffer_(false)
{
    // auto filename
    std::string path = SystemToolkit::path_directory(Settings::application.record.path);
    if (path.empty())
        path = SystemToolkit::home_path();

    filename_ = path + SystemToolkit::date_time_string() + "_vimix.mov";

    // configure fix parameter
    frame_duration_ = gst_util_uint64_scale_int (1, GST_SECOND, 30);  // 30 FPS
    timeframe_ = 2 * frame_duration_;
}

VideoRecorder::~VideoRecorder()
{
    if (src_ != nullptr)
        gst_object_unref (src_);
    if (pipeline_ != nullptr) {
        gst_element_set_state (pipeline_, GST_STATE_NULL);
        gst_object_unref (pipeline_);
    }

    glDeleteBuffers(2, pbo_);
}

void VideoRecorder::addFrame (FrameBuffer *frame_buffer, float dt)
{
    // TODO : avoid software videoconvert by using a GPU shader to produce Y444 frames

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
       string description = "appsrc name=src ! videoconvert ! ";
       description += profile_description[Settings::application.record.profile];
       description += "qtmux ! filesink name=sink";

       // parse pipeline descriptor
       GError *error = NULL;
       pipeline_ = gst_parse_launch (description.c_str(), &error);
       if (error != NULL) {
           Log::Warning("VideoRecorder Could not construct pipeline %s:\n%s", description.c_str(), error->message);
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
                         "stream-type", GST_APP_STREAM_TYPE_STREAM,
                         "is-live", TRUE,
                         "format", GST_FORMAT_TIME,
                         //                     "do-timestamp", TRUE,
                         NULL);

           // Direct encoding (no buffering)
           gst_app_src_set_max_bytes( src_, 0 );
//           gst_app_src_set_max_bytes( src_, 2 * buf_size_);

           // instruct src to use the required caps
           GstCaps *caps = gst_caps_new_simple ("video/x-raw",
                                "format", G_TYPE_STRING, "RGB",
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
           Log::Warning("VideoRecorder Could not configure capture source");
           finished_ = true;
           return;
       }

       // start recording
       GstStateChangeReturn ret = gst_element_set_state (pipeline_, GST_STATE_PLAYING);
       if (ret == GST_STATE_CHANGE_FAILURE) {
           Log::Warning("VideoRecorder Could not record %s", filename_.c_str());
           finished_ = true;
           return;
       }

       // all good
       Log::Info("VideoRecorder start recording (%d x %d)", width_, height_);

       // start recording !!
       recording_ = true;
   }
   // frame buffer changed ?
   else if (frame_buffer_ != frame_buffer) {

       // if an incompatilble frame buffer given: stop recorder
       if ( frame_buffer->width() != width_ ||
            frame_buffer->height() != height_ ||
            frame_buffer->use_alpha() != frame_buffer_->use_alpha()) {

           stop();
           Log::Warning("Recording interrupted: new session (%d x %d) incompatible with recording (%d x %d)", frame_buffer->width(), frame_buffer->height(), width_, height_);
       }
       else {
           // accepting a new frame buffer as input
           frame_buffer_ = frame_buffer;
       }
   }

   // store a frame if recording is active
   if (recording_ && size_ > 0)
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
               Log::Warning("VideoRecorder Could not stop");
           else
               Log::Notify("Recording %s ready.", filename_.c_str());

           finished_ = true;
       }
   }
}

void VideoRecorder::stop ()
{
    // send end of stream
    gst_app_src_end_of_stream (src_);
//    Log::Info("VideoRecorder push EOS");

    // stop recording
    recording_ = false;
}

std::string VideoRecorder::info()
{
    if (recording_)
        return GstToolkit::time_to_string(timestamp_);
    else
        return "Saving file...";
}

// appsrc needs data and we should start sending
void VideoRecorder::callback_need_data (GstAppSrc *src, guint length, gpointer p)
{
//    Log::Info("H264Recording callback_need_data");
    VideoRecorder *rec = (VideoRecorder *)p;
    if (rec) {
        rec->accept_buffer_ = rec->recording_ ? true : false;
    }
}

// appsrc has enough data and we can stop sending
void VideoRecorder::callback_enough_data (GstAppSrc *src, gpointer p)
{
//    Log::Info("H264Recording callback_enough_data");
    VideoRecorder *rec = (VideoRecorder *)p;
    if (rec) {
        rec->accept_buffer_ = false;
    }
}

