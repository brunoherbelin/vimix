#ifndef NETWORKSOURCE_H
#define NETWORKSOURCE_H

#include "NetworkToolkit.h"
#include "StreamSource.h"

class NetworkStream : public Stream
{
public:

    NetworkStream();
    void open(NetworkToolkit::Protocol protocol, const std::string &host, uint port );
    bool ping(int *w, int *h);

    glm::ivec2 resolution();

    inline NetworkToolkit::Protocol protocol() const { return protocol_; }
    inline std::string host() const { return host_; }
    inline uint port() const { return port_; }

private:
    NetworkToolkit::Protocol protocol_;
    std::string host_;
    uint port_;

    static GstFlowReturn callback_sample (GstAppSink *, gpointer );
};

class NetworkSource : public StreamSource
{
public:
    NetworkSource();

    // Source interface
    void accept (Visitor& v) override;

    // StreamSource interface
    Stream *stream() const override { return stream_; }

    // specific interface
    NetworkStream *networkstream() const;
    void connect(NetworkToolkit::Protocol protocol, const std::string &address);

    glm::ivec2 icon() const override { return glm::ivec2(11, 8); }

};

#endif // NETWORKSOURCE_H
