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
    std::string path = SystemToolkit::path_directory(Settings::application.record.path);
    if (path.empty())
        path = SystemToolkit::home_path();

    filename_ = path + "vimix_" + SystemToolkit::date_time_string() + ".png";

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
        if (pbo_[0] > 0)
            glDeleteBuffers(2, pbo_);

        // recorded one frame
        finished_ = true;
    }

    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

//    unsigned char * data = (unsigned char*) malloc(size);
//    GLenum format = frame_buffer->use_alpha() ? GL_RGBA : GL_RGB;
//    glGetTextureSubImage( frame_buffer->texture(), 0, 0, 0, 0, w, h, 1, format, GL_UNSIGNED_BYTE, size, data);

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
    "video/x-raw, format=I420 ! x264enc tune=\"zerolatency\" threads=4 ! video/x-h264, profile=baseline ! h264parse ! ",
#else
    "video/x-raw, format=I420 ! vtenc_h264_hw realtime=1 ! h264parse ! ",
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
    "avenc_prores_ks pass=2 profile=2 quantizer=26 ! ",
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


VideoRecorder::VideoRecorder() : FrameGrabber(), frame_buffer_(nullptr), width_(0), height_(0),
    recording_(false), accept_buffer_(false), pipeline_(nullptr), src_(nullptr), timestamp_(0)
{

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

    if (pbo_[0] > 0)
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
       Log::Info("VideoRecorder start (%s %d x %d)", profile_name[Settings::application.record.profile], width_, height_);

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

               accept_buffer_ = false;

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


double VideoRecorder::duration()
{
    return gst_guint64_to_gdouble( GST_TIME_AS_MSECONDS(timestamp_) ) / 1000.0;
}

bool VideoRecorder::busy()
{
    return accept_buffer_ ? true : false;
}

// appsrc needs data and we should start sending
void VideoRecorder::callback_need_data (GstAppSrc *, guint , gpointer p)
{
//    Log::Info("H264Recording callback_need_data");
    VideoRecorder *rec = (VideoRecorder *)p;
    if (rec) {
        rec->accept_buffer_ = rec->recording_ ? true : false;
    }
}

// appsrc has enough data and we can stop sending
void VideoRecorder::callback_enough_data (GstAppSrc *, gpointer p)
{
//    Log::Info("H264Recording callback_enough_data");
    VideoRecorder *rec = (VideoRecorder *)p;
    if (rec) {
        rec->accept_buffer_ = false;
    }
}

