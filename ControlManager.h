#ifndef CONTROL_H
#define CONTROL_H

#include <map>
#include <list>
#include <string>
#include <condition_variable>

#include "SourceList.h"
#include "BaseToolkit.h"
#include "NetworkToolkit.h"

#define OSC_SYNC               "/sync"

#define OSC_INFO               "/info"
#define OSC_INFO_LOG           "/log"
#define OSC_INFO_NOTIFY        "/notify"

#define OSC_OUTPUT             "/output"
#define OSC_OUTPUT_ENABLE      "/enable"
#define OSC_OUTPUT_DISABLE     "/disable"
#define OSC_OUTPUT_FADING      "/fading"
#define OSC_OUTPUT_FADE_IN     "/fade-in"
#define OSC_OUTPUT_FADE_OUT    "/fade-out"

#define OSC_ALL                "/all"
#define OSC_SELECTED           "/selected"
#define OSC_CURRENT            "/current"
#define OSC_NEXT               "/next"
#define OSC_PREVIOUS           "/previous"

#define OSC_SOURCE_NAME        "/name"
#define OSC_SOURCE_LOCK        "/lock"
#define OSC_SOURCE_PLAY        "/play"
#define OSC_SOURCE_PAUSE       "/pause"
#define OSC_SOURCE_REPLAY      "/replay"
#define OSC_SOURCE_ALPHA       "/alpha"
#define OSC_SOURCE_LOOM        "/loom"
#define OSC_SOURCE_TRANSPARENCY "/transparency"
#define OSC_SOURCE_DEPTH       "/depth"
#define OSC_SOURCE_GRAB        "/grab"
#define OSC_SOURCE_RESIZE      "/resize"
#define OSC_SOURCE_TURN        "/turn"
#define OSC_SOURCE_RESET       "/reset"

#define OSC_SESSION            "/session"
#define OSC_SESSION_VERSION    "/version"

#define INPUT_UNDEFINED        0
#define INPUT_KEYBOARD_FIRST   1
#define INPUT_KEYBOARD_LAST    26
#define INPUT_NUMPAD_FIRST     27
#define INPUT_NUMPAD_LAST      43
#define INPUT_JOYSTICK_FIRST   44
#define INPUT_JOYSTICK_LAST    65
#define INPUT_CUSTOM_FIRST     66
#define INPUT_CUSTOM_LAST      99
#define INPUT_MAX              100


class Session;
class Source;
class SourceCallback;
class GLFWwindow;

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

    // OSC translation
    std::string translate (std::string addresspqattern);

    static bool inputActive(uint id);
    float inputValue(uint id);
    std::string inputLabel(uint id);

protected:

    // OSC management
    class RequestListener : public osc::OscPacketListener {
    protected:
        virtual void ProcessMessage( const osc::ReceivedMessage& m,
                                     const IpEndpointName& remoteEndpoint );
        std::string FullMessage( const osc::ReceivedMessage& m );
    };

    bool receiveOutputAttribute(const std::string &attribute,
                            osc::ReceivedMessageArgumentStream arguments);
    bool receiveSourceAttribute(Source *target, const std::string &attribute,
                            osc::ReceivedMessageArgumentStream arguments);
    bool receiveSessionAttribute(const std::string &attribute,
                            osc::ReceivedMessageArgumentStream arguments);
    void sendSourceAttibutes(const IpEndpointName& remoteEndpoint,
                             std::string target, Source *s = nullptr);
    void sendSourcesStatus(const IpEndpointName& remoteEndpoint,
                           osc::ReceivedMessageArgumentStream arguments);
    void sendOutputStatus(const IpEndpointName& remoteEndpoint);

private:

    static void listen();
    RequestListener listener_;
    std::condition_variable receiver_end_;
    UdpListeningReceiveSocket *receiver_;

    std::map<std::string, std::string> translation_;
    void loadOscConfig();
    void resetOscConfig();

    static bool input_active[INPUT_MAX];
    static void keyboardCalback(GLFWwindow*, int, int, int, int);
};

#endif // CONTROL_H
