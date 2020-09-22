#ifndef DEVICESOURCE_H
#define DEVICESOURCE_H

#include "StreamSource.h"

class Device : public Stream
{
public:

    Device();
    void open( uint deviceid );

    glm::ivec2 resolution();

private:
    uint device_;
};

class DeviceSource : public StreamSource
{
public:
    DeviceSource();

    // Source interface
    void accept (Visitor& v) override;

    // StreamSource interface
    Stream *stream() const override { return stream_; }

    // specific interface
    Device *device() const;
    void setDevice(int id);

};

#endif // DEVICESOURCE_H
