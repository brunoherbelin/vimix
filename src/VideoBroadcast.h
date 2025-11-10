#ifndef VIDEOBROADCAST_H
#define VIDEOBROADCAST_H

#include "FrameGrabber.h"
#include "StreamSource.h"

#define BROADCAST_DEFAULT_PORT 7070
#define BROADCAST_FPS 30

class VideoBroadcast : public FrameGrabber
{
public:

    VideoBroadcast(int port = BROADCAST_DEFAULT_PORT);
    virtual ~VideoBroadcast() {}

    FrameGrabber::Type type () const override { return FrameGrabber::GRABBER_BROADCAST; }

    static bool available();
    inline int port() const { return port_; }

    void stop() override;
    std::string info(bool extended = false) const override;

private:
    std::string init(GstCaps *caps) override;
    void terminate() override;

    // connection information
    int port_;

    // pipeline elements
    static std::string srt_sink_;
    static std::string srt_encoder_;
};


#endif // VIDEOBROADCAST_H
