#include <thread>
#include <sstream>

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

#include "Connection.h"
#include "NetworkToolkit.h"
#include "Streamer.h"

#include <iostream>
#include <cstring>

#ifndef NDEBUG
#define STREAMER_DEBUG
#endif

// this is called when a U
void StreamingRequestListener::ProcessMessage( const osc::ReceivedMessage& m,
                                               const IpEndpointName& remoteEndpoint )
{
    char sender[IpEndpointName::ADDRESS_AND_PORT_STRING_LENGTH];
    remoteEndpoint.AddressAndPortAsString(sender);

    try{

        if( std::strcmp( m.AddressPattern(), OSC_PREFIX OSC_STREAM_REQUEST) == 0 ){

            osc::ReceivedMessage::const_iterator arg = m.ArgumentsBegin();
            int reply_to_port = (arg++)->AsInt32();
            const char *client_name = (arg++)->AsString();
            Streaming::manager().addStream(sender, reply_to_port, client_name);

#ifdef STREAMER_DEBUG
            Log::Info("%s wants a stream.", sender);
#endif
        }
        else if( std::strcmp( m.AddressPattern(), OSC_PREFIX OSC_STREAM_DISCONNECT) == 0 ){

            osc::ReceivedMessage::const_iterator arg = m.ArgumentsBegin();
            int port = (arg++)->AsInt32();
            Streaming::manager().removeStream(sender, port);

#ifdef STREAMER_DEBUG
            Log::Info("%s does not need streaming anymore.", sender);
#endif
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
    int port = Connection::manager().info().port_stream_request;
    receiver_ = new UdpListeningReceiveSocket(IpEndpointName( IpEndpointName::ANY_ADDRESS, port ), &listener_ );

}


bool Streaming::busy() const
{
    bool b = false;

    std::vector<VideoStreamer *>::const_iterator sit = streamers_.begin();
    for (; sit != streamers_.end() && !b; sit++)
        b = (*sit)->busy() ;

    return b;
}


std::vector<std::string> Streaming::listStreams() const
{
    std::vector<std::string>  ls;

    std::vector<VideoStreamer *>::const_iterator sit = streamers_.begin();
    for (; sit != streamers_.end(); sit++)
        ls.push_back( (*sit)->info() );

    return ls;
}

void wait_for_request_(UdpListeningReceiveSocket *receiver)
{
    receiver->Run();
}

void Streaming::enable(bool on)
{
    if (on) {
        std::thread(wait_for_request_, receiver_).detach();
        Log::Info("Accepting stream requests to %s.", Connection::manager().info().name.c_str());
    }
    else {
        // end streaming requests
        receiver_->AsynchronousBreak();
        // ending and removing all streaming
        for (auto sit = streamers_.begin(); sit != streamers_.end(); sit=streamers_.erase(sit))
            (*sit)->stop();
        Log::Info("Refusing stream requests to %s. No streaming ongoing.", Connection::manager().info().name.c_str());
    }
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

void Streaming::removeStream(const std::string &sender, int port)
{
    // get ip of sender
    std::string sender_ip = sender.substr(0, sender.find_last_of(":"));

    // parse the list for a streamers matching IP and port
    std::vector<VideoStreamer *>::const_iterator sit = streamers_.begin();
    for (; sit != streamers_.end(); sit++){
        NetworkToolkit::StreamConfig config = (*sit)->config_;
        if (config.client_address.compare(sender_ip) == 0 && config.port == port ) {
#ifdef STREAMER_DEBUG
            Log::Info("Ending streaming to %s:%d", config.client_address.c_str(), config.port);
#endif
            // match: stop this streamer
            (*sit)->stop();
            // remove from list
            streamers_.erase(sit);
            break;
        }
    }

}

void Streaming::removeStreams(const std::string &clientname)
{
    // remove all streamers matching given IP
    std::vector<VideoStreamer *>::const_iterator sit = streamers_.begin();
    while ( sit != streamers_.end() ){
        NetworkToolkit::StreamConfig config = (*sit)->config_;
        if (config.client_name.compare(clientname) == 0) {
#ifdef STREAMER_DEBUG
            Log::Info("Ending streaming to %s:%d", config.client_address.c_str(), config.port);
#endif
            // match: stop this streamer
            (*sit)->stop();
            // remove from list
            sit = streamers_.erase(sit);
        }
        else
            sit++;
    }
}


void Streaming::addStream(const std::string &sender, int reply_to, const std::string &clientname)
{
    // get ip of client
    std::string sender_ip = sender.substr(0, sender.find_last_of(":"));
    // get port used to send the request
    std::string sender_port = sender.substr(sender.find_last_of(":") + 1);

    // prepare to reply to client
    IpEndpointName host( sender_ip.c_str(), reply_to );
    UdpTransmitSocket socket( host );

    // prepare an offer
    NetworkToolkit::StreamConfig conf;
    conf.client_address = sender_ip;
    conf.client_name = clientname;
    conf.port = std::stoi(sender_port); // this port seems free, so re-use it!
    conf.width = width_;
    conf.height = height_;
    // offer SHM if same IP that our host IP (i.e. on the same machine)
    if( NetworkToolkit::is_host_ip(conf.client_address) ) {
        conf.protocol = NetworkToolkit::SHM_RAW;
    }
    //  any other IP : offer UDP streaming
    else {
        conf.protocol = NetworkToolkit::UDP_JPEG;
    }
//    conf.protocol = NetworkToolkit::UDP_JPEG; // force udp for testing

    // build OSC message
    char buffer[IP_MTU_SIZE];
    osc::OutboundPacketStream p( buffer, IP_MTU_SIZE );
    p.Clear();
    p << osc::BeginMessage( OSC_PREFIX OSC_STREAM_OFFER );
    p << conf.port;
    p << (int) conf.protocol;
    p << conf.width << conf.height;
    p << osc::EndMessage;

    // send OSC message to client
    socket.Send( p.Data(), p.Size() );

#ifdef STREAMER_DEBUG
    Log::Info("Starting streaming to %s:%d", sender_ip.c_str(), conf.port);
#endif

    // create streamer & remember it
    VideoStreamer *streamer = new VideoStreamer(conf);
    streamers_.push_back(streamer);

    // start streamer
    session_->addFrameGrabber(streamer);
}


VideoStreamer::VideoStreamer(NetworkToolkit::StreamConfig conf): FrameGrabber(), frame_buffer_(nullptr), width_(0), height_(0),
        streaming_(false), accept_buffer_(false), pipeline_(nullptr), src_(nullptr), timestamp_(0)
{
    // configure fix parameter
    frame_duration_ = gst_util_uint64_scale_int (1, GST_SECOND, 30);  // 30 FPS
    timeframe_ = 2 * frame_duration_;

    config_ = conf;
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

       // define frames properties
       width_ = frame_buffer_->width();
       height_ = frame_buffer_->height();
       size_ = width_ * height_ * (frame_buffer_->use_alpha() ? 4 : 3);

       // if an incompatilble frame buffer given: cancel streaming
       if ( config_.width != width_ || config_.height != height_) {
           Log::Warning("Streaming cannot start: given frames (%d x %d) incompatible with stream (%d x %d)",
                        width_, height_, config_.width, config_.height);
           finished_ = true;
           return;
       }

       // create PBOs
       glGenBuffers(2, pbo_);
       glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo_[1]);
       glBufferData(GL_PIXEL_PACK_BUFFER, size_, NULL, GL_STREAM_READ);
       glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo_[0]);
       glBufferData(GL_PIXEL_PACK_BUFFER, size_, NULL, GL_STREAM_READ);

       // prevent eroneous protocol values
       if (config_.protocol < 0 || config_.protocol >= NetworkToolkit::DEFAULT)
           config_.protocol = NetworkToolkit::UDP_JPEG;

       // create a gstreamer pipeline
       std::string description = "appsrc name=src ! videoconvert ! ";
       description += NetworkToolkit::protocol_send_pipeline[config_.protocol];

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
       if (config_.protocol == NetworkToolkit::UDP_JPEG || config_.protocol == NetworkToolkit::UDP_H264) {
           g_object_set (G_OBJECT (gst_bin_get_by_name (GST_BIN (pipeline_), "sink")),
                         "host", config_.client_address.c_str(),
                         "port", config_.port,  NULL);
       }
       else if (config_.protocol == NetworkToolkit::SHM_RAW) {
           // TODO rename SHM socket "shm_PORT"
           std::string path = SystemToolkit::full_filename(SystemToolkit::settings_path(), "shm");
           path += std::to_string(config_.port);
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
       Log::Info("Streaming video to %s (%d x %d)",
                 config_.client_name.c_str(), width_, height_);

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
   // did the streaming receive end-of-stream ?
   else if (!finished_)
   {
       // Wait for EOS message
       GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline_));
       GstMessage *msg = gst_bus_poll(bus, GST_MESSAGE_EOS, GST_TIME_AS_USECONDS(4));

       if (msg) {
           // stop the pipeline
           GstStateChangeReturn ret = gst_element_set_state (pipeline_, GST_STATE_NULL);
#ifdef STREAMER_DEBUG
           if (ret == GST_STATE_CHANGE_FAILURE)
               Log::Info("Streaming to %s:%d could not stop properly.", config_.client_address.c_str(), config_.port);
           else
               Log::Info("Streaming to %s:%d ending...", config_.client_address.c_str(), config_.port);
#endif
           finished_ = true;
       }
   }
   // finished !
   else {

       // send EOS
       gst_app_src_end_of_stream (src_);

       // make sure the shared memory socket is deleted
       if (config_.protocol == NetworkToolkit::SHM_RAW) {
           std::string path = SystemToolkit::full_filename(SystemToolkit::settings_path(), "shm");
           path += std::to_string(config_.port);
           SystemToolkit::remove_file(path);
       }

       Log::Notify("Streaming to %s finished after %s s.", config_.client_name.c_str(),
                   GstToolkit::time_to_string(timestamp_).c_str());

   }


}

void VideoStreamer::stop ()
{
    // stop recording
    streaming_ = false;
    finished_ = true;

}

std::string VideoStreamer::info()
{
    std::ostringstream ret;
    if (streaming_) {
        ret << NetworkToolkit::protocol_name[config_.protocol];
        ret << " to ";
        ret << config_.client_name;
    }
    else
        ret <<  "Streaming terminated.";
    return ret.str();
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
