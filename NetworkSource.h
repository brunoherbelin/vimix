#ifndef NETWORKSOURCE_H
#define NETWORKSOURCE_H

#include "osc/OscReceivedElements.h"
#include "osc/OscPacketListener.h"
#include "osc/OscOutboundPacketStream.h"
#include "ip/UdpSocket.h"

#include "NetworkToolkit.h"
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
public:

    NetworkStream();
    void open(const std::string &nameconnection);
    void close();

    void setConfig(NetworkToolkit::StreamConfig  conf);
    void update() override;

    glm::ivec2 resolution() const;
    inline NetworkToolkit::Protocol protocol() const { return config_.protocol; }
    inline std::string IP() const { return config_.client_address; }
    inline uint port() const { return config_.port; }

private:
    // connection information
    IpEndpointName streamer_address_;
//    int listener_port_;
    StreamerResponseListener listener_;
    UdpListeningReceiveSocket *receiver_;
    std::atomic<bool> confirmed_;

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

    glm::ivec2 icon() const override { return glm::ivec2(11, 8); }

};



#endif // NETWORKSOURCE_H
