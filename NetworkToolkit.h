#ifndef NETWORKTOOLKIT_H
#define NETWORKTOOLKIT_H

#include <string>
#include <vector>

namespace NetworkToolkit
{

typedef enum {
    TCP_JPEG = 0,
    TCP_H264,
    SHM_JPEG,
    DEFAULT
} Protocol;

extern const char* protocol_name[DEFAULT];
extern const std::vector<std::string> protocol_broadcast_pipeline;
extern const std::vector<std::string> protocol_receive_pipeline;

std::vector<std::string> host_ips();

}

#endif // NETWORKTOOLKIT_H
