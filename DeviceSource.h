#ifndef DEVICESOURCE_H
#define DEVICESOURCE_H

#include "Stream.h"
#include "Source.h"

class Device : public Stream
{
public:

    Device();
    void open( uint deviceid );

    glm::ivec2 resolution();

private:
    void open( std::string description ) override;
    uint device_;
};

class DeviceSource : public Source
{
public:
    DeviceSource();
    ~DeviceSource();

    // implementation of source API
    void update (float dt) override;
    void setActive (bool on) override;
    void render() override;
    bool failed() const override;
    uint texture() const override;
    void accept (Visitor& v) override;

    // Pattern specific interface
    inline Device *device() const { return stream_; }
    void setDevice(int id);

protected:

    void init() override;
    void replaceRenderingShader() override;

    Surface *devicesurface_;
    Device  *stream_;
};

#endif // DEVICESOURCE_H
