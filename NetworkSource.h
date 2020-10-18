#ifndef NETWORKSOURCE_H
#define NETWORKSOURCE_H

#include "NetworkToolkit.h"
#include "StreamSource.h"

class NetworkStream : public Stream
{
public:

    NetworkStream();
    void open(NetworkToolkit::Protocol protocol, const std::string &address, uint port );

    glm::ivec2 resolution();

    inline NetworkToolkit::Protocol protocol() const { return protocol_; }
    inline std::string address() const { return address_; }
    inline uint port() const { return port_; }

private:
    NetworkToolkit::Protocol protocol_;
    std::string address_;
    uint port_;
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
    void connect(NetworkToolkit::Protocol protocol, const std::string &address, uint port);

    glm::ivec2 icon() const override { return glm::ivec2(11, 8); }

};

#endif // NETWORKSOURCE_H
