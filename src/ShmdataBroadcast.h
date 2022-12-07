#ifndef SHMDATABROADCAST_H
#define SHMDATABROADCAST_H

#include "FrameGrabber.h"

class ShmdataBroadcast : public FrameGrabber
{
public:
    ShmdataBroadcast(const std::string &socketpath = "/tmp/shmdata_vimix");
    virtual ~ShmdataBroadcast() {}

    static bool available();
    inline std::string socket_path() const { return socket_path_; }

    void stop() override;
    std::string info() const override;

private:
    std::string init(GstCaps *caps) override;
    void terminate() override;

    // connection information
    std::string socket_path_;
    std::atomic<bool> stopped_;

    static std::string shmdata_sink_;
};

#endif // SHMDATABROADCAST_H
