#ifndef VIDEOBROADCAST_H
#define VIDEOBROADCAST_H

#include "NetworkToolkit.h"
#include "FrameGrabber.h"

#define BROADCAST_FPS 30

class VideoBroadcast : public FrameGrabber
{
public:

    VideoBroadcast(NetworkToolkit::BroadcastProtocol p = NetworkToolkit::BROADCAST_DEFAULT, int port = 8888);
    virtual ~VideoBroadcast() {}

    void stop() override;
    std::string info() const override;

private:
    std::string init(GstCaps *caps) override;
    void terminate() override;

    // connection information
    NetworkToolkit::BroadcastProtocol protocol_;
    int port_;
    std::atomic<bool> stopped_;
};

#endif // VIDEOBROADCAST_H
