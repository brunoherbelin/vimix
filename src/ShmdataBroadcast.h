#ifndef SHMDATABROADCAST_H
#define SHMDATABROADCAST_H

#include "FrameGrabber.h"

#define SHMDATA_DEFAULT_PATH "/tmp/shm_vimix"
#define SHMDATA_FPS 30

class ShmdataBroadcast : public FrameGrabber
{
public:

    enum Method {
        SHM_SHMSINK,
        SHM_SHMDATASINK,
        SHM_SHMDATAANY
    };

    ShmdataBroadcast(Method m = SHM_SHMSINK, const std::string &socketpath = "");
    virtual ~ShmdataBroadcast() {}

    static bool available(Method m = SHM_SHMDATAANY);

    inline Method method() const { return method_; }
    inline std::string socket_path() const { return socket_path_; }
    std::string gst_pipeline() const;

    std::string info() const override;

private:
    std::string init(GstCaps *caps) override;
    void terminate() override;

    // connection information
    Method method_;
    std::string socket_path_;
};

#endif // SHMDATABROADCAST_H
