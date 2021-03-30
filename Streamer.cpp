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

void StreamingRequestListener::ProcessMessage( const osc::ReceivedMessage& m,
                                               const IpEndpointName& remoteEndpoint )
{
    char sender[IpEndpointName::ADDRESS_AND_PORT_STRING_LENGTH];
    remoteEndpoint.AddressAndPortAsString(sender);

    try{

        if( std::strcmp( m.AddressPattern(), OSC_PREFIX OSC_STREAM_REQUEST) == 0 ){

#ifdef STREAMER_DEBUG
            Log::Info("%s wants a stream.", sender);
#endif
            osc::ReceivedMessage::const_iterator arg = m.ArgumentsBegin();
            int reply_to_port = (arg++)->AsInt32();
            const char *client_name = (arg++)->AsString();
            if (Streaming::manager().enabled())
                Streaming::manager().addStream(sender, reply_to_port, client_name);
            else
                Streaming::manager().refuseStream(sender, reply_to_port);
        }
        else if( std::strcmp( m.AddressPattern(), OSC_PREFIX OSC_STREAM_DISCONNECT) == 0 ){

#ifdef STREAMER_DEBUG
            Log::Info("%s does not need streaming anymore.", sender);
#endif
            osc::ReceivedMessage::const_iterator arg = m.ArgumentsBegin();
            int port = (arg++)->AsInt32();
            Streaming::manager().removeStream(sender, port);

        }
    }
    catch( osc::Exception& e ){
        // any parsing errors such as unexpected argument types, or
        // missing arguments get thrown as exceptions.
        Log::Info("error while parsing message '%s' from %s : %s", m.AddressPattern(), sender, e.what());
    }
}

void wait_for_request_(UdpListeningReceiveSocket *receiver)
{
    receiver->Run();
}

Streaming::Streaming() : enabled_(false)
{
    int port = Connection::manager().info().port_stream_request;
    receiver_ = new UdpListeningReceiveSocket(IpEndpointName( IpEndpointName::ANY_ADDRESS, port ), &listener_ );

    std::thread(wait_for_request_, receiver_).detach();
}

Streaming::~Streaming()
{
    if (receiver_!=nullptr) {
        receiver_->Break();
        delete receiver_;
    }
}

bool Streaming::busy()
{
    bool b = false;

    streamers_lock_.lock();
    std::vector<VideoStreamer *>::const_iterator sit = streamers_.begin();
    for (; sit != streamers_.end() && !b; sit++)
        b = (*sit)->busy() ;
    streamers_lock_.unlock();

    return b;
}


std::vector<std::string> Streaming::listStreams()
{
    std::vector<std::string>  ls;

    streamers_lock_.lock();
    std::vector<VideoStreamer *>::const_iterator sit = streamers_.begin();
    for (; sit != streamers_.end(); sit++)
        ls.push_back( (*sit)->info() );
    streamers_lock_.unlock();

    return ls;
}

void Streaming::enable(bool on)
{
    if (on) {
        // accept streaming requests
        enabled_ = true;
        Log::Info("Accepting stream requests to %s.", Connection::manager().info().name.c_str());
    }
    else {
        // refuse streaming requests
        enabled_ = false;
        // ending and removing all streaming
        streamers_lock_.lock();
        for (auto sit = streamers_.begin(); sit != streamers_.end(); sit=streamers_.erase(sit))
            (*sit)->stop();
        streamers_lock_.unlock();
        Log::Info("Refusing stream requests to %s. No streaming ongoing.", Connection::manager().info().name.c_str());
    }
}

void Streaming::removeStream(const std::string &sender, int port)
{
    // get ip of sender
    std::string sender_ip = sender.substr(0, sender.find_last_of(":"));

    // parse the list for a streamers matching IP and port
    streamers_lock_.lock();
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
    streamers_lock_.unlock();

}

void Streaming::removeStreams(const std::string &clientname)
{
    // remove all streamers matching given IP
    streamers_lock_.lock();
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
    streamers_lock_.unlock();
}

void Streaming::removeStream(const VideoStreamer *vs)
{
    if ( vs!= nullptr && streamers_lock_.try_lock()) {

        std::vector<VideoStreamer *>::const_iterator sit = streamers_.begin();
        while ( sit != streamers_.end() ){
            if ( *(sit) == vs) {
#ifdef STREAMER_DEBUG
                NetworkToolkit::StreamConfig config = vs->config_;
                Log::Info("Ending streaming to %s:%d", config.client_address.c_str(), config.port);
#endif
                // remove from list
                streamers_.erase(sit);
                break;
            }
            else
                sit++;
        }
        streamers_lock_.unlock();
    }
}

void Streaming::refuseStream(const std::string &sender, int reply_to)
{
    // get ip of client
    std::string sender_ip = sender.substr(0, sender.find_last_of(":"));
    // prepare to reply to client
    IpEndpointName host( sender_ip.c_str(), reply_to );
    UdpTransmitSocket socket( host );
    // build OSC message
    char buffer[IP_MTU_SIZE];
    osc::OutboundPacketStream p( buffer, IP_MTU_SIZE );
    p.Clear();
    p << osc::BeginMessage( OSC_PREFIX OSC_STREAM_REJECT );
    p << osc::EndMessage;
    // send OSC message to client
    socket.Send( p.Data(), p.Size() );
    // inform user
    Log::Warning("A connection request for streaming came and was rejected.\nYou can Accept connections from the Output window.");
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
    conf.width = FrameGrabbing::manager().width();
    conf.height = FrameGrabbing::manager().height();

    // TEMP DISABLED : TODO Fix snap to allow system wide shared access

    // offer SHM if same IP that our host IP (i.e. on the same machine)
    //    if( NetworkToolkit::is_host_ip(conf.client_address) )
    //        conf.protocol = NetworkToolkit::SHM_RAW;
    //    //  any other IP : offer network streaming
    //    else
        conf.protocol = NetworkToolkit::UDP_JPEG;

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
    Log::Info("Replying to %s:%d", sender_ip.c_str(), reply_to);
    Log::Info("Starting streaming to %s:%d", sender_ip.c_str(), conf.port);
#endif

    // create streamer & remember it
    VideoStreamer *streamer = new VideoStreamer(conf);
    streamers_lock_.lock();
    streamers_.push_back(streamer);
    streamers_lock_.unlock();

    // start streamer
    FrameGrabbing::manager().add(streamer);
}


VideoStreamer::VideoStreamer(NetworkToolkit::StreamConfig conf): FrameGrabber(), config_(conf), stopped_(false)
{
}

void VideoStreamer::init(GstCaps *caps)
{
    // ignore
    if (caps == nullptr)
        return;

    // check that config matches the given buffer properties
    gint w = 0, h = 0;
    GstStructure *capstruct = gst_caps_get_structure (caps, 0);
    if ( gst_structure_has_field (capstruct, "width"))
        gst_structure_get_int (capstruct, "width", &w);
    if ( gst_structure_has_field (capstruct, "height"))
        gst_structure_get_int (capstruct, "height", &h);
    if ( config_.width != w || config_.height != h) {
        Log::Warning("Streaming cannot start: given frames (%d x %d) incompatible with stream (%d x %d)",
                     w, w, config_.width, config_.height);
        finished_ = true;
        return;
    }

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
        std::string path = SystemToolkit::full_filename(SystemToolkit::temp_path(), "shm");
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
                      //                     "do-timestamp", TRUE,
                      NULL);

        // Direct encoding (no buffering)
        gst_app_src_set_max_bytes( src_, 0 );

        // instruct src to use the required caps
        caps_ = gst_caps_copy( caps );
        gst_app_src_set_caps (src_, caps_);

        // setup callbacks
        GstAppSrcCallbacks callbacks;
        callbacks.need_data = FrameGrabber::callback_need_data;
        callbacks.enough_data = FrameGrabber::callback_enough_data;
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
    Log::Notify("Streaming to %s.", config_.client_name.c_str());

    // start streaming !!
    active_ = true;
}

void VideoStreamer::terminate()
{
    // send EOS
    gst_app_src_end_of_stream (src_);

    // make sure the shared memory socket is deleted
    if (config_.protocol == NetworkToolkit::SHM_RAW) {
        std::string path = SystemToolkit::full_filename(SystemToolkit::temp_path(), "shm");
        path += std::to_string(config_.port);
        SystemToolkit::remove_file(path);
    }

    Log::Notify("Streaming to %s finished after %s s.", config_.client_name.c_str(),
                GstToolkit::time_to_string(timestamp_).c_str());
}

void VideoStreamer::stop ()
{
    // stop recording
    FrameGrabber::stop ();

    // inform streaming manager to remove myself
    // NB: will not be effective if called inside a locked streamers_lock_
    Streaming::manager().removeStream(this);

    // force finished
    finished_ = true;
}

std::string VideoStreamer::info() const
{
    std::ostringstream ret;
    if (active_) {
        ret << NetworkToolkit::protocol_name[config_.protocol];
        ret << " to ";
        ret << config_.client_name;
    }
    else
        ret <<  "Streaming terminated.";
    return ret.str();
}
