#ifndef DEVICESOURCE_H
#define DEVICESOURCE_H

#include <string>
#include <vector>
#include <set>
#include <map>

#include "Toolkit/GstToolkit.h"
#include "StreamSource.h"

class DeviceSource : public StreamSource
{
    friend class Device;

public:
    DeviceSource(uint64_t id = 0);
    ~DeviceSource();

    // Source interface
    Failure failed() const override;
    void accept (Visitor& v) override;
    void setActive (bool on) override;

    // StreamSource interface
    Stream *stream() const override { return stream_; }

    // specific interface
    void setDevice(const std::string &devicename);
    inline std::string  device() const { return device_; }
    void reconnect();

    glm::ivec2 icon() const override;
    inline std::string info() const override;

protected:
    void unplug() { unplugged_ = true; }

private:
    std::string device_;
    std::atomic<bool> unplugged_;
    void unsetDevice();
};

struct DeviceHandle {

    std::string name;
    std::string pipeline;
    std::string properties;
    GstToolkit::PipelineConfigSet configs;

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
    std::string properties (int index) ;
    GstToolkit::PipelineConfigSet config (int index) ;

    int  index  (const std::string &device);
    bool exists (const std::string &device);
    void reload ();

    static gboolean callback_device_monitor (GstBus *, GstMessage *, gpointer);

private:

    static void launchMonitoring(Device *d);
    static bool initialized();
    void add(GstDevice *device);
    void remove(GstDevice *device);

    std::mutex access_;
    std::vector< DeviceHandle > handles_;

    GstDeviceMonitor *monitor_;
    std::condition_variable monitor_initialization_;
    bool monitor_initialized_;

};


#endif // DEVICESOURCE_H
