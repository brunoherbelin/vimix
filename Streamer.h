#ifndef STREAMER_H
#define STREAMER_H

#include <mutex>

#include <gst/pbutils/pbutils.h>
#include <gst/app/gstappsrc.h>

#include "osc/OscReceivedElements.h"
#include "osc/OscPacketListener.h"
#include "ip/UdpSocket.h"

#include "NetworkToolkit.h"
#include "FrameGrabber.h"

class Session;
class VideoStreamer;

class StreamingRequestListener : public osc::OscPacketListener {

protected:
    virtual void ProcessMessage( const osc::ReceivedMessage& m,
                                 const IpEndpointName& remoteEndpoint );
};

class Streaming
{
    friend class StreamingRequestListener;

    // Private Constructor
    Streaming();
    Streaming(Streaming const& copy);            // Not Implemented
    Streaming& operator=(Streaming const& copy); // Not Implemented

public:

    static Streaming& manager()
    {
        // The only instance
        static Streaming _instance;
        return _instance;
    }
    ~Streaming();

    void enable(bool on);
    inline bool enabled() const { return enabled_; }
    void removeStreams(const std::string &clientname);
    void removeStream(const std::string &sender, int port);
    void removeStream(const VideoStreamer *vs);

    bool busy();
    std::vector<std::string> listStreams();

protected:
    void addStream(const std::string &sender, int reply_to, const std::string &clientname);
    void refuseStream(const std::string &sender, int reply_to);

private:

    bool enabled_;
    StreamingRequestListener listener_;
    UdpListeningReceiveSocket *receiver_;

    std::vector<VideoStreamer *> streamers_;    
    std::mutex streamers_lock_;
};

class VideoStreamer : public FrameGrabber
{
    friend class Streaming;

    void init(GstCaps *caps) override;
    void terminate() override;
    void stop() override;

    // connection information
    NetworkToolkit::StreamConfig config_;
    std::atomic<bool> stopped_;

public:

    VideoStreamer(const NetworkToolkit::StreamConfig &conf);
    virtual ~VideoStreamer() {}
    std::string info() const override;

};

#endif // STREAMER_H
