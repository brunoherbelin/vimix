
#include <thread>
#include <chrono>
#include <vector>
#include <algorithm>

#include "osc/OscOutboundPacketStream.h"

#include "defines.h"
#include "Connection.h"
#include "Log.h"

Connection::Connection()
{
    receiver_ = nullptr;
}


bool Connection::init()
{
    // add default info for myself
    connections_.push_back(ConnectionInfo());

    // try to open a socket at base handshake port
    int trial = 0;
    while (trial < MAX_HANDSHAKE) {
        try {
            // increment the port to have unique ports
            connections_[0].port_handshake = HANDSHAKE_PORT + trial;
            connections_[0].port_stream_send = STREAM_REQUEST_PORT + trial;
            connections_[0].port_stream_receive = STREAM_RESPONSE_PORT + trial;

            // try to create listenning socket
            // through exception runtime if fails
            receiver_ = new UdpListeningReceiveSocket( IpEndpointName( IpEndpointName::ANY_ADDRESS,
                                                                       connections_[0].port_handshake ), &listener_ );

            // all good
            trial = MAX_HANDSHAKE;
        }
        catch(const std::runtime_error& e) {
            // arg, the receiver could not be initialized
            // because the port was not available
            receiver_ = nullptr;
        }
        // try again
        trial++;
    }

    // perfect, we could initialize the receiver
    if (receiver_!=nullptr) {

        // listen for answers
        std::thread(listen).detach();

        // regularly check for available streaming hosts
        std::thread(ask).detach();

    }

    return receiver_ != nullptr;
}

void Connection::terminate()
{
    if (receiver_!=nullptr)
        receiver_->AsynchronousBreak();
}



int Connection::numHosts () const
{
    return connections_.size();
}

ConnectionInfo Connection::info(int index)
{
    if (connections_.empty()) {
        connections_.push_back(ConnectionInfo());
    }

    index = CLAMP(index, 0, connections_.size());

    return connections_[index];
}


void Connection::print()
{
    for(int i = 0; i<connections_.size(); i++) {
        Log::Info(" - %s:%d", connections_[i].address_.c_str(), connections_[i].port_handshake);
    }
}

int Connection::index(ConnectionInfo i) const
{
    int id = -1;

    std::vector<ConnectionInfo>::const_iterator p = std::find(connections_.begin(), connections_.end(), i);
    if (p != connections_.end())
        id = std::distance(connections_.begin(), p);

    return id;
}

void Connection::listen()
{
//    Log::Info("Accepting handshake on %d", Connection::manager().connections_[0].port_handshake);
    Connection::manager().receiver_->Run();
}

void Connection::ask()
{
//    Log::Info("Broadcasting handshakes with info %d", Connection::manager().connections_[0].port_handshake);

    // prepare OSC PING message
    char buffer[IP_MTU_SIZE];
    osc::OutboundPacketStream p( buffer, IP_MTU_SIZE );
    p.Clear();
    p << osc::BeginMessage( OSC_PREFIX OSC_PING );
    p << Connection::manager().connections_[0].port_handshake;
    p << osc::EndMessage;

    // broadcast on several ports, except myself
    std::vector<int> handshake_ports;
    for(int i=HANDSHAKE_PORT; i<HANDSHAKE_PORT+MAX_HANDSHAKE; i++) {
        if (i != Connection::manager().connections_[0].port_handshake)
            handshake_ports.push_back(i);
    }

    // loop infinitely
    while(true)
    {

        // set status to pending : after pong status will be confirmed
        for(auto it=Connection::manager().connections_.begin(); it!=Connection::manager().connections_.end(); it++) {
            (*it).status--;
        }

        // broadcast the PING message on every possible ports
        for(auto it=handshake_ports.begin(); it!=handshake_ports.end(); it++) {
            IpEndpointName host( "255.255.255.255", (*it) );
            UdpTransmitSocket socket( host );
            socket.SetEnableBroadcast(true);
            socket.Send( p.Data(), p.Size() );
        }

        // wait a bit
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        // check if status decreased
        for(auto it=Connection::manager().connections_.begin(); it!=Connection::manager().connections_.end(); ) {
            if ( it!=Connection::manager().connections_.begin() && (*it).status < 1 )
                it = Connection::manager().connections_.erase(it);
            else
                it++;
        }
//        Connection::manager().print();
    }

}

void ConnectionRequestListener::ProcessMessage( const osc::ReceivedMessage& m,
                                                const IpEndpointName& remoteEndpoint )
{
    char sender[IpEndpointName::ADDRESS_AND_PORT_STRING_LENGTH];
    remoteEndpoint.AddressAndPortAsString(sender);

    // get ip of connection (without port)
    std::string remote_ip(sender);
    remote_ip = remote_ip.substr(0, remote_ip.find_last_of(":"));

    try{
        // ping request : reply with pong
        if( std::strcmp( m.AddressPattern(), OSC_PREFIX OSC_PING) == 0 ){

            // PING message has parameter : port where to reply
            osc::ReceivedMessage::const_iterator arg = m.ArgumentsBegin();
            int remote_port = (arg++)->AsInt32();

//            Log::Info("Receive PING from %s:%d", remote_ip.c_str(), remote_port);

            // build message
            char buffer[IP_MTU_SIZE];
            osc::OutboundPacketStream p( buffer, IP_MTU_SIZE );
            p.Clear();
            p << osc::BeginMessage( OSC_PREFIX OSC_PONG );
            p << Connection::manager().connections_[0].port_handshake;
            p << Connection::manager().connections_[0].port_stream_send;
            p << Connection::manager().connections_[0].port_stream_receive;
            p << osc::EndMessage;

            // send OSC message to port indicated by remote
            IpEndpointName host( remote_ip.c_str(), remote_port );
            UdpTransmitSocket socket( host );
            socket.Send( p.Data(), p.Size() );

//            Log::Info("reply PONG to %s:%d", remote_ip.c_str(), remote_port);
        }
        // pong response: add info
        else if( std::strcmp( m.AddressPattern(), OSC_PREFIX OSC_PONG) == 0 ){

            // create info struct
            ConnectionInfo info;
            info.address_ = remote_ip;

            // add all ports info
            osc::ReceivedMessage::const_iterator arg = m.ArgumentsBegin();
            info.port_handshake = (arg++)->AsInt32();
            info.port_stream_send = (arg++)->AsInt32();
            info.port_stream_receive = (arg++)->AsInt32();

            int i = Connection::manager().index(info);
            if ( i < 0) {
                // add to list
                info.status = 3;
                Connection::manager().connections_.push_back(info);
//                Log::Info("Received PONG from %s:%d : added", info.address_.c_str(), info.port_handshake);
//                Connection::manager().print();
            }
            else {
                // set high (== ALIVE)
                Connection::manager().connections_[i].status = 3;
            }

        }
    }
    catch( osc::Exception& e ){
        // any parsing errors such as unexpected argument types, or
        // missing arguments get thrown as exceptions.
        Log::Info("error while parsing message '%s' from %s : %s", m.AddressPattern(), sender, e.what());
    }
}

