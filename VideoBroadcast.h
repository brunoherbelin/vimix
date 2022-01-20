#ifndef VIDEOBROADCAST_H
#define VIDEOBROADCAST_H

#include "NetworkToolkit.h"
#include "FrameGrabber.h"

#define BROADCAST_FPS 30

class VideoBroadcast : public FrameGrabber
{
    friend class Streaming;

    std::string init(GstCaps *caps) override;
    void terminate() override;
    void stop() override;

    // connection information
    int port_;
    std::atomic<bool> stopped_;

public:

    VideoBroadcast();
    virtual ~VideoBroadcast() {}
    std::string info() const override;
};

#endif // VIDEOBROADCAST_H
