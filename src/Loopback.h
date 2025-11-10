#ifndef LOOPBACK_H
#define LOOPBACK_H

#include "FrameGrabber.h"

#define LOOPBACK_DEFAULT_DEVICE 10
#define LOOPBACK_FPS 30

class Loopback : public FrameGrabber
{
public:
    Loopback(int deviceid = LOOPBACK_DEFAULT_DEVICE);
    virtual ~Loopback() {}

    FrameGrabber::Type type () const override { return FrameGrabber::GRABBER_LOOPBACK; }

    static bool available();

    inline int  device_id() const { return loopback_device_; }
    std::string device_name() const;

    void stop() override;
    std::string info(bool extended = false) const override;

private:
    std::string init(GstCaps *caps) override;
    void terminate() override;

    // device information
    int loopback_device_;

    static std::string loopback_sink_;
};


#endif // LOOPBACK_H
