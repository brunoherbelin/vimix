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

class StreamingRequestListener : public osc::OscPacketListener {

protected:
    virtual void ProcessMessage( const osc::ReceivedMessage& m,
                                 const IpEndpointName& remoteEndpoint );
};

class Streaming
{
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

    void enable(bool on);
    void setSession(Session *se);

    typedef struct {
        std::string client;
        std::string address;
        NetworkToolkit::Protocol protocol;
        int width;
        int height;
    } offer;

    void makeOffer(const std::string &client);
    void retractOffer(offer o);

    void addStream(const std::string &address);
    void removeStream(const std::string &address);

private:

    std::string hostname_;
    static void listen();
    StreamingRequestListener listener_;
    UdpListeningReceiveSocket *receiver_;

    Session *session_;
    int width_;
    int height_;
    std::vector<offer> offers_;
};

class VideoStreamer : public FrameGrabber
{
    // Frame buffer information
    FrameBuffer  *frame_buffer_;
    uint width_;
    uint height_;

    //
    NetworkToolkit::Protocol protocol_;
    std::string address_;

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

    VideoStreamer(Streaming::offer config);
    ~VideoStreamer();

    void addFrame(FrameBuffer *frame_buffer, float dt) override;
    void stop() override;
    std::string info() override;
    double duration() override;
    bool busy() override;
};

#endif // STREAMER_H
