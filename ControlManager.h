#ifndef CONTROL_H
#define CONTROL_H

#include "NetworkToolkit.h"

#define OSC_SEPARATOR          '/'

#define OSC_LOG                "log"
#define OSC_LOG_INFO           "info"

#define OSC_OUTPUT             "output"
#define OSC_OUTPUT_ENABLE      "enable"
#define OSC_OUTPUT_DISABLE     "disable"
#define OSC_OUTPUT_FADING      "fading"

#define OSC_CURRENT            "current"
#define OSC_CURRENT_NONE       "none"
#define OSC_CURRENT_NEXT       "next"
#define OSC_CURRENT_PREVIOUS   "previous"

#define OSC_SOURCE_PLAY        "play"
#define OSC_SOURCE_PAUSE       "pause"
#define OSC_SOURCE_ALPHA       "alpha"
#define OSC_SOURCE_ALPHA_XY    "alphaXY"
#define OSC_SOURCE_ALPHA_X     "alphaX"
#define OSC_SOURCE_ALPHA_Y     "alphaY"
#define OSC_SOURCE_TRANSPARENT "transparency"
#define OSC_SOURCE_POSITION    "position"
#define OSC_SOURCE_POSITION_X  "positionX"
#define OSC_SOURCE_POSITION_Y  "positionY"
#define OSC_SOURCE_SCALE       "scale"
#define OSC_SOURCE_SCALE_X     "scaleX"
#define OSC_SOURCE_SCALE_Y     "scaleY"
#define OSC_SOURCE_ANGLE       "angle"

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
    };

    void setOutputAttribute(const std::string &attribute,
                            osc::ReceivedMessageArgumentStream arguments);

    void setSourceAttribute(Source *target, const std::string &attribute,
                            osc::ReceivedMessageArgumentStream arguments);

private:

    static void listen();
    RequestListener listener_;
    UdpListeningReceiveSocket *receiver_;
};

#endif // CONTROL_H
