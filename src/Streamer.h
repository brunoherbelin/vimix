#ifndef STREAMER_H
#define STREAMER_H

#include <mutex>

#include "osc/OscReceivedElements.h"
#include "osc/OscPacketListener.h"
#include "ip/UdpSocket.h"

#include "NetworkToolkit.h"
#include "FrameGrabber.h"

#define STREAMING_FPS 30

class VideoStreamer;

class Streaming
{
    // Private Constructor
    Streaming();
    Streaming(Streaming const& copy) = delete;
    Streaming& operator=(Streaming const& copy) = delete;

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
    NetworkToolkit::StreamConfig removeStream(const std::string &sender, int port);
    void removeStream(const VideoStreamer *vs);
    void addStream(const std::string &sender, int port, const std::string &clientname);

    bool busy();
    std::vector<std::string> listStreams();

protected:

    class RequestListener : public osc::OscPacketListener {

    protected:
        virtual void ProcessMessage( const osc::ReceivedMessage& m,
                                     const IpEndpointName& remoteEndpoint );
    };
    void _addStream(const std::string &sender, int reply_to, const std::string &clientname,
                   NetworkToolkit::StreamProtocol protocol = NetworkToolkit::DEFAULT);
    void _refuseStream(const std::string &sender, int reply_to);

private:

    bool enabled_;
    RequestListener listener_;
    UdpListeningReceiveSocket *receiver_;

    std::vector<VideoStreamer *> streamers_;    
    std::mutex streamers_lock_;

    std::list<std::string> shm_blacklist_;
};

class VideoStreamer : public FrameGrabber
{
    friend class Streaming;

    std::string init(GstCaps *caps) override;
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
