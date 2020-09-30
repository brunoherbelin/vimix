#ifndef DEVICESOURCE_H
#define DEVICESOURCE_H

#include <vector>
#include <set>

#include "GstToolkit.h"
#include "StreamSource.h"

struct DeviceConfig {
    gint width;
    gint height;
    gint fps_numerator;
    gint fps_denominator;
    std::string stream;
    std::string format;

    DeviceConfig() {
        width = 0;
        height = 0;
        fps_numerator = 1;
        fps_denominator = 1;
        stream = "";
        format = "";
    }

    inline DeviceConfig& operator = (const DeviceConfig& b)
    {
        if (this != &b) {
            this->width = b.width;
            this->height = b.height;
            this->fps_numerator = b.fps_numerator;
            this->fps_denominator = b.fps_denominator;
            this->stream = b.stream;
            this->format = b.format;
        }
        return *this;
    }

    inline bool operator < (const DeviceConfig b) const
    {
        int formatscore = this->format.find("R") != std::string::npos ? 2 : 1; // best score for RGBx
        int b_formatscore = b.format.find("R") != std::string::npos ? 2 : 1;
        float fps = static_cast<float>(this->fps_numerator) / static_cast<float>(this->fps_denominator);
        float b_fps = static_cast<float>(b.fps_numerator) / static_cast<float>(b.fps_denominator);
        return ( fps * static_cast<float>(this->height * formatscore) < b_fps * static_cast<float>(b.height * b_formatscore));
    }
};

struct better_device_comparator
{
    inline bool operator () (const DeviceConfig a, const DeviceConfig b) const
    {
        return (a < b);
    }
};

typedef std::set<DeviceConfig, better_device_comparator> DeviceConfigSet;


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
    std::string description (int index) const;
    DeviceConfigSet config (int index) const;

    bool exists (const std::string &device) const;
    bool unplugged (const std::string &device) const;
    int  index  (const std::string &device) const;

    static gboolean callback_device_monitor (GstBus *, GstMessage *, gpointer);
    static DeviceConfigSet getDeviceConfigs(const std::string &src_description);

private:

    void remove(const char *device);
    std::vector< std::string > src_name_;
    std::vector< std::string > src_description_;

    std::vector< DeviceConfigSet > src_config_;

    bool list_uptodate_;
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
