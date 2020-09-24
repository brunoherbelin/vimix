#ifndef DEVICESOURCE_H
#define DEVICESOURCE_H

#include <vector>
#include <set>

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
    std::string pipeline (int index) const;

    bool exists (const std::string &device) const;
    std::string pipeline (const std::string &device) const;

    static gboolean callback_device_monitor (GstBus *, GstMessage *, gpointer);

private:

    void remove(const char *device);
    std::vector< std::string > names_;
    std::vector< std::string > pipelines_;
    GstDeviceMonitor *monitor_;
};


struct DeviceInfo {
    gint width;
    gint height;
    gint fps_numerator;
    gint fps_denominator;
    std::string format;

    DeviceInfo() {
        width = 0;
        height = 0;
        fps_numerator = 1;
        fps_denominator = 1;
        format = "";
    }

    inline DeviceInfo& operator = (const DeviceInfo& b)
    {
        if (this != &b) {
            this->width = b.width;
            this->height = b.height;
            this->fps_numerator = b.fps_numerator;
            this->fps_denominator = b.fps_denominator;
            this->format = b.format;
        }
        return *this;
    }

    inline bool operator < (const DeviceInfo b) const
    {
        return ( this->fps_numerator * this->height < b.fps_numerator * b.height );
    }
};

struct better_device_comparator
{
    inline bool operator () (const DeviceInfo a, const DeviceInfo b) const
    {
        return (a < b);
    }
};

typedef std::set<DeviceInfo, better_device_comparator> DeviceInfoSet;

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

    DeviceInfoSet getDeviceConfigs(const std::string &pipeline);

};

#endif // DEVICESOURCE_H
