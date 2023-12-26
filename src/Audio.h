#ifndef AUDIO_H
#define AUDIO_H

#include <string>
#include <mutex>
#include <condition_variable>
#include <vector>

#include <gst/gst.h>

struct AudioHandle {
    std::string name;
    bool is_monitor;
    std::string pipeline;
    AudioHandle() : is_monitor(false) { }
};

class Audio
{
    Audio();
    Audio(Audio const& copy) = delete;
    Audio& operator=(Audio const& copy) = delete;

public:

    static Audio& manager()
    {
        // The only instance
        static Audio _instance;
        return _instance;
    }

    void initialize();

    int numDevices ();
    std::string name (int index);
    std::string pipeline (int index);
    bool is_monitor (int index);

    int  index  (const std::string &device);
    bool exists (const std::string &device);

    static gboolean callback_audio_monitor (GstBus *, GstMessage *, gpointer);

private:

    static void launchMonitoring(Audio *d);
    static bool _initialized();
    void add(GstDevice *device);
    void remove(GstDevice *device);

    GstDeviceMonitor *monitor_;
    std::condition_variable monitor_initialization_;
    bool monitor_initialized_;
    bool monitor_unplug_event_;

    std::mutex access_;
    std::vector< AudioHandle > handles_;

};

#endif // AUDIO_H
