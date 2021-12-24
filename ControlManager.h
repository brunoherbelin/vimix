#ifndef CONTROL_H
#define CONTROL_H

#include "NetworkToolkit.h"

#define OSC_INFO               "/info"
#define OSC_INFO_SYNC          "/sync"
#define OSC_INFO_LOG           "/log"

#define OSC_OUTPUT             "/output"
#define OSC_OUTPUT_ENABLE      "/enable"
#define OSC_OUTPUT_DISABLE     "/disable"
#define OSC_OUTPUT_FADING      "/fading"

#define OSC_ALL                "/all"
#define OSC_SELECTED           "/selected"
#define OSC_CURRENT            "/current"
#define OSC_VERSION            "/version"
#define OSC_NEXT               "/next"
#define OSC_PREVIOUS           "/previous"

#define OSC_SOURCE_NAME        "/name"
#define OSC_SOURCE_PLAY        "/play"
#define OSC_SOURCE_PAUSE       "/pause"
#define OSC_SOURCE_REPLAY      "/replay"
#define OSC_SOURCE_ALPHA       "/alpha"
#define OSC_SOURCE_TRANSPARENCY "/transparency"
#define OSC_SOURCE_DEPTH       "/depth"
#define OSC_SOURCE_GRAB        "/grab"
#define OSC_SOURCE_RESIZE      "/resize"
#define OSC_SOURCE_TURN        "/turn"
#define OSC_SOURCE_RESET       "/reset"


class Session;
class Source;

class Control
{
    // Private Constructor
    Control();
    Control(Control const& copy) = delete;
    Control& operator=(Control const& copy) = delete;

public:

    static Control& manager ()
    {
        // The only instance
        static Control _instance;
        return _instance;
    }
    ~Control();

    bool init();
    void terminate();

    // void setOscPort(int P);

protected:

    class RequestListener : public osc::OscPacketListener {
    protected:
        virtual void ProcessMessage( const osc::ReceivedMessage& m,
                                     const IpEndpointName& remoteEndpoint );
        std::string FullMessage( const osc::ReceivedMessage& m );
    };

    void receiveOutputAttribute(const std::string &attribute,
                            osc::ReceivedMessageArgumentStream arguments);

    void receiveSourceAttribute(Source *target, const std::string &attribute,
                            osc::ReceivedMessageArgumentStream arguments);

    void sendCurrentSourceAttibutes(const IpEndpointName& remoteEndpoint);
    void sendSourcesStatus(const IpEndpointName& remoteEndpoint, float max_count = 0.f);
    void sendStatus(const IpEndpointName& remoteEndpoint);

private:

    static void listen();
    RequestListener listener_;
    UdpListeningReceiveSocket *receiver_;

};

#endif // CONTROL_H
