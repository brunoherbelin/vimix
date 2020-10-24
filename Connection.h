#ifndef CONNECTION_H
#define CONNECTION_H

#include "osc/OscReceivedElements.h"
#include "osc/OscPacketListener.h"
#include "ip/UdpSocket.h"

#include "NetworkToolkit.h"

#define ALIVE 3

class ConnectionRequestListener : public osc::OscPacketListener {

protected:
    virtual void ProcessMessage( const osc::ReceivedMessage& m,
                                 const IpEndpointName& remoteEndpoint );
};

struct ConnectionInfo {

    std::string address;
    int port_handshake;
    int port_stream_request;
    int port_osc;
    std::string name;
    int alive;

    ConnectionInfo () {
        address = "127.0.0.1";
        port_handshake = HANDSHAKE_PORT;
        port_stream_request = STREAM_REQUEST_PORT;
        port_osc = OSC_DIALOG_PORT;
        name = "";
        alive = ALIVE;
    }

    inline ConnectionInfo& operator = (const ConnectionInfo& o)
    {
        if (this != &o) {
            this->address = o.address;
            this->port_handshake = o.port_handshake;
            this->port_stream_request = o.port_stream_request;
            this->port_osc = o.port_osc;
            this->name = o.name;
        }
        return *this;
    }

    inline bool operator == (const ConnectionInfo& o) const
    {
        return this->address.compare(o.address) == 0
                && this->port_handshake == o.port_handshake;
    }

};

class Connection
{
    friend class ConnectionRequestListener;

    // Private Constructor
    Connection();
    Connection(Connection const& copy);            // Not Implemented
    Connection& operator=(Connection const& copy); // Not Implemented

public:
    static Connection& manager()
    {
        // The only instance
        static Connection _instance;
        return _instance;
    }

    bool init();
    void terminate();

    int numHosts () const;
    int index(ConnectionInfo i) const;
    int index(const std::string &name) const;
    ConnectionInfo info(int index = 0);  // index 0 for self

private:

    std::string hostname_;

    static void ask();
    static void listen();
    ConnectionRequestListener listener_;
    UdpListeningReceiveSocket *receiver_;

    std::vector< ConnectionInfo > connections_;

    void print();
};

#endif // CONNECTION_H
