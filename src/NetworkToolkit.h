#ifndef NETWORKTOOLKIT_H
#define NETWORKTOOLKIT_H

#include <string>
#include <vector>


#define OSC_SEPARATOR   '/'
#define OSC_PREFIX "/vimix"
#define OSC_PING "/ping"
#define OSC_PONG "/pong"
#define OSC_STREAM_REQUEST "/request"
#define OSC_STREAM_OFFER "/offer"
#define OSC_STREAM_REJECT "/reject"
#define OSC_STREAM_DISCONNECT "/disconnect"

#define IP_MTU_SIZE 1536

namespace NetworkToolkit
{

typedef enum {
    UDP_RAW  = 0,
    UDP_JPEG = 1,
    UDP_H264 = 2,
    SHM_RAW  = 3,
    DEFAULT  = 4
} StreamProtocol;

extern const char* stream_protocol_label[DEFAULT];
extern const std::vector<std::string> stream_send_pipeline;
extern const std::vector< std::pair<std::string, std::string> > stream_h264_send_pipeline;
extern const std::vector<std::string> stream_receive_pipeline;

struct StreamConfig {

    StreamProtocol protocol;
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
};

//typedef enum {
//    BROADCAST_SRT = 0,
//    BROADCAST_DEFAULT
//} BroadcastProtocol;

//extern const char* broadcast_protocol_label[BROADCAST_DEFAULT];
//extern const std::vector<std::string> broadcast_pipeline;

std::string hostname();
std::vector<std::string> host_ips();
bool is_host_ip(const std::string &ip);
std::string closest_host_ip(const std::string &ip);

}

#endif // NETWORKTOOLKIT_H
