#ifndef STREAMER_H
#define STREAMER_H

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
    void setSession(Session *se);
    void removeStreams(const std::string &clientname);

    bool busy() const;
    std::vector<std::string> listStreams() const;

protected:
    void addStream(const std::string &sender, int reply_to, const std::string &clientname);
    void refuseStream(const std::string &sender, int reply_to);
    void removeStream(const std::string &sender, int port);

private:

    bool enabled_;
    StreamingRequestListener listener_;
    UdpListeningReceiveSocket *receiver_;

    Session *session_;
    int width_;
    int height_;    

    // TODO Mutex to protect access to list of streamers
    std::vector<VideoStreamer *> streamers_;
};

class VideoStreamer : public FrameGrabber
{
    friend class Streaming;

    // Frame buffer information
    FrameBuffer  *frame_buffer_;
    uint width_;
    uint height_;

    // connection information
    NetworkToolkit::StreamConfig config_;

    // operation
    std::atomic<bool> streaming_;
    std::atomic<bool> accept_buffer_;

    // gstreamer pipeline
    GstElement   *pipeline_;
    GstAppSrc    *src_;
    GstClockTime timeframe_;
    GstClockTime timestamp_;
    GstClockTime frame_duration_;

    static void callback_need_data (GstAppSrc *, guint, gpointer user_data);
    static void callback_enough_data (GstAppSrc *, gpointer user_data);

public:

    VideoStreamer(NetworkToolkit::StreamConfig conf);
    ~VideoStreamer();

    void addFrame(FrameBuffer *frame_buffer, float dt) override;
    void stop() override;
    std::string info() override;
    double duration() override;
    bool busy() override;
};

#endif // STREAMER_H
