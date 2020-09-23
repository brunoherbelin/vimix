#ifndef DEVICESOURCE_H
#define DEVICESOURCE_H

#include <vector>

#include "GstToolkit.h"
#include "StreamSource.h"

class Device
{
    Device();
    Device(Rendering const& copy);            // Not Implemented
    Device& operator=(Rendering const& copy); // Not Implemented

public:

    static Device& manager()
    {
        // The only instance
        static Device _instance;
        return _instance;
    }

    int numDevices () const;
    std::string name (int index) const;
    bool exists (const std::string &device) const;

    std::string pipeline (int index) const;
    std::string pipeline (const std::string &device) const;

    static gboolean callback_device_monitor (GstBus *, GstMessage *, gpointer);

private:

    void remove(const char *device);
    std::vector< std::string > names_;
    std::vector< std::string > pipelines_;
    GstDeviceMonitor *monitor_;
};


class DeviceSource : public StreamSource
{
public:
    DeviceSource();

    // Source interface
    bool failed() const override;
    void accept (Visitor& v) override;

    // StreamSource interface
    Stream *stream() const override { return stream_; }

    // specific interface
    void setDevice(const std::string &devicename);
    inline std::string device() const { return device_; }

private:
    std::string device_;

};

#endif // DEVICESOURCE_H
