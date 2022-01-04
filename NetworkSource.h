#ifndef NETWORKSOURCE_H
#define NETWORKSOURCE_H

#include "NetworkToolkit.h"
#include "Connection.h"
#include "StreamSource.h"


class NetworkStream : public Stream
{
public:

    NetworkStream();

    void connect(const std::string &nameconnection);
    bool connected() const;
    void disconnect();

    void update() override;

    glm::ivec2 resolution() const;
    inline NetworkToolkit::Protocol protocol() const { return config_.protocol; }
    std::string clientAddress() const;
    std::string serverAddress() const;

protected:
    class ResponseListener : public osc::OscPacketListener
    {
    protected:
        class NetworkStream *parent_;
        virtual void ProcessMessage( const osc::ReceivedMessage& m,
                                     const IpEndpointName& remoteEndpoint );
    public:
        inline void setParent(NetworkStream *s) { parent_ = s; }
        ResponseListener() : parent_(nullptr) {}
    };

private:
    // connection information
    ConnectionInfo streamer_;
    ResponseListener listener_;
    UdpListeningReceiveSocket *receiver_;
    std::atomic<bool> received_config_;
    std::atomic<bool> connected_;

    NetworkToolkit::StreamConfig config_;
};


class NetworkSource : public StreamSource
{
    std::string connection_name_;

public:
    NetworkSource(uint64_t id = 0);
    ~NetworkSource();

    // Source interface
    void accept (Visitor& v) override;

    // StreamSource interface
    Stream *stream() const override { return stream_; }
    NetworkStream *networkStream() const;

    // specific interface
    void setConnection(const std::string &nameconnection);
    std::string connection() const;

    glm::ivec2 icon() const override;
    std::string info() const override;

};



#endif // NETWORKSOURCE_H
