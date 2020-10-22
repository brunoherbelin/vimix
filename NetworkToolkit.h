#ifndef NETWORKTOOLKIT_H
#define NETWORKTOOLKIT_H

#include <string>
#include <vector>

#define OSC_PREFIX "/vimix"
#define OSC_PING "/ping"
#define OSC_PONG "/pong"
#define OSC_STREAM_REQUEST "/request"
#define OSC_STREAM_OFFER "/offer"
#define OSC_STREAM_RETRACT "/retract"
#define OSC_STREAM_CONNECT "/connect"
#define OSC_STREAM_DISCONNECT "/disconnect"


#define MAX_HANDSHAKE 10
#define HANDSHAKE_PORT 71310
#define STREAM_REQUEST_PORT 71510
#define STREAM_RESPONSE_PORT 71610
#define IP_MTU_SIZE 1536

namespace NetworkToolkit
{

typedef enum {
    TCP_JPEG = 0,
    TCP_H264,
    SHM_RAW,
    DEFAULT
} Protocol;

extern const char* protocol_name[DEFAULT];
extern const std::vector<std::string> protocol_send_pipeline;
extern const std::vector<std::string> protocol_receive_pipeline;

std::vector<std::string> host_ips();
bool is_host_ip(const std::string &ip);
std::string closest_host_ip(const std::string &ip);

}

#endif // NETWORKTOOLKIT_H
