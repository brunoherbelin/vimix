#include <algorithm>
#include <sstream>
#include <thread>
#include <chrono>
#include <future>

#include <glm/gtc/matrix_transform.hpp>

#include <gst/pbutils/pbutils.h>
#include <gst/gst.h>

#include "SystemToolkit.h"
#include "defines.h"
#include "Stream.h"
#include "Decorations.h"
#include "Visitor.h"
#include "Log.h"
#include "Connection.h"

#include "NetworkSource.h"

#ifndef NDEBUG
#define NETWORK_DEBUG
#endif



// this is called when receiving an answer for streaming request
void StreamerResponseListener::ProcessMessage( const osc::ReceivedMessage& m,
                                               const IpEndpointName& remoteEndpoint )
{
    char sender[IpEndpointName::ADDRESS_AND_PORT_STRING_LENGTH];
    remoteEndpoint.AddressAndPortAsString(sender);

    try{
        if( std::strcmp( m.AddressPattern(), OSC_PREFIX OSC_STREAM_OFFER ) == 0 ){

            NetworkToolkit::StreamConfig conf;

            // someone is offering a stream
            osc::ReceivedMessage::const_iterator arg = m.ArgumentsBegin();
            conf.port     = (arg++)->AsInt32();
            conf.protocol = (NetworkToolkit::Protocol) (arg++)->AsInt32();
            conf.width    = (arg++)->AsInt32();
            conf.height   = (arg++)->AsInt32();

            // we got the offer from Streaming::manager()
            parent_->setConfig(conf);
#ifdef NETWORK_DEBUG
            Log::Info("Received stream info from %s", sender);
#endif
        }
    }
    catch( osc::Exception& e ){
        // any parsing errors such as unexpected argument types, or
        // missing arguments get thrown as exceptions.
        Log::Info("error while parsing message '%s' from %s : %s", m.AddressPattern(), sender, e.what());
    }
}


NetworkStream::NetworkStream(): Stream(), receiver_(nullptr)
{
//    listener_port_ = 5400; // TODO Find a free port, unique each time
    confirmed_ = false;
}

glm::ivec2 NetworkStream::resolution() const
{
    return glm::ivec2(config_.width, config_.height);
}

void wait_for_stream_(UdpListeningReceiveSocket *receiver)
{
    receiver->Run();
}

void NetworkStream::open(const std::string &nameconnection)
{
    // does this Connection exists?
    int streamer_index = Connection::manager().index(nameconnection);

    // Nope, cannot connect to unknown connection
    if (streamer_index < 0) {
        Log::Notify("Cannot connect to %s: please make sure the program is active.", nameconnection);
        failed_ = true;
        return;
    }

    // prepare listener to receive stream config from remote streaming manager
    listener_.setParent(this);

    // find an available port to receive response from remote streaming manager
    int listener_port_ = -1;
    for (int trial = 0; receiver_ == nullptr && trial < 10 ; trial++) {
        try {
            // invent a port which would be available
            listener_port_ = 72000 + rand()%1000;
            // try to create receiver (through exception on fail)
            receiver_ = new UdpListeningReceiveSocket(IpEndpointName(Connection::manager().info().address.c_str(), listener_port_), &listener_);
        }
        catch (const std::runtime_error&) {
            receiver_ = nullptr;
        }
    }
    if (receiver_ == nullptr) {
        Log::Notify("Cannot open %s. Please try again.", nameconnection);
        failed_ = true;
        return;
    }

    // ok, we can ask to this connected streamer to send us a stream
    ConnectionInfo streamer = Connection::manager().info(streamer_index);
    IpEndpointName a( streamer.address.c_str(), streamer.port_stream_request );
    streamer_address_ = a.address;
    streamer_address_ = a.port;

    // build OSC message
    char buffer[IP_MTU_SIZE];
    osc::OutboundPacketStream p( buffer, IP_MTU_SIZE );
    p.Clear();
    p << osc::BeginMessage( OSC_PREFIX OSC_STREAM_REQUEST );
    // send my listening port to indicate to Connection::manager where to reply
    p << listener_port_;
    p << osc::EndMessage;

    // send OSC message to streamer
    UdpTransmitSocket socket( streamer_address_ );
    socket.Send( p.Data(), p.Size() );

    // Now we wait for the offer from the streamer
    std::thread(wait_for_stream_, receiver_).detach();

#ifdef NETWORK_DEBUG
    Log::Info("Asking %s:%d for a stream", streamer.address.c_str(), streamer.port_stream_request);
    Log::Info("Waiting for response at %s:%d", Connection::manager().info().address.c_str(), listener_port_);
#endif
}

void NetworkStream::close()
{
    if (receiver_)
        delete receiver_;

    // build OSC message to inform disconnection
    char buffer[IP_MTU_SIZE];
    osc::OutboundPacketStream p( buffer, IP_MTU_SIZE );
    p.Clear();
    p << osc::BeginMessage( OSC_PREFIX OSC_STREAM_DISCONNECT );
    p << config_.port; // send my stream port to identify myself to the streamer Connection::manager
    p << osc::EndMessage;

    // send OSC message to streamer
    UdpTransmitSocket socket( streamer_address_ );
    socket.Send( p.Data(), p.Size() );
}


void NetworkStream::setConfig(NetworkToolkit::StreamConfig  conf)
{
    config_ = conf;
    confirmed_ = true;
}

void NetworkStream::update()
{
    Stream::update();

    if ( !ready_ && !failed_ && confirmed_)
    {
        // stop receiving streamer info
        receiver_->AsynchronousBreak();

#ifdef NETWORK_DEBUG
        Log::Info("Creating Network Stream %d (%d x %d)", config_.port, config_.width, config_.height);
#endif
        // build the pipeline depending on stream info
        std::ostringstream pipeline;

        // get generic pipeline string
        std::string pipelinestring = NetworkToolkit::protocol_receive_pipeline[config_.protocol];
        // find placeholder for PORT
        int xxxx = pipelinestring.find("XXXX");
        // keep beginning of pipeline
        pipeline << pipelinestring.substr(0, xxxx);
        // Replace 'XXXX'
        if (config_.protocol == NetworkToolkit::SHM_RAW) {
            std::string path = SystemToolkit::full_filename(SystemToolkit::settings_path(), "shm");
            path += std::to_string(config_.port);
            pipeline << path;
        }
        else
            pipeline << config_.port;
        // keep ending of pipeline
        pipeline << pipelinestring.substr(xxxx + 4);
        // add a videoconverter
        pipeline << " ! videoconvert";

        // open the pipeline with generic stream class
        Stream::open(pipeline.str(), config_.width, config_.height);
    }

}


NetworkSource::NetworkSource() : StreamSource()
{
    // create stream
    stream_ = (Stream *) new NetworkStream;

    // set icons
    overlays_[View::MIXING]->attach( new Symbol(Symbol::EMPTY, glm::vec3(0.8f, 0.8f, 0.01f)) );
    overlays_[View::LAYER]->attach( new Symbol(Symbol::EMPTY, glm::vec3(0.8f, 0.8f, 0.01f)) );
}


NetworkSource::~NetworkSource()
{
    networkStream()->close();
}

NetworkStream *NetworkSource::networkStream() const
{
    return dynamic_cast<NetworkStream *>(stream_);
}

void NetworkSource::setConnection(const std::string &nameconnection)
{
    connection_name_ = nameconnection;
    Log::Notify("Creating Network Source '%s'", connection_name_.c_str());

    // open network stream
    networkStream()->open( connection_name_ );
    stream_->play(true);
}


std::string NetworkSource::connection() const
{
    return connection_name_;
}


void NetworkSource::accept(Visitor& v)
{
    Source::accept(v);
    if (!failed())
        v.visit(*this);
}



