#ifndef NETWORKSOURCE_H
#define NETWORKSOURCE_H

#include "osc/OscReceivedElements.h"
#include "osc/OscPacketListener.h"
#include "osc/OscOutboundPacketStream.h"
#include "ip/UdpSocket.h"

#include "NetworkToolkit.h"
#include "Connection.h"
#include "StreamSource.h"

class NetworkStream;

class StreamerResponseListener : public osc::OscPacketListener
{
protected:
    class NetworkStream *parent_;
    virtual void ProcessMessage( const osc::ReceivedMessage& m,
                                 const IpEndpointName& remoteEndpoint );
public:
    inline void setParent(NetworkStream *s) { parent_ = s; }
};


class NetworkStream : public Stream
{
    friend class StreamerResponseListener;

public:

    NetworkStream();

    void connect(const std::string &nameconnection);
    bool connected() const;
    void disconnect();

    void update() override;

    glm::ivec2 resolution() const;
    inline NetworkToolkit::Protocol protocol() const { return config_.protocol; }
    inline std::string IP() const { return config_.client_address; }
    inline uint port() const { return config_.port; }

private:
    // connection information
    ConnectionInfo streamer_;
    StreamerResponseListener listener_;
    UdpListeningReceiveSocket *receiver_;
    std::atomic<bool> received_config_;
    std::atomic<bool> connected_;

    NetworkToolkit::StreamConfig config_;
};


class NetworkSource : public StreamSource
{
    std::string connection_name_;

public:
    NetworkSource();
    ~NetworkSource();

    // Source interface
    void accept (Visitor& v) override;

    // StreamSource interface
    Stream *stream() const override { return stream_; }
    NetworkStream *networkStream() const;

    // specific interface
    void setConnection(const std::string &nameconnection);
    std::string connection() const;

    glm::ivec2 icon() const override { return glm::ivec2(18, 11); }

};



#endif // NETWORKSOURCE_H
