#include <algorithm>
#include <sstream>
#include <thread>
#include <chrono>

#include <glm/gtc/matrix_transform.hpp>

#include <gst/pbutils/pbutils.h>
#include <gst/gst.h>

#include "SystemToolkit.h"
#include "defines.h"
#include "Stream.h"
#include "Decorations.h"
#include "Visitor.h"
#include "Log.h"

#include "NetworkSource.h"

#ifndef NDEBUG
#define NETWORK_DEBUG
#endif

NetworkSource::NetworkSource() : StreamSource()
{
    // create stream
    stream_ = (Stream *) new NetworkStream;

    // set icons
    overlays_[View::MIXING]->attach( new Symbol(Symbol::EMPTY, glm::vec3(0.8f, 0.8f, 0.01f)) );
    overlays_[View::LAYER]->attach( new Symbol(Symbol::EMPTY, glm::vec3(0.8f, 0.8f, 0.01f)) );
}

void NetworkSource::setAddress(const std::string &address)
{
    address_ = address;
    Log::Notify("Creating Network Source '%s'", address_.c_str());

//    // extract host and port from "host:port"
//    std::string host = address.substr(0, address.find_last_of(":"));
//    std::string port_s = address.substr(address.find_last_of(":") + 1);
//    uint port = std::stoi(port_s);

//    // validate protocol
//    if (protocol < NetworkToolkit::TCP_JPEG || protocol >= NetworkToolkit::DEFAULT)
//        protocol = NetworkToolkit::TCP_JPEG;

//    // open network stream
//    networkstream()->open( protocol, host, port );
//    stream_->play(true);
}


std::string NetworkSource::address() const
{
    return address_;
}


void NetworkSource::accept(Visitor& v)
{
    Source::accept(v);
    if (!failed())
        v.visit(*this);
}


void NetworkHosts::listen()
{
    Log::Info("Accepting Streaming responses on port %d", STREAM_RESPONSE_PORT);
    NetworkHosts::manager().receiver_->Run();
    Log::Info("Refusing Streaming responses");
}

void NetworkHosts::ask()
{
    Log::Info("Broadcasting Streaming requests on port %d", STREAM_REQUEST_PORT);

    IpEndpointName host( "255.255.255.255", STREAM_REQUEST_PORT );
    UdpTransmitSocket socket( host );
    socket.SetEnableBroadcast(true);

    char buffer[IP_MTU_SIZE];
    osc::OutboundPacketStream p( buffer, IP_MTU_SIZE );
    p.Clear();
    p << osc::BeginMessage( OSC_PREFIX OSC_STREAM_REQUEST ) << osc::EndMessage;

    while(true)
    {
        socket.Send( p.Data(), p.Size() );

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

}

// this is called when receiving an answer for streaming request
void HostsResponsesListener::ProcessMessage( const osc::ReceivedMessage& m,
                                             const IpEndpointName& remoteEndpoint )
{
    char sender[IpEndpointName::ADDRESS_AND_PORT_STRING_LENGTH];
    remoteEndpoint.AddressAndPortAsString(sender);

    std::string se(sender);
    std::string senderip = se.substr(0, se.find_last_of(":"));

    try{
        if( std::strcmp( m.AddressPattern(), OSC_PREFIX OSC_STREAM_OFFER ) == 0 ){

            // someone is offering a stream
            osc::ReceivedMessage::const_iterator arg = m.ArgumentsBegin();
            const char *address = (arg++)->AsString();
            int p = (arg++)->AsInt32();
            int w = (arg++)->AsInt32();
            int h = (arg++)->AsInt32();

            Log::Info("Able to receive %s %d x %d stream from %s ", address, w, h, senderip.c_str());
            NetworkHosts::manager().addHost(address, p, w, h);

        }
        else if( std::strcmp( m.AddressPattern(), OSC_PREFIX OSC_STREAM_RETRACT ) == 0 ){
            // someone is retracting a stream
            osc::ReceivedMessage::const_iterator arg = m.ArgumentsBegin();
            const char *address = (arg++)->AsString();
            NetworkHosts::manager().removeHost(address);
        }
    }
    catch( osc::Exception& e ){
        // any parsing errors such as unexpected argument types, or
        // missing arguments get thrown as exceptions.
        Log::Info("error while parsing message '%s' from %s : %s", m.AddressPattern(), sender, e.what());
    }
}



NetworkHosts::NetworkHosts()
{
//    // listen for answers
//    receiver_ = new UdpListeningReceiveSocket(IpEndpointName( IpEndpointName::ANY_ADDRESS, STREAM_RESPONSE_PORT ), &listener_ );
//    std::thread(listen).detach();

//    // regularly check for available streaming hosts
//    std::thread(ask).detach();
}


void NetworkHosts::addHost(const std::string &address, int protocol, int width, int height)
{
    // add only new
    if (disconnected(address)) {
        src_address_.push_back(address);

        // TODO : fill description
        src_description_.push_back(address);

        list_uptodate_ = false;
    }

}
void NetworkHosts::removeHost(const std::string &address)
{
    // remove existing only
    if (connected(address)) {
        std::vector< std::string >::iterator addrit   = src_address_.begin();
        std::vector< std::string >::iterator descit   = src_description_.begin();
        while (addrit != src_address_.end()){

            if ( (*addrit).compare(address) == 0 )
            {
                src_address_.erase(addrit);
                src_description_.erase(descit);
                break;
            }

            addrit++;
            descit++;
        }
        list_uptodate_ = false;
    }
}

int NetworkHosts::numHosts() const
{
    return src_address_.size();
}

std::string NetworkHosts::name(int index) const
{
    if (index > -1 && index < (int) src_address_.size())
        return src_address_[index];
    else
        return "";
}

std::string NetworkHosts::description(int index) const
{
    if (index > -1 && index < (int) src_description_.size())
        return src_description_[index];
    else
        return "";
}

int  NetworkHosts::index(const std::string &address) const
{
    int i = -1;
    std::vector< std::string >::const_iterator p = std::find(src_address_.begin(), src_address_.end(), address);
    if (p != src_address_.end())
        i = std::distance(src_address_.begin(), p);

    return i;
}

bool NetworkHosts::connected(const std::string &address) const
{
    std::vector< std::string >::const_iterator d = std::find(src_address_.begin(), src_address_.end(), address);
    return d != src_address_.end();
}

bool NetworkHosts::disconnected(const std::string &address) const
{
    if (list_uptodate_)
        return false;
    return !connected(address);
}

struct hasAddress: public std::unary_function<NetworkSource*, bool>
{
    inline bool operator()(const NetworkSource* elem) const {
       return (elem && elem->address() == _a);
    }
    hasAddress(std::string a) : _a(a) { }
private:
    std::string _a;
};

Source *NetworkHosts::createSource(const std::string &address) const
{
    Source *s = nullptr;

    // find if a DeviceSource with this device is already registered
    std::list< NetworkSource *>::const_iterator d = std::find_if(network_sources_.begin(), network_sources_.end(), hasAddress(address));

    // if already registered, clone the device source
    if ( d != network_sources_.end())  {
        CloneSource *cs = (*d)->clone();
        s = cs;
    }
    // otherwise, we are free to create a new Network source
    else {
        NetworkSource *ns = new NetworkSource();
        ns->setAddress(address);
        s = ns;
    }

    return s;
}



















NetworkStream::NetworkStream(): Stream(), protocol_(NetworkToolkit::DEFAULT), host_("127.0.0.1"), port_(5000)
{

}

glm::ivec2 NetworkStream::resolution()
{
    return glm::ivec2( width_, height_);
}


void NetworkStream::open(NetworkToolkit::Protocol protocol, const std::string &host, uint port )
{
    protocol_ = protocol;
    host_ = host;
    port_ = port;

    int w = 1920;
    int h = 1080;

    std::ostringstream pipeline;
    if (protocol_ == NetworkToolkit::TCP_JPEG || protocol_ == NetworkToolkit::TCP_H264) {

        pipeline << "tcpclientsrc name=src timeout=1 host=" << host_ << " port=" << port_;
    }
    else if (protocol_ == NetworkToolkit::SHM_RAW) {

        std::string path = SystemToolkit::full_filename(SystemToolkit::settings_path(), "shm_socket");
        pipeline << "shmsrc name=src is-live=true socket-path=" << path;
        // TODO SUPPORT multiple sockets shared memory
    }

    pipeline << NetworkToolkit::protocol_receive_pipeline[protocol_];
    pipeline << " ! videoconvert";

//    if ( ping(&w, &h) )
        // (private) open stream
        Stream::open(pipeline.str(), w, h);
//    else {
//        Log::Notify("Failed to connect to %s:%d", host.c_str(), port);
//        failed_ = true;
//    }




}
