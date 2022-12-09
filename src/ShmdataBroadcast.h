#ifndef SHMDATABROADCAST_H
#define SHMDATABROADCAST_H

#include "FrameGrabber.h"

#define SHMDATA_DEFAULT_PATH "/tmp/shmdata_vimix"
#define SHMDATA_FPS 30

class ShmdataBroadcast : public FrameGrabber
{
public:
    ShmdataBroadcast(const std::string &socketpath = SHMDATA_DEFAULT_PATH);
    virtual ~ShmdataBroadcast() {}

    static bool available();

    inline std::string socket_path() const { return socket_path_; }

    std::string info() const override;

private:
    std::string init(GstCaps *caps) override;
    void terminate() override;

    // connection information
    std::string socket_path_;

    static std::string shmdata_sink_;
};

#endif // SHMDATABROADCAST_H
