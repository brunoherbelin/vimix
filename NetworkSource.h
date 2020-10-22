#ifndef NETWORKSOURCE_H
#define NETWORKSOURCE_H

#include "osc/OscReceivedElements.h"
#include "osc/OscPacketListener.h"
#include "osc/OscOutboundPacketStream.h"
#include "ip/UdpSocket.h"

#include "NetworkToolkit.h"
#include "StreamSource.h"

class NetworkSource : public StreamSource
{
    std::string address_;

public:
    NetworkSource();

    // Source interface
    void accept (Visitor& v) override;

    // StreamSource interface
    Stream *stream() const override { return stream_; }

    // specific interface
    void setAddress(const std::string &address);
    std::string address() const;

    glm::ivec2 icon() const override { return glm::ivec2(11, 8); }

};


class HostsResponsesListener : public osc::OscPacketListener {

protected:

    virtual void ProcessMessage( const osc::ReceivedMessage& m,
                                 const IpEndpointName& remoteEndpoint );
};


class NetworkHosts
{
    friend class NetworkSource;
    friend class HostsResponsesListener;

    NetworkHosts();
    NetworkHosts(NetworkHosts const& copy);            // Not Implemented
    NetworkHosts& operator=(NetworkHosts const& copy); // Not Implemented


public:

    static NetworkHosts& manager()
    {
        // The only instance
        static NetworkHosts _instance;
        return _instance;
    }

    int numHosts () const;
    std::string name (int index) const;
    std::string description (int index) const;

    int  index  (const std::string &address) const;
    bool connected (const std::string &address) const;
    bool disconnected (const std::string &address) const;

    Source *createSource(const std::string &address) const;

private:

    std::vector< std::string > src_address_;
    std::vector< std::string > src_description_;
    bool list_uptodate_;

    std::list< NetworkSource * > network_sources_;

    static void ask();
    static void listen();
    HostsResponsesListener listener_;
    UdpListeningReceiveSocket *receiver_;
    void addHost(const std::string &address, int protocol, int width, int height);
    void removeHost(const std::string &address);

};


class NetworkStream : public Stream
{
public:

    NetworkStream();
    void open(NetworkToolkit::Protocol protocol, const std::string &host, uint port );

    glm::ivec2 resolution();

    inline NetworkToolkit::Protocol protocol() const { return protocol_; }
    inline std::string host() const { return host_; }
    inline uint port() const { return port_; }

private:
    NetworkToolkit::Protocol protocol_;
    std::string host_;
    uint port_;

};


#endif // NETWORKSOURCE_H
