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
#ifdef NETWORK_DEBUG
            Log::Info("Received stream info from %s", sender);
#endif
            NetworkToolkit::StreamConfig conf;

            // someone is offering a stream
            osc::ReceivedMessage::const_iterator arg = m.ArgumentsBegin();
            conf.port     = (arg++)->AsInt32();
            conf.protocol = (NetworkToolkit::Protocol) (arg++)->AsInt32();
            conf.width    = (arg++)->AsInt32();
            conf.height   = (arg++)->AsInt32();

            // we got the offer from Streaming::manager()
            parent_->config_ = conf;
            parent_->connected_ = true;
            parent_->received_config_ = true;

        }
        else if( std::strcmp( m.AddressPattern(), OSC_PREFIX OSC_STREAM_REJECT ) == 0 ){
#ifdef NETWORK_DEBUG
            Log::Info("Received rejection from %s", sender);
#endif
            parent_->connected_ = false;
            parent_->received_config_ = true;
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
    received_config_ = false;
    connected_ = false;
}

glm::ivec2 NetworkStream::resolution() const
{
    return glm::ivec2(config_.width, config_.height);
}


std::string NetworkStream::clientAddress() const
{
    return config_.client_address + ":" + std::to_string(config_.port);
}

std::string NetworkStream::serverAddress() const
{
    return streamer_.address;
}

void wait_for_stream_(UdpListeningReceiveSocket *receiver)
{
    receiver->Run();
}

void NetworkStream::connect(const std::string &nameconnection)
{
    // start fresh
    if (connected())
        disconnect();
    received_config_ = false;

    // refuse self referencing
    if (nameconnection.compare(Connection::manager().info().name) == 0) {
        Log::Warning("Cannot create self-referencing Network Source '%s'", nameconnection.c_str());
        failed_ = true;
        return;
    }

    // does this Connection exists?
    int streamer_index = Connection::manager().index(nameconnection);

    // Nope, cannot connect to unknown connection
    if (streamer_index < 0) {
        Log::Warning("Cannot connect to %s: please make sure %s is active on this machine.", nameconnection.c_str(), APP_NAME);
        failed_ = true;
        return;
    }

    // ok, we want to ask to this connected streamer to send us a stream
    streamer_ = Connection::manager().info(streamer_index);
    std::string listener_address = NetworkToolkit::closest_host_ip(streamer_.address);

    // prepare listener to receive stream config from remote streaming manager
    listener_.setParent(this);

    // find an available port to receive response from remote streaming manager
    int listener_port_ = -1;
    for (int trial = 0; receiver_ == nullptr && trial < 10 ; trial++) {
        try {
            // invent a port which would be available
            listener_port_ = 72000 + rand()%1000;
            // try to create receiver (through exception on fail)
            receiver_ = new UdpListeningReceiveSocket(IpEndpointName(listener_address.c_str(), listener_port_), &listener_);
        }
        catch (const std::runtime_error&) {
            receiver_ = nullptr;
        }
    }
    if (receiver_ == nullptr) {
        Log::Notify("Cannot establish connection with %s. Please check your network.", streamer_.name.c_str());
        failed_ = true;
        return;
    }

    // build OSC message
    char buffer[IP_MTU_SIZE];
    osc::OutboundPacketStream p( buffer, IP_MTU_SIZE );
    p.Clear();
    p << osc::BeginMessage( OSC_PREFIX OSC_STREAM_REQUEST );
    // send my listening port to indicate to Connection::manager where to reply
    p << listener_port_;
    p << Connection::manager().info().name.c_str();
    p << osc::EndMessage;

    // send OSC message to streamer
    UdpTransmitSocket socket( IpEndpointName(streamer_.address.c_str(), streamer_.port_stream_request) );
    socket.Send( p.Data(), p.Size() );

    // Now we wait for the offer from the streamer
    std::thread(wait_for_stream_, receiver_).detach();

#ifdef NETWORK_DEBUG
    Log::Info("Asking %s:%d for a stream", streamer_.address.c_str(), streamer_.port_stream_request);
    Log::Info("Waiting for response at %s:%d", Connection::manager().info().address.c_str(), listener_port_);
#endif
}

void NetworkStream::disconnect()
{
    // receiver should not be active anyway, make sure it is deleted
    if (receiver_) {
        delete receiver_;
        receiver_ = nullptr;
    }

    if (connected_) {
        // build OSC message to inform disconnection
        char buffer[IP_MTU_SIZE];
        osc::OutboundPacketStream p( buffer, IP_MTU_SIZE );
        p.Clear();
        p << osc::BeginMessage( OSC_PREFIX OSC_STREAM_DISCONNECT );
        p << config_.port; // send my stream port to identify myself to the streamer Connection::manager
        p << osc::EndMessage;

        // send OSC message to streamer
        UdpTransmitSocket socket( IpEndpointName(streamer_.address.c_str(), streamer_.port_stream_request) );
        socket.Send( p.Data(), p.Size() );

        connected_ = false;
    }

    close();
}


bool NetworkStream::connected() const
{
    return connected_ && Stream::isPlaying();
}

void NetworkStream::update()
{
    Stream::update();

    if ( !ready_ && !failed_ && received_config_)
    {
        // only once
        received_config_ = false;

        // stop receiving streamer info
        if (receiver_)
            receiver_->AsynchronousBreak();

        if (connected_) {

#ifdef NETWORK_DEBUG
            Log::Info("Creating Network Stream %d (%d x %d)", config_.port, config_.width, config_.height);
#endif
            // prepare pipeline parameter with port given in config_
            std::string parameter = std::to_string(config_.port);

            // make sure the shared memory socket exists
            if (config_.protocol == NetworkToolkit::SHM_RAW) {
                // for shared memory, the parameter is a file location in settings
                parameter = SystemToolkit::full_filename(SystemToolkit::temp_path(), "shm") + parameter;
                // try few times to see if file exists and wait 20ms each time
                for(int trial = 0; trial < 5; trial ++){
                    if ( SystemToolkit::file_exists(parameter))
                        break;
                    std::this_thread::sleep_for (std::chrono::milliseconds(20));
                }
                // failed to find the shm socket file: cannot connect
                if (!SystemToolkit::file_exists(parameter)) {
                    Log::Warning("Cannot connect to shared memory %s.", parameter.c_str());
                    failed_ = true;
                }
                parameter = "\"" + parameter + "\"";
            }

            // general case : create pipeline and open
            if (!failed_) {
                // build the pipeline depending on stream info
                std::ostringstream pipeline;
                // get generic pipeline string
                std::string pipelinestring = NetworkToolkit::protocol_receive_pipeline[config_.protocol];
                // find placeholder for PORT
                int xxxx = pipelinestring.find("XXXX");
                // keep beginning of pipeline
                pipeline << pipelinestring.substr(0, xxxx);
                // Replace 'XXXX' by info on port config
                pipeline << parameter;
                // keep ending of pipeline
                pipeline << pipelinestring.substr(xxxx + 4);
                // add a videoconverter
                pipeline << " ! videoconvert";

                // open the pipeline with generic stream class
                Stream::open(pipeline.str(), config_.width, config_.height);
            }
        }
        else {
            Log::Warning("Connection was rejected by %s.\nMake sure it accepts connection and try again.", streamer_.name.c_str());
            failed_=true;
        }
    }
}


NetworkSource::NetworkSource() : StreamSource()
{
    // create stream
    stream_ = (Stream *) new NetworkStream;

    // set icons
    overlays_[View::MIXING]->attach( new Symbol(Symbol::SHARE, glm::vec3(0.8f, 0.8f, 0.01f)) );
    overlays_[View::LAYER]->attach( new Symbol(Symbol::SHARE, glm::vec3(0.8f, 0.8f, 0.01f)) );
}


NetworkSource::~NetworkSource()
{
    networkStream()->disconnect();
}

NetworkStream *NetworkSource::networkStream() const
{
    return dynamic_cast<NetworkStream *>(stream_);
}

void NetworkSource::setConnection(const std::string &nameconnection)
{
    connection_name_ = nameconnection;
    Log::Notify("Network Source connecting to '%s'", connection_name_.c_str());

    // open network stream
    networkStream()->connect( connection_name_ );
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



