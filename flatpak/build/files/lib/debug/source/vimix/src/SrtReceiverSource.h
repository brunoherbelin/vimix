#ifndef SRTRECEIVERSOURCE_H
#define SRTRECEIVERSOURCE_H

#include "StreamSource.h"

class SrtReceiverSource : public StreamSource
{
    std::string ip_;
    std::string port_;

public:
    SrtReceiverSource(uint64_t id = 0);

    // Source interface
    void accept (Visitor& v) override;

    // StreamSource interface
    Stream *stream() const override { return stream_; }

    // specific interface
    void setConnection(const std::string &ip, const std::string &port);
    std::string ip()   const { return ip_; }
    std::string port() const { return port_; }
    std::string uri()  const;

    glm::ivec2 icon()  const override;
    std::string info() const override;

};


#endif // SRTRECEIVERSOURCE_H
