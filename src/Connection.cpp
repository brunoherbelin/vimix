/*
 * This file is part of vimix - video live mixer
 *
 * **Copyright** (C) 2019-2023 Bruno Herbelin <bruno.herbelin@gmail.com>
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

#if ((ULONG_MAX) != (UINT_MAX))
#define _M_X64
#endif
#include "osc/OscOutboundPacketStream.h"

#ifdef VIMIX_USE_AVAHI
#include <avahi-client/client.h>
#include <avahi-client/lookup.h>
#include <avahi-client/publish.h>
#include <avahi-common/simple-watch.h>
#include <avahi-common/error.h>
#include <avahi-common/address.h>
#endif

#ifdef VIMIX_USE_BONJOUR
#include <dns_sd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/select.h>
#endif

#include "defines.h"
#include "Settings.h"
#include "Streamer.h"
#include "Log.h"
#include "Toolkit/NetworkToolkit.h"

#include "Connection.h"

#ifndef NDEBUG
#define CONNECTION_DEBUG
#endif


Connection::Connection() : mdns_running_(false), receiver_(nullptr)
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

        // listen for pong replies
        std::thread(listen).detach();

        // mDNS peer discovery: register our service and browse for peers
#if defined(VIMIX_USE_AVAHI) || defined(VIMIX_USE_BONJOUR)
        mdns_running_ = true;
        std::thread(mdns).detach();
#endif

        // inform the application settings of our id
        Settings::application.instance_id = connections_[0].port_handshake - HANDSHAKE_PORT;
        // restore state of Streamer
        Streaming::manager().enable( Settings::application.accept_connections );

    }

    return receiver_ != nullptr;
}

void Connection::terminate()
{
    // end mDNS thread
#if defined(VIMIX_USE_AVAHI) || defined(VIMIX_USE_BONJOUR)
    {
        std::mutex mtx;
        std::unique_lock<std::mutex> lck(mtx);
        mdns_running_ = false;
        if ( mdns_end_.wait_for(lck, std::chrono::seconds(2)) == std::cv_status::timeout)
            g_printerr("Failed to terminate Connection manager (mDNS).");
    }
#endif

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


struct hasName
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

void Connection::print()
{
    for(size_t i = 0; i<connections_.size(); i++) {
        Log::Info(" - %s %s:%d", connections_[i].name.c_str(), connections_[i].address.c_str(), connections_[i].port_handshake);
    }
}

void Connection::addPeer(const ConnectionInfo &info)
{
    int i = index(info.name);
    if (i < 0) {
        connections_.push_back(info);
#ifdef CONNECTION_DEBUG
        Log::Info("Connection: peer joined: %s %s:%d",
            info.name.c_str(), info.address.c_str(), info.port_handshake);
        print();
#endif
    }
    else {
        connections_[i].alive = ALIVE;
    }
}

void Connection::removePeer(const std::string &name)
{
    int i = index(name);
    if (i > 0) {
        Streaming::manager().removeStreams(connections_[i].name);
        connections_.erase(connections_.begin() + i);
#ifdef CONNECTION_DEBUG
        Log::Info("Connection: peer left: %s", name.c_str());
        print();
#endif
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


// ---- mDNS peer discovery ----
//
// When a peer's service appears on the network, we send them a unicast PONG
// with our connection info (name, ports). Their existing OSC listener receives it
// and adds us to their connections_ list. Simultaneously, they see OUR service
// appear and send us a PONG — which our OSC listener handles symmetrically.
//
// On peer departure (graceful or after mDNS cache expiry), BROWSER_REMOVE fires
// and we remove them from connections_ directly — no alive countdown needed.


#ifdef VIMIX_USE_AVAHI

static void avahi_group_cb(AvahiEntryGroup *, AvahiEntryGroupState state, void *)
{
    if (state == AVAHI_ENTRY_GROUP_COLLISION)
        Log::Info("Connection: mDNS service name collision");
    else if (state == AVAHI_ENTRY_GROUP_FAILURE)
        Log::Info("Connection: mDNS entry group failure");
}

// Called when a peer service has been resolved to an IP address and port.
// Sends a unicast PONG to that peer so they add us to their connections_.
static void avahi_resolve_cb(AvahiServiceResolver *r,
    AvahiIfIndex, AvahiProtocol,
    AvahiResolverEvent event,
    const char *, const char *, const char *,   // name, type, domain
    const char *,                                // hosttarget
    const AvahiAddress *address,
    uint16_t port,                               // host byte order
    AvahiStringList *, AvahiLookupResultFlags,
    void *)
{
    if (event == AVAHI_RESOLVER_FOUND) {
        char ipstr[AVAHI_ADDRESS_STR_MAX];
        avahi_address_snprint(ipstr, sizeof(ipstr), address);

        ConnectionInfo self = Connection::manager().info(0);

        // ignore self
        if (!NetworkToolkit::is_host_ip(ipstr) || port != (uint16_t)self.port_handshake)
        {
            // send our PONG so the peer adds us to their connections_
            char buffer[IP_MTU_SIZE];
            osc::OutboundPacketStream p(buffer, IP_MTU_SIZE);
            p.Clear();
            p << osc::BeginMessage(OSC_PREFIX OSC_PONG);
            p << self.name.c_str();
            p << (osc::int32) self.port_handshake;
            p << (osc::int32) self.port_stream_request;
            p << (osc::int32) self.port_osc;
            p << osc::EndMessage;

            try {
                // port is already host byte order — UdpTransmitSocket will htons() it
                UdpTransmitSocket( IpEndpointName(ipstr, (int)port) ).Send(p.Data(), p.Size());
            } catch (...) {}

#ifdef CONNECTION_DEBUG
            Log::Info("Connection: mDNS sent PONG to %s:%d", ipstr, (int)port);
#endif
        }
    }
    avahi_service_resolver_free(r);
}

// Called when a _vimix._udp service appears or disappears on the network.
static void avahi_browse_cb(AvahiServiceBrowser *,
    AvahiIfIndex iface, AvahiProtocol proto,
    AvahiBrowserEvent event,
    const char *name, const char *type, const char *domain,
    AvahiLookupResultFlags, void *userdata)
{
    AvahiClient *client = (AvahiClient *)userdata;

    if (event == AVAHI_BROWSER_NEW) {
        if (!avahi_service_resolver_new(client, iface, proto, name, type, domain,
                AVAHI_PROTO_INET, (AvahiLookupFlags)0, avahi_resolve_cb, NULL))
            Log::Info("Connection: avahi_service_resolver_new failed: %s",
                avahi_strerror(avahi_client_errno(client)));
    }
    else if (event == AVAHI_BROWSER_REMOVE) {
        Connection::manager().removePeer(name);
    }
}

// Called when the Avahi client state changes.
// When the daemon is ready, register our service and start browsing.
static void avahi_client_cb(AvahiClient *client, AvahiClientState state, void *)
{
    if (state == AVAHI_CLIENT_S_RUNNING) {
        ConnectionInfo self = Connection::manager().info(0);
        AvahiEntryGroup *group = avahi_entry_group_new(client, avahi_group_cb, NULL);
        if (group) {
            // port in host byte order — Avahi handles the htons() internally
            int ret = avahi_entry_group_add_service(group,
                AVAHI_IF_UNSPEC, AVAHI_PROTO_INET, (AvahiPublishFlags)0,
                self.name.c_str(), "_vimix._udp",
                NULL, NULL,
                (uint16_t)self.port_handshake,
                NULL);
            if (ret < 0)
                Log::Info("Connection: avahi_entry_group_add_service failed: %s",
                    avahi_strerror(ret));
            else
                avahi_entry_group_commit(group);
        }
        avahi_service_browser_new(client, AVAHI_IF_UNSPEC, AVAHI_PROTO_INET,
            "_vimix._udp", NULL, (AvahiLookupFlags)0, avahi_browse_cb, client);
    }
    else if (state == AVAHI_CLIENT_FAILURE) {
        Log::Info("Connection: Avahi client failure: %s",
            avahi_strerror(avahi_client_errno(client)));
    }
}

void Connection::mdns()
{
    AvahiSimplePoll *poll = avahi_simple_poll_new();
    if (!poll) {
        Log::Info("Connection: avahi_simple_poll_new failed");
        Connection::manager().mdns_end_.notify_all();
        return;
    }

    int error;
    AvahiClient *client = avahi_client_new(
        avahi_simple_poll_get(poll),
        (AvahiClientFlags)0,
        avahi_client_cb,
        NULL,
        &error);
    if (!client) {
        Log::Info("Connection: Avahi client init failed: %s", avahi_strerror(error));
        avahi_simple_poll_free(poll);
        Connection::manager().mdns_end_.notify_all();
        return;
    }

    // iterate() with 200ms timeout checks mdns_running_ without long blocking
    while (Connection::manager().mdns_running_) {
        if (avahi_simple_poll_iterate(poll, 200) != 0)
            break;
    }

    avahi_client_free(client);
    avahi_simple_poll_free(poll);
    Connection::manager().mdns_end_.notify_all();
}

#endif // VIMIX_USE_AVAHI


#ifdef VIMIX_USE_BONJOUR

// Context passed from browse callback to resolve callback.
// Stores the sub-ref (initialized to main_ref, overwritten by DNSServiceResolve).
// Must be heap-allocated so it outlives the browse callback stack frame.
struct BonjourResolveCtx {
    std::string name;   // service instance name for potential debug use
    DNSServiceRef ref;  // sub-ref; written by DNSServiceResolve, freed in resolve_cb
};

// Called when a peer service has been resolved to hostname + port.
// Uses getaddrinfo to get IP, then sends unicast PONG.
static void DNSSD_API bonjour_resolve_cb(
    DNSServiceRef sdRef,
    DNSServiceFlags,
    uint32_t,
    DNSServiceErrorType error,
    const char *,           // fullname
    const char *hosttarget,
    uint16_t port,          // network byte order
    uint16_t,
    const unsigned char *,
    void *context)
{
    BonjourResolveCtx *ctx = (BonjourResolveCtx *)context;

    if (error == kDNSServiceErr_NoError) {
        struct addrinfo hints = {}, *res = nullptr;
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_DGRAM;
        if (getaddrinfo(hosttarget, NULL, &hints, &res) == 0 && res) {
            char ipstr[INET_ADDRSTRLEN];
            inet_ntop(AF_INET,
                &((struct sockaddr_in *)res->ai_addr)->sin_addr,
                ipstr, sizeof(ipstr));
            freeaddrinfo(res);

            uint16_t p = ntohs(port);   // convert to host byte order
            ConnectionInfo self = Connection::manager().info(0);

            // ignore self
            if (!NetworkToolkit::is_host_ip(ipstr) || p != (uint16_t)self.port_handshake)
            {
                char buffer[IP_MTU_SIZE];
                osc::OutboundPacketStream msg(buffer, IP_MTU_SIZE);
                msg.Clear();
                msg << osc::BeginMessage(OSC_PREFIX OSC_PONG);
                msg << self.name.c_str();
                msg << (osc::int32)self.port_handshake;
                msg << (osc::int32)self.port_stream_request;
                msg << (osc::int32)self.port_osc;
                msg << osc::EndMessage;

                try {
                    UdpTransmitSocket( IpEndpointName(ipstr, (int)p) ).Send(msg.Data(), msg.Size());
                } catch (...) {}

#ifdef CONNECTION_DEBUG
                Log::Info("Connection: mDNS sent PONG to %s:%d", ipstr, (int)p);
#endif
            }
        }
    }

    DNSServiceRefDeallocate(sdRef);
    delete ctx;
}

// Called when a _vimix._udp service appears or disappears on the network.
static void DNSSD_API bonjour_browse_cb(
    DNSServiceRef,
    DNSServiceFlags flags,
    uint32_t ifIndex,
    DNSServiceErrorType error,
    const char *name,
    const char *type,
    const char *domain,
    void *context)
{
    if (error != kDNSServiceErr_NoError) return;

    DNSServiceRef main_ref = (DNSServiceRef)context;

    if (flags & kDNSServiceFlagsAdd) {
        // resolve to get hostname + port, then send PONG in resolve_cb
        BonjourResolveCtx *ctx = new BonjourResolveCtx{name, main_ref};
        DNSServiceErrorType err = DNSServiceResolve(
            &ctx->ref, kDNSServiceFlagsShareConnection,
            ifIndex, name, type, domain,
            bonjour_resolve_cb, ctx);
        if (err != kDNSServiceErr_NoError) {
            Log::Info("Connection: DNSServiceResolve failed: %d", (int)err);
            delete ctx;
        }
    }
    else {
        Connection::manager().removePeer(name);
    }
}

void Connection::mdns()
{
    DNSServiceRef main_ref;
    if (DNSServiceCreateConnection(&main_ref) != kDNSServiceErr_NoError) {
        Log::Info("Connection: DNSServiceCreateConnection failed");
        Connection::manager().mdns_end_.notify_all();
        return;
    }

    ConnectionInfo self = Connection::manager().info(0);

    // Register this vimix instance so other peers can discover it
    DNSServiceRef reg_ref = main_ref;
    DNSServiceRegister(&reg_ref, kDNSServiceFlagsShareConnection, 0,
        self.name.c_str(), "_vimix._udp", NULL, NULL,
        htons((uint16_t)self.port_handshake),
        0, NULL, NULL, NULL);

    // Browse for other vimix instances; pass main_ref as context for sub-ref creation
    DNSServiceRef browse_ref = main_ref;
    DNSServiceBrowse(&browse_ref, kDNSServiceFlagsShareConnection, 0,
        "_vimix._udp", NULL,
        bonjour_browse_cb, (void *)main_ref);

    // Event loop: 500ms select timeout so mdns_running_ is checked regularly
    int fd = DNSServiceRefSockFD(main_ref);
    while (Connection::manager().mdns_running_) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(fd, &readfds);
        struct timeval tv = {0, 500000};
        if (select(fd + 1, &readfds, NULL, NULL, &tv) > 0)
            DNSServiceProcessResult(main_ref);
    }

    // Deallocating main_ref cancels all sub-operations (register, browse, any pending resolves)
    DNSServiceRefDeallocate(main_ref);
    Connection::manager().mdns_end_.notify_all();
}

#endif // VIMIX_USE_BONJOUR


void Connection::RequestListener::ProcessMessage( const osc::ReceivedMessage& m,
                                                const IpEndpointName& remoteEndpoint )
{
    char sender[IpEndpointName::ADDRESS_AND_PORT_STRING_LENGTH];
    remoteEndpoint.AddressAndPortAsString(sender);

    // get ip of connection (without port)
    std::string remote_ip(sender);
    remote_ip = remote_ip.substr(0, remote_ip.find_last_of(":"));

    try{
        // pong: a peer is introducing itself with its connection info
        if( std::strcmp( m.AddressPattern(), OSC_PREFIX OSC_PONG) == 0 ){

            // create info struct
            ConnectionInfo info;
            info.address = remote_ip;

            // add all ports info
            osc::ReceivedMessage::const_iterator arg = m.ArgumentsBegin();
            info.name = std::string( (arg++)->AsString() );
            info.port_handshake = (arg++)->AsInt32();
            info.port_stream_request = (arg++)->AsInt32();
            info.port_osc = (arg++)->AsInt32();

            Connection::manager().addPeer(info);
        }
#ifdef CONNECTION_DEBUG
        else {
            Log::Info("Connection: unexpected message '%s' from %s",
                m.AddressPattern(), sender);
        }
#endif
    }
    catch( osc::Exception& e ){
        Log::Info("Error while parsing message '%s' from %s : %s",
            m.AddressPattern(), sender, e.what());
    }
}
