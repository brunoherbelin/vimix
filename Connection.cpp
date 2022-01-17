/*
 * This file is part of vimix - video live mixer
 *
 * **Copyright** (C) 2019-2022 Bruno Herbelin <bruno.herbelin@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
**/

#include <thread>
#include <chrono>
#include <algorithm>

#include "osc/OscOutboundPacketStream.h"

#include "defines.h"
#include "Settings.h"
#include "Streamer.h"
#include "Log.h"

#include "Connection.h"

#ifndef NDEBUG
#define CONNECTION_DEBUG
#endif


Connection::Connection() : asking_(false), receiver_(nullptr)
{
}

Connection::~Connection()
{
}

bool Connection::init()
{
    // add default info for myself
    connections_.push_back(ConnectionInfo());

    if (receiver_ == nullptr) {
        // try to open a socket at base handshake port
        int trial = 0;
        while (trial < MAX_HANDSHAKE) {
            try {
                // increment the port to have unique ports
                connections_[0].port_handshake = HANDSHAKE_PORT + trial;
                connections_[0].port_stream_request = STREAM_REQUEST_PORT + trial;
                connections_[0].port_osc = OSC_DIALOG_PORT + trial;

                // try to create listenning socket
                // through exception runtime if fails
                receiver_ = new UdpListeningReceiveSocket( IpEndpointName( IpEndpointName::ANY_ADDRESS,
                                                                           connections_[0].port_handshake ), &listener_ );
                // validate hostname
                connections_[0].name = APP_NAME "@" + NetworkToolkit::hostname() +
                        "." + std::to_string(connections_[0].port_handshake-HANDSHAKE_PORT);
                // all good
                trial = MAX_HANDSHAKE;
            }
            catch (const std::runtime_error&) {
                // arg, the receiver could not be initialized
                // because the port was not available
                receiver_ = nullptr;
            }
            // try again
            trial++;
        }
    }

    // perfect, we could initialize the receiver
    if (receiver_!=nullptr) {
        // listen for answers
        std::thread(listen).detach();

        // regularly check for available streaming hosts
        asking_ = true;
        std::thread(ask).detach();

        // inform the application settings of our id
        Settings::application.instance_id = connections_[0].port_handshake - HANDSHAKE_PORT;
        // use or replace instance name from settings
//        if (Settings::application.instance_names.count(Settings::application.instance_id))
//            connections_[0].name = Settings::application.instance_names[Settings::application.instance_id];
//        else
//            Settings::application.instance_names[Settings::application.instance_id] = connections_[0].name;
        // restore state of Streamer
        Streaming::manager().enable( Settings::application.accept_connections );

    }

    return receiver_ != nullptr;
}

void Connection::terminate()
{
    // end ask loop
    std::mutex mtx;
    std::unique_lock<std::mutex> lck(mtx);
    asking_ = false;
    if ( ask_end_.wait_for(lck,std::chrono::seconds(2)) == std::cv_status::timeout)
        g_printerr("Failed to terminate Connection manager (asker).");

    // end receiver
    if (receiver_!=nullptr) {

        // request termination of receiver
        receiver_->AsynchronousBreak();

        // wait for the listenner end notification
        std::mutex mtx;
        std::unique_lock<std::mutex> lck(mtx);
        if ( listen_end_.wait_for(lck,std::chrono::seconds(2)) == std::cv_status::timeout)
            g_printerr("Failed to terminate Connection manager (listener).");

        // delete receiver and ready to initialize
        delete receiver_;
        receiver_ = nullptr;
    }

    // end Streamers
    Streaming::manager().enable( false );
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

    index = CLAMP(index, 0, (int) connections_.size());

    return connections_[index];
}


struct hasName: public std::unary_function<ConnectionInfo, bool>
{
    inline bool operator()(const ConnectionInfo &elem) const {
       return (elem.name.compare(_a) == 0);
    }
    explicit hasName(const std::string &a) : _a(a) { }
private:
    std::string _a;
};


int Connection::index(const std::string &name) const
{
    int id = -1;

    std::vector<ConnectionInfo>::const_iterator p = std::find_if(connections_.begin(), connections_.end(), hasName(name));
    if (p != connections_.end())
        id = std::distance(connections_.begin(), p);

    return id;
}

int Connection::index(ConnectionInfo i) const
{
    int id = -1;

    std::vector<ConnectionInfo>::const_iterator p = std::find(connections_.begin(), connections_.end(), i);
    if (p != connections_.end())
        id = std::distance(connections_.begin(), p);

    return id;
}

void Connection::print()
{
    for(size_t i = 0; i<connections_.size(); i++) {
        Log::Info(" - %s %s:%d", connections_[i].name.c_str(), connections_[i].address.c_str(), connections_[i].port_handshake);
    }
}

void Connection::listen()
{
#ifdef CONNECTION_DEBUG
    Log::Info("Accepting handshake on port %d", Connection::manager().connections_[0].port_handshake);
#endif
    if (Connection::manager().receiver_)
        Connection::manager().receiver_->Run();

    Connection::manager().listen_end_.notify_all();
}

void Connection::ask()
{
    // prepare OSC PING message
    char buffer[IP_MTU_SIZE];
    osc::OutboundPacketStream p( buffer, IP_MTU_SIZE );
    p.Clear();
    p << osc::BeginMessage( OSC_PREFIX OSC_PING );
    p << Connection::manager().connections_[0].port_handshake;
    p << osc::EndMessage;

    UdpSocket socket;
    socket.SetEnableBroadcast(true);

    // loop infinitely
    while(Connection::manager().asking_)
    {
        // broadcast on several ports
        for(int i=HANDSHAKE_PORT; i<HANDSHAKE_PORT+MAX_HANDSHAKE; i++)
            socket.SendTo( IpEndpointName( i ), p.Data(), p.Size() );

        // wait a bit
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        // check the list of connections for non responding (disconnected)
        std::vector< ConnectionInfo >::iterator it = Connection::manager().connections_.begin();
        for(++it; it!=Connection::manager().connections_.end(); ) {
            // decrease life score
            (*it).alive--;
            // erase connection if its life score is negative (not responding too many times)
            if ( (*it).alive < 0 ) {
                // inform streamer to cancel streaming to this client
//                Streaming::manager().removeStreams( (*it).name );
                // remove from list
                it = Connection::manager().connections_.erase(it);
#ifdef CONNECTION_DEBUG
                Log::Info("List of connection updated:");
                Connection::manager().print();
#endif
            }
            // loop
            else
                ++it;
        }
    }

    Connection::manager().ask_end_.notify_all();
}

void Connection::RequestListener::ProcessMessage( const osc::ReceivedMessage& m,
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

            // ignore requests from myself
            if ( !NetworkToolkit::is_host_ip(remote_ip)
                 || Connection::manager().connections_[0].port_handshake != remote_port) {

                // build message
                char buffer[IP_MTU_SIZE];
                osc::OutboundPacketStream p( buffer, IP_MTU_SIZE );
                p.Clear();
                p << osc::BeginMessage( OSC_PREFIX OSC_PONG );
                p << Connection::manager().connections_[0].name.c_str();
                p << Connection::manager().connections_[0].port_handshake;
                p << Connection::manager().connections_[0].port_stream_request;
                p << Connection::manager().connections_[0].port_osc;
                p << osc::EndMessage;

                // send OSC message to port indicated by remote
                IpEndpointName host( remote_ip.c_str(), remote_port );
                UdpTransmitSocket socket( host );
                socket.Send( p.Data(), p.Size() );

            }
        }
        // pong response: add info
        else if( std::strcmp( m.AddressPattern(), OSC_PREFIX OSC_PONG) == 0 ){

            // create info struct
            ConnectionInfo info;
            info.address = remote_ip;

            // add all ports info
            osc::ReceivedMessage::const_iterator arg = m.ArgumentsBegin();
            info.name = std::string( (arg++)->AsString() );
            info.port_handshake = (arg++)->AsInt32();
            info.port_stream_request = (arg++)->AsInt32();
            info.port_osc = (arg++)->AsInt32();

            // do we know this connection ?
            int i = Connection::manager().index(info);
            if ( i < 0) {
                // a new connection! Add to list
                Connection::manager().connections_.push_back(info);
                // replace instance name in settings
//                int id = info.port_handshake - HANDSHAKE_PORT;
//                Settings::application.instance_names[id] = info.name;

#ifdef CONNECTION_DEBUG
                Log::Info("List of connection updated:");
                Connection::manager().print();
#endif
            }
            else {
                // we know this connection: keep its status to ALIVE
                Connection::manager().connections_[i].alive = ALIVE;
            }

        }
    }
    catch( osc::Exception& e ){
        // any parsing errors such as unexpected argument types, or
        // missing arguments get thrown as exceptions.
        Log::Info("error while parsing message '%s' from %s : %s", m.AddressPattern(), sender, e.what());
    }
}

