#ifndef CONTROL_H
#define CONTROL_H

#include <glm/fwd.hpp>
#include <map>
#include <list>
#include <string>
#include <atomic>
#include <condition_variable>

#include "osc/OscReceivedElements.h"
#include "osc/OscPacketListener.h"
#include "ip/UdpSocket.h"

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
#define OSC_SELECTION          "/selection"
#define OSC_SOURCEID           "(\\/#?)[[:digit:]]+$"
#define OSC_BATCH              "(\\/batch#)[[:digit:]]+$"
#define OSC_CURRENT            "/current"
#define OSC_NEXT               "/next"
#define OSC_PREVIOUS           "/previous"
#define OSC_METRONOME          "/metronome"
#define OSC_METRO_SYNC         "/sync"

#define OSC_SOURCE_NAME        "/name"
#define OSC_SOURCE_RENAME      "/rename"
#define OSC_SOURCE_ALIAS       "/alias"
#define OSC_SOURCE_LOCK        "/lock"
#define OSC_SOURCE_PLAY        "/play"
#define OSC_SOURCE_PAUSE       "/pause"
#define OSC_SOURCE_REPLAY      "/replay"
#define OSC_SOURCE_RELOAD      "/reload"
#define OSC_SOURCE_ALPHA       "/alpha"
#define OSC_SOURCE_LOOM        "/loom"
#define OSC_SOURCE_TRANSPARENCY "/transparency"
#define OSC_SOURCE_DEPTH       "/depth"
#define OSC_SOURCE_GRAB        "/grab"
#define OSC_SOURCE_RESIZE      "/resize"
#define OSC_SOURCE_TURN        "/turn"
#define OSC_SOURCE_RESET       "/reset"
#define OSC_SOURCE_POSITION    "/position"
#define OSC_SOURCE_CORNER      "/corner"
#define OSC_SOURCE_SIZE        "/size"
#define OSC_SOURCE_ANGLE       "/angle"
#define OSC_SOURCE_SEEK        "/seek"
#define OSC_SOURCE_FFWD        "/ffwd"
#define OSC_SOURCE_SPEED       "/speed"
#define OSC_SOURCE_CONTENTS    "/contents"
#define OSC_SOURCE_BRIGHTNESS  "/brightness"
#define OSC_SOURCE_CONTRAST    "/contrast"
#define OSC_SOURCE_SATURATION  "/saturation"
#define OSC_SOURCE_HUE         "/hue"
#define OSC_SOURCE_THRESHOLD   "/threshold"
#define OSC_SOURCE_GAMMA       "/gamma"
#define OSC_SOURCE_COLOR       "/color"
#define OSC_SOURCE_POSTERIZE   "/posterize"
#define OSC_SOURCE_INVERT      "/invert"
#define OSC_SOURCE_CORRECTION  "/correction"
#define OSC_SOURCE_CROP        "/crop"
#define OSC_SOURCE_TEXPOSITION "/texture_position"
#define OSC_SOURCE_TEXSIZE     "/texture_size"
#define OSC_SOURCE_TEXANGLE    "/texture_angle"
#define OSC_SOURCE_FILTER      "/filter"
#define OSC_SOURCE_UNIFORM     "/uniform"
#define OSC_SOURCE_BLENDING    "/blending"

#define OSC_SESSION            "/session"
#define OSC_SESSION_VERSION    "/version"
#define OSC_SESSION_OPEN       "/open"
#define OSC_SESSION_SAVE       "/save"
#define OSC_SESSION_CLOSE      "/close"

#define OSC_STREAM             "/peertopeer"
#define OSC_MULTITOUCH         "/multitouch"

#define INPUT_UNDEFINED        0
#define INPUT_KEYBOARD_FIRST   1
#define INPUT_KEYBOARD_COUNT   25
#define INPUT_KEYBOARD_LAST    26
#define INPUT_NUMPAD_FIRST     27
#define INPUT_NUMPAD_COUNT     16
#define INPUT_NUMPAD_LAST      43
#define INPUT_JOYSTICK_FIRST   44
#define INPUT_JOYSTICK_COUNT   20
#define INPUT_JOYSTICK_LAST    64
#define INPUT_JOYSTICK_FIRST_BUTTON  44
#define INPUT_JOYSTICK_COUNT_BUTTON  15
#define INPUT_JOYSTICK_LAST_BUTTON   58
#define INPUT_JOYSTICK_FIRST_AXIS    59
#define INPUT_JOYSTICK_COUNT_AXIS    6
#define INPUT_JOYSTICK_LAST_AXIS     64
#define INPUT_MULTITOUCH_FIRST 65
#define INPUT_MULTITOUCH_COUNT 16
#define INPUT_MULTITOUCH_LAST  81
#define INPUT_TIMER_FIRST      82
#define INPUT_TIMER_LAST       114
#define INPUT_MAX              115


class Session;
class Source;
class SourceCallback;
class GLFWwindow;

class Control
{
    friend class RenderingWindow;

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
    void update();
    void terminate();


    bool  inputActive (uint id);
    float inputValue  (uint id);
    float inputDelay  (uint id);
    static std::string inputLabel(uint id);
    static int layoutKey(int key);

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
    bool receiveBatchAttribute(int batch, const std::string &attribute,
                            osc::ReceivedMessageArgumentStream arguments);
    bool receiveSessionAttribute(const std::string &attribute,
                            osc::ReceivedMessageArgumentStream arguments);
    void receiveMultitouchAttribute(const std::string &attribute,
                                    osc::ReceivedMessageArgumentStream arguments);
    void sendSourceAttibutes(const IpEndpointName& remoteEndpoint,
                             std::string target, Source *s = nullptr);
    void sendSourcesStatus(const IpEndpointName& remoteEndpoint,
                           osc::ReceivedMessageArgumentStream arguments);
    void sendBatchStatus(const IpEndpointName& remoteEndpoint);
    void sendOutputStatus(const IpEndpointName& remoteEndpoint);

    void receiveStreamAttribute(const std::string &attribute,
                            osc::ReceivedMessageArgumentStream arguments, const std::string &sender);

    static void keyboardCalback(GLFWwindow*, int, int, int, int);

    // OSC translation
    std::string translate (std::string addresspqattern);
    std::string alias (std::string target);

private:

    static void listen();
    RequestListener listener_;
    std::condition_variable receiver_end_;
    UdpListeningReceiveSocket *receiver_;

    std::map<std::string, std::string> aliases_;
    std::map<std::string, std::string> translation_;
    void loadOscConfig();
    void resetOscConfig();

    bool  input_active[INPUT_MAX];
    float input_values[INPUT_MAX];
    std::mutex input_access_;

    int   multitouch_active[INPUT_MULTITOUCH_COUNT];
    glm::vec2 multitouch_values[INPUT_MULTITOUCH_COUNT];


};

#endif // CONTROL_H
