#include <thread>

//  Desktop OpenGL function loader
#include <glad/glad.h>

// standalone image loader
#include <stb_image.h>
#include <stb_image_write.h>

// gstreamer
#include <gst/gstformat.h>
#include <gst/video/video.h>

//osc
#include "osc/OscOutboundPacketStream.h"

#include "Settings.h"
#include "GstToolkit.h"
#include "defines.h"
#include "SystemToolkit.h"
#include "Session.h"
#include "FrameBuffer.h"
#include "Log.h"

#include "NetworkToolkit.h"
#include "Streamer.h"

#include <iostream>
#include <cstring>

// this is called when a U
void StreamingRequestListener::ProcessMessage( const osc::ReceivedMessage& m,
                                               const IpEndpointName& remoteEndpoint )
{
    char sender[IpEndpointName::ADDRESS_AND_PORT_STRING_LENGTH];
    remoteEndpoint.AddressAndPortAsString(sender);

    try{

        if( std::strcmp( m.AddressPattern(), OSC_PREFIX OSC_STREAM_REQUEST) == 0 ){

            Log::Info("%s wants to know if i can stream", sender);
            Streaming::manager().makeOffer(sender);

        }
        else if( std::strcmp( m.AddressPattern(), OSC_PREFIX OSC_STREAM_CONNECT) == 0 ){

            Log::Info("%s wants me stream", sender);

        }
        else if( std::strcmp( m.AddressPattern(), OSC_PREFIX OSC_STREAM_DISCONNECT) == 0 ){

            Log::Info("%s ended reception", sender);

        }
    }
    catch( osc::Exception& e ){
        // any parsing errors such as unexpected argument types, or
        // missing arguments get thrown as exceptions.
        Log::Info("error while parsing message '%s' from %s : %s", m.AddressPattern(), sender, e.what());
    }
}


Streaming::Streaming() : session_(nullptr), width_(0), height_(0)
{
//    receiver_ = new UdpListeningReceiveSocket(IpEndpointName( IpEndpointName::ANY_ADDRESS, STREAM_REQUEST_PORT ), &listener_ );
////    receiver_ = new UdpListeningReceiveSocket(IpEndpointName( IpEndpointName::ANY_ADDRESS, STREAM_REQUEST_PORT ), &listener_ );



}

void Streaming::listen()
{
    Log::Info("Accepting Streaming requests on port %d", STREAM_REQUEST_PORT);
    Streaming::manager().receiver_->Run();
    Log::Info("Refusing Streaming requests");
}

void Streaming::enable(bool on)
{
//    if (on){
//        std::thread(listen).detach();
//    }
//    else {
//        // end streaming requests
//        receiver_->AsynchronousBreak();
//        // end all streaming TODO
//    }
}


void Streaming::setSession(Session *se)
{
    if (se != nullptr && session_ != se) {
        session_ = se;
        FrameBuffer *f = session_->frame();
        width_ = f->width();
        height_ = f->height();
    }
    else {
        session_ = nullptr;
        width_ = 0;
        height_ = 0;
    }
}

void Streaming::makeOffer(const std::string &client)
{
    // do not reply if no session can be streamed (should not happen)
    if (session_ == nullptr)
        return;

    // get ip of client
    std::string clientip = client.substr(0, client.find_last_of(":"));

    // get port used to request
    std::string clientport_s = client.substr(client.find_last_of(":") + 1);

    // prepare to reply to client
    IpEndpointName host( clientip.c_str(), STREAM_RESPONSE_PORT );
    UdpTransmitSocket socket( host );

    // prepare an offer
    offer o;
    o.client = clientip;
    o.width = width_;
    o.height = height_;
    // offer SHM if same IP that our host IP (i.e. on the same machine)
    if( NetworkToolkit::is_host_ip(clientip) ){
        // this port seems free, so re-use it!
        o.address = "localhost:" + clientport_s;
        o.protocol = NetworkToolkit::SHM_RAW;
    }
    //  any other IP : offer UDP streaming
    else {
        o.address = NetworkToolkit::closest_host_ip(clientip) + ":" + clientport_s;
        o.protocol = NetworkToolkit::TCP_JPEG;
    }

    // build message
    char buffer[IP_MTU_SIZE];
    osc::OutboundPacketStream p( buffer, IP_MTU_SIZE );
    p.Clear();
    p << osc::BeginMessage( OSC_PREFIX OSC_STREAM_OFFER );
    p << o.address.c_str();
    p << (int) o.protocol;
    p << o.width << o.height;
    p << osc::EndMessage;

    // send OSC message
    socket.Send( p.Data(), p.Size() );
}

void Streaming::addStream(const std::string &address)
{
}

void Streaming::removeStream(const std::string &address)
{

}

VideoStreamer::VideoStreamer(Streaming::offer config): FrameGrabber(), frame_buffer_(nullptr), width_(0), height_(0),
        streaming_(false), accept_buffer_(false), pipeline_(nullptr), src_(nullptr), timestamp_(0)
{
    // configure fix parameter
    frame_duration_ = gst_util_uint64_scale_int (1, GST_SECOND, 30);  // 30 FPS
    timeframe_ = 2 * frame_duration_;

    protocol_ = config.protocol;
    address_ = config.address;
    width_ = config.width;
    height_ = config.height;
}

VideoStreamer::~VideoStreamer()
{
    stop();

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

       if (protocol_ < 0 || protocol_ >= NetworkToolkit::DEFAULT)
           protocol_ = NetworkToolkit::TCP_JPEG;
       description += NetworkToolkit::protocol_send_pipeline[protocol_];

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
       if (protocol_ == NetworkToolkit::TCP_JPEG || protocol_ == NetworkToolkit::TCP_H264) {
           // extract host and port from "host:port"
           std::string ip = address_.substr(0, address_.find_last_of(":"));
           std::string port_s = address_.substr(address_.find_last_of(":") + 1);
           uint port = std::stoi(port_s);
           g_object_set (G_OBJECT (gst_bin_get_by_name (GST_BIN (pipeline_), "sink")),
                         "host", ip.c_str(),
                         "port", port,  NULL);
       }
       else if (protocol_ == NetworkToolkit::SHM_RAW) {
           std::string path = SystemToolkit::full_filename(SystemToolkit::settings_path(), "shm_socket");
           SystemToolkit::remove_file(path);
           g_object_set (G_OBJECT (gst_bin_get_by_name (GST_BIN (pipeline_), "sink")),
                         "socket-path", path.c_str(),  NULL);
       }

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
       Log::Info("VideoStreamer start (%s %d x %d)", NetworkToolkit::protocol_name[protocol_], width_, height_);

Log::Info("%s", description.c_str());

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
//               Log::Info("VideoRecorder push data %ld", buffer->pts);
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
               Log::Warning("VideoStreamer Could not stop");
           else
               Log::Notify("Stream finished after %s s.", GstToolkit::time_to_string(timestamp_).c_str());

           finished_ = true;
       }

       // make sure the shared memory socket is deleted
       if (protocol_ == NetworkToolkit::SHM_RAW) {
           std::string path = SystemToolkit::full_filename(SystemToolkit::settings_path(), "shm_socket");
           SystemToolkit::remove_file(path);
       }
   }


}

void VideoStreamer::stop ()
{
    // send end of stream
    if (src_)
        gst_app_src_end_of_stream (src_);

    // stop recording
    streaming_ = false;
}

std::string VideoStreamer::info()
{
    std::string ret = "Streaming terminated.";
    if (streaming_) {

        if (protocol_ == NetworkToolkit::TCP_JPEG || protocol_ == NetworkToolkit::TCP_H264) {


            ret = "TCP";

        }
        else if (protocol_ == NetworkToolkit::SHM_RAW) {
            ret = "Shared Memory";
        }

    }
    return ret;
}


double VideoStreamer::duration()
{
    return gst_guint64_to_gdouble( GST_TIME_AS_MSECONDS(timestamp_) ) / 1000.0;
}

bool VideoStreamer::busy()
{
    return accept_buffer_ ? true : false;
}

// appsrc needs data and we should start sending
void VideoStreamer::callback_need_data (GstAppSrc *, guint , gpointer p)
{
    VideoStreamer *rec = (VideoStreamer *)p;
    if (rec) {
        rec->accept_buffer_ =  true;
//        Log::Info("VideoStreamer need_data");
    }
}

// appsrc has enough data and we can stop sending
void VideoStreamer::callback_enough_data (GstAppSrc *, gpointer p)
{
//    Log::Info("VideoStreamer enough_data");
    VideoStreamer *rec = (VideoStreamer *)p;
    if (rec) {
        rec->accept_buffer_ = false;
    }
}
