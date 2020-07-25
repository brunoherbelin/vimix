#include <thread>

//  Desktop OpenGL function loader
#include <glad/glad.h>

// standalone image loader
#include <stb_image.h>
#include <stb_image_write.h>

// gstreamer
#include <gst/gstformat.h>
#include <gst/video/video.h>

#include "defines.h"
#include "SystemToolkit.h"
#include "FrameBuffer.h"
#include "Log.h"

#include "Recorder.h"

using namespace std;

Recorder::Recorder() : enabled_(true), finished_(false)
{

}


PNGRecorder::PNGRecorder() : Recorder()
{
    filename_ = SystemToolkit::home_path() + SystemToolkit::date_time_string() + "_vimix.png";
}


// Thread to perform slow operation of saving to file
void save_png(std::string filename, unsigned char *data, uint w, uint h, uint c)
{
    // got data to save ?
    if (data) {
        // save file
        stbi_write_png(filename.c_str(), w, h, c, data, w * c);
        // notify
        Log::Notify("Capture %s saved.", filename.c_str());
        // done
        free(data);
    }
}

void PNGRecorder::addFrame(FrameBuffer *frame_buffer, float)
{
    if (enabled_)
    {
        uint w = frame_buffer->width();
        uint h = frame_buffer->height();
        uint c = frame_buffer->use_alpha() ? 4 : 3;
        GLenum format = frame_buffer->use_alpha() ? GL_RGBA : GL_RGB;
        uint size = w * h * c;
        unsigned char * data = (unsigned char*) malloc(size);

        glGetTextureSubImage( frame_buffer->texture(), 0, 0, 0, 0, w, h, 1, format, GL_UNSIGNED_BYTE, size, data);

        // save in separate thread
        std::thread(save_png, filename_, data, w, h, c).detach();
    }

    // record one frame only
    finished_ = true;
}



H264Recorder::H264Recorder() : Recorder(), frame_buffer_(nullptr), width_(0), height_(0), buf_size_(0),
    recording_(false), pipeline_(nullptr), src_(nullptr), timestamp_(0), time_(0), accept_buffer_(false)
{
    // auto filename
    filename_ = SystemToolkit::home_path() + SystemToolkit::date_time_string() + "_vimix.mov";

    // configure H264stream
    frame_duration_ = gst_util_uint64_scale_int (1, GST_SECOND, 30);  // 30 FPS
    time_ = 2 * frame_duration_;

}

H264Recorder::~H264Recorder()
{
    if (pipeline_ != nullptr) {
        gst_element_set_state (pipeline_, GST_STATE_NULL);
        gst_object_unref (pipeline_);
    }
    if (src_ != nullptr)
        gst_object_unref (src_);

}


void H264Recorder::addFrame (FrameBuffer *frame_buffer, float dt)
{
    // TODO : avoid software videoconvert by using a GPU shader to produce Y444 frames

    // ignore
    if (frame_buffer == nullptr)
        return;

    // first frame for initialization
   if (frame_buffer_ == nullptr) {

       // accepting a new frame buffer as input
       frame_buffer_ = frame_buffer;

       // define stream properties
       width_ = frame_buffer_->width();
       height_ = frame_buffer_->height();
       buf_size_ = width_ * height_ * (frame_buffer_->use_alpha() ? 4 : 3);

       // create a gstreamer pipeline

       // Control x264 encoder quality :
       // pass=5
       //    quant (4) – Constant Quantizer
       //    qual  (5) – Constant Quality
       // quantizer=25
       //   The total range is from 0 to 51, where 0 is lossless, 18 can be considered ‘visually lossless’,
       //   and 51 is terrible quality. A sane range is 18-26, and the default is 23.
       // speed-preset
       //    veryfast (3)
       //    faster (4)
       //    fast (5)
//       string description = "appsrc name=src ! videoconvert ! x264enc pass=5 quantizer=25 speed-preset=6 ! video/x-h264, profile=high ! qtmux ! filesink name=sink";

       string description = "appsrc name=src ! videoconvert ! vp9enc cq-level=35 threads=4 cpu-used=4 end-usage=cq ! video/x-vp9 ! qtmux ! filesink name=sink";

       //       string description = "appsrc name=src ! videoconvert ! avenc_prores ! qtmux ! filesink name=sink";


//       string description = "appsrc name=src ! videoconvert ! x265enc tune=4 speed-preset=4 ! video/x-h265, profile=main ! matroskamux ! filesink name=sink";



       // parse pipeline descriptor
       GError *error = NULL;
       pipeline_ = gst_parse_launch (description.c_str(), &error);
       if (error != NULL) {
           Log::Warning("H264Recorder Could not construct pipeline %s:\n%s", description.c_str(), error->message);
           g_clear_error (&error);
           finished_ = true;
           return;
       }

       // setup file sink
       sink_ = GST_BASE_SINK( gst_bin_get_by_name (GST_BIN (pipeline_), "sink") );
       if (sink_) {
           g_object_set (G_OBJECT (sink_),
                         "location", filename_.c_str(),
                         "sync", FALSE,
                         NULL);
       }
       else {
           Log::Warning("H264Recorder Could not configure file");
           finished_ = true;
           return;
       }

       // setup custom app source
       src_ = GST_APP_SRC( gst_bin_get_by_name (GST_BIN (pipeline_), "src") );
       if (src_) {

           g_object_set (G_OBJECT (src_),
                         "stream-type", GST_APP_STREAM_TYPE_STREAM,
                         "is-live", TRUE,
                         "format", GST_FORMAT_TIME,
                         //                     "do-timestamp", TRUE,
                         //                         "size", 120,
                         NULL);

           // 2 sec buffer
           gst_app_src_set_max_bytes( src_, 60 * buf_size_);

           // instruct src to use the required caps
           GstCaps *caps = gst_caps_new_simple ("video/x-raw",
                                "format", G_TYPE_STRING, "RGB",
                                "width",  G_TYPE_INT, width_,
                                "height", G_TYPE_INT, height_,
//                                "framerate", GST_TYPE_FRACTION, 30, 1,
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
           Log::Warning("H264Recorder Could not configure capture source");
           finished_ = true;
           return;
       }

       // start recording
       GstStateChangeReturn ret = gst_element_set_state (pipeline_, GST_STATE_PLAYING);
       if (ret == GST_STATE_CHANGE_FAILURE) {
           Log::Warning("H264Recorder Could not record %s", filename_.c_str());
           finished_ = true;
           return;
       }

       // all good
       Log::Info("H264Recorder start recording (%d x %d)", width_, height_);

       // start recording !!
       recording_ = true;
   }
   // frame buffer changed ?
   else if (frame_buffer_ != frame_buffer) {

       // if an incompatilble frame buffer given: stop recorder
       if ( frame_buffer->width() != width_ ||
            frame_buffer->height() != height_ ||
            frame_buffer->use_alpha() != frame_buffer_->use_alpha()) {

           enabled_ = false;
           recording_ = false;
       }
   }

   static int count = 0;
   // store a frame if enabled
   if (enabled_ && buf_size_ > 0)
   {
       // calculate dt in ns
       time_ +=  gst_gdouble_to_guint64( dt * 1000000.f);

       // if time is passed one frame duration (with 10% margin)
       if ( time_ > frame_duration_ - 3000000 ) {

           GstBuffer *buffer = gst_buffer_new_and_alloc (buf_size_);
           GLenum format = frame_buffer_->use_alpha() ? GL_RGBA : GL_RGB;

           // set timing of buffer
           buffer->pts = timestamp_;
           buffer->duration = frame_duration_;

           // OpenGL capture
           GstMapInfo map;
           gst_buffer_map (buffer, &map, GST_MAP_WRITE);
           glGetTextureSubImage( frame_buffer_->texture(), 0, 0, 0, 0, width_, height_, 1, format, GL_UNSIGNED_BYTE, buf_size_, map.data);
           gst_buffer_unmap (buffer, &map);


           // push the buffer if the appsrc accepts
           if (accept_buffer_)
           {
               Log::Info("H264Recorder push data %ld", buffer->pts);
               // push
               gst_app_src_push_buffer (src_, buffer);
               // NB: buffer will be unrefed by the appsrc

               // TODO : detect user request for end
               count++;
               if (count > 120)
               {
                   Log::Info("H264Recorder push EOS");
                   gst_app_src_end_of_stream (src_);

                   recording_ = false;
               }

           }
           // the appsrc refuses to take anymore buffer
           else {
               // drop frame
               gst_buffer_unref (buffer);

               // TODO: if encoding is too busy, maybe we should stack the frame instead of skipping it
           }

           // restart counter
           time_ = 0;
           // next timestamp
           timestamp_ += frame_duration_;

       }

   }

   // did the recording terminate with sink receiving end-of-stream ?
   if ( !recording_ && sink_->eos)
   {
       // stop the pipeline
       GstStateChangeReturn ret = gst_element_set_state (pipeline_, GST_STATE_NULL);
       if (ret == GST_STATE_CHANGE_FAILURE)
           Log::Warning("H264Recorder Could not stop");
       else
           Log::Notify("H264Recording finished");

       count = 0;
       finished_ = true;
   }

}

// appsrc needs data and we should start sending
void H264Recorder::callback_need_data (GstAppSrc *src, guint length, gpointer p)
{
    Log::Info("H264Recording callback_need_data");
    H264Recorder *rec = (H264Recorder *)p;
    if (rec) {
        rec->accept_buffer_ = rec->recording_ ? true : false;
    }
}


// appsrc has enough data and we can stop sending
void H264Recorder::callback_enough_data (GstAppSrc *src, gpointer p)
{
    Log::Info("H264Recording callback_enough_data");
    H264Recorder *rec = (H264Recorder *)p;
    if (rec) {
        rec->accept_buffer_ = false;
    }
}

