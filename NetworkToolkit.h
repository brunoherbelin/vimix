#ifndef NETWORKTOOLKIT_H
#define NETWORKTOOLKIT_H

#include <string>
#include <vector>

#define OSC_PREFIX "/vimix"
#define OSC_PING "/ping"
#define OSC_PONG "/pong"
#define OSC_STREAM_REQUEST "/request"
#define OSC_STREAM_OFFER "/offer"
#define OSC_STREAM_REJECT "/reject"
#define OSC_STREAM_DISCONNECT "/disconnect"


#define MAX_HANDSHAKE 20
#define HANDSHAKE_PORT 71310
#define STREAM_REQUEST_PORT 71510
#define OSC_DIALOG_PORT 71010
#define IP_MTU_SIZE 1536

namespace NetworkToolkit
{

typedef enum {
    SHM_RAW = 0,
    UDP_JPEG,
    UDP_H264,
    TCP_JPEG,
    TCP_H264,
    DEFAULT
} Protocol;


struct StreamConfig {

    Protocol protocol;
    std::string client_name;
    std::string client_address;
    int port;
    int width;
    int height;

    StreamConfig () {
        protocol = DEFAULT;
        client_name = "";
        client_address = "127.0.0.1";
        port = 0;
        width = 0;
        height = 0;
    }

    inline StreamConfig& operator = (const StreamConfig& o)
    {
        if (this != &o) {
            this->client_name = o.client_name;
            this->client_address = o.client_address;
            this->port = o.port;
            this->protocol = o.protocol;
            this->width = o.width;
            this->height = o.height;
        }
        return *this;
    }
};

extern const char* protocol_name[DEFAULT];
extern const std::vector<std::string> protocol_send_pipeline;
extern const std::vector<std::string> protocol_receive_pipeline;

std::string hostname();
std::vector<std::string> host_ips();
bool is_host_ip(const std::string &ip);
std::string closest_host_ip(const std::string &ip);

}

#endif // NETWORKTOOLKIT_H
