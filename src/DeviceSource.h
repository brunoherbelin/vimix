#ifndef DEVICESOURCE_H
#define DEVICESOURCE_H

#include <string>
#include <vector>
#include <set>

#include "GstToolkit.h"
#include "StreamSource.h"

class DeviceSource : public StreamSource
{
public:
    DeviceSource(uint64_t id = 0);
    ~DeviceSource();

    // Source interface
    bool failed() const override;
    void accept (Visitor& v) override;
    void setActive (bool on) override;

    // StreamSource interface
    Stream *stream() const override { return stream_; }

    // specific interface
    void setDevice(const std::string &devicename);
    inline std::string device() const { return device_; }
    void unplug() { unplugged_ = true; }

    glm::ivec2 icon() const override;
    std::string info() const override;

private:
    std::string device_;
    std::atomic<bool> unplugged_;
    void unsetDevice();
};

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
        fps_numerator = 30;
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

struct DeviceHandle {

    std::string name;
    std::string pipeline;
    DeviceConfigSet configs;

    Stream *stream;
    std::list<DeviceSource *> connected_sources;

    DeviceHandle() {
        stream = nullptr;
    }
};

class Device
{
    friend class DeviceSource;

    Device();
    Device(Device const& copy) = delete;
    Device& operator=(Device const& copy) = delete;

public:

    static Device& manager()
    {
        // The only instance
        static Device _instance;
        return _instance;
    }

    int numDevices () ;
    std::string name (int index) ;
    std::string description (int index) ;
    DeviceConfigSet config (int index) ;

    int  index  (const std::string &device);
    bool exists (const std::string &device) ;

    static gboolean callback_device_monitor (GstBus *, GstMessage *, gpointer);
    static DeviceConfigSet getDeviceConfigs(const std::string &src_description);

private:

    static void launchMonitoring(Device *d);
    static bool initialized();
    void remove(GstDevice *device);
    void add(GstDevice *device);

    std::mutex access_;
    std::vector< DeviceHandle > handles_;

    GstDeviceMonitor *monitor_;
    std::condition_variable monitor_initialization_;
    bool monitor_initialized_;
    bool monitor_unplug_event_;

};


#endif // DEVICESOURCE_H
