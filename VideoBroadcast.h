#ifndef VIDEOBROADCAST_H
#define VIDEOBROADCAST_H

#include "NetworkToolkit.h"
#include "FrameGrabber.h"
#include "StreamSource.h"

#define BROADCAST_FPS 30

class VideoBroadcast : public FrameGrabber
{
public:

    VideoBroadcast(int port = 7070);
    virtual ~VideoBroadcast() {}

    static bool available();

    void stop() override;
    std::string info() const override;

private:
    std::string init(GstCaps *caps) override;
    void terminate() override;

    // connection information
    int port_;
    std::atomic<bool> stopped_;

    // pipeline elements
    static std::string srt_sink_;
    static std::string h264_encoder_;
};


#endif // VIDEOBROADCAST_H
