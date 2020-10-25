
#include <algorithm>

#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <arpa/inet.h>

// TODO OSX implementation
#include <linux/netdevice.h>

// OSC IP gethostbyname
#include "ip/NetworkingUtils.h"

#include "NetworkToolkit.h"


/***
 *
 *      TCP Server JPEG : broadcast
 * SND:
 * gst-launch-1.0 videotestsrc is-live=true ! jpegenc ! rtpjpegpay ! rtpstreampay ! tcpserversink port=5400
 * RCV:
 * gst-launch-1.0 tcpclientsrc port=5400 ! application/x-rtp-stream,encoding-name=JPEG ! rtpstreamdepay! rtpjpegdepay ! jpegdec ! autovideosink
 *
 *      TCP Server H264 : broadcast
 * SND:
 * gst-launch-1.0 videotestsrc is-live=true ! x264enc ! rtph264pay ! rtpstreampay ! tcpserversink port=5400
 * RCV:
 * gst-launch-1.0 tcpclientsrc port=5400 ! application/x-rtp-stream,media=video,encoding-name=H264,payload=96,clock-rate=90000 ! rtpstreamdepay ! rtpjitterbuffer ! rtph264depay ! avdec_h264 ! autovideosink
 *
 *      UDP unicast
 * SND
 * gst-launch-1.0 videotestsrc is-live=true !  videoconvert ! video/x-raw, format=RGB, framerate=30/1 ! queue max-size-buffers=3 ! jpegenc ! rtpjpegpay ! udpsink port=5000 host=127.0.0.1
 * RCV
 * gst-launch-1.0 udpsrc port=5000 ! application/x-rtp,encoding-name=JPEG,payload=26 ! rtpjpegdepay ! jpegdec ! autovideosink
 *
 * *      UDP multicast : hass to know the PORT and IP of all clients
 * SND
 * gst-launch-1.0 videotestsrc is-live=true ! jpegenc ! rtpjpegpay ! multiudpsink clients="127.0.0.1:5000,127.0.0.1:5001"
 * RCV
 * gst-launch-1.0 -v udpsrc port=5000 ! application/x-rtp,encoding-name=JPEG,payload=26 ! rtpjpegdepay ! jpegdec ! autovideosink
 * gst-launch-1.0 -v udpsrc port=5001 ! application/x-rtp,encoding-name=JPEG,payload=26 ! rtpjpegdepay ! jpegdec ! autovideosink
 *
 *        RAW UDP (caps has to match exactly, and depends on resolution)
 * SND
 * gst-launch-1.0 -v videotestsrc is-live=true ! video/x-raw,format=RGBA,width=1920,height=1080 ! rtpvrawpay ! udpsink port=5000 host=127.0.0.1
 * RCV
 * gst-launch-1.0 udpsrc port=5000 caps = "application/x-rtp, media=(string)video, clock-rate=(int)90000, encoding-name=(string)RAW, sampling=(string)RGBA, depth=(string)8, width=(string)1920, height=(string)1080, colorimetry=(string)SMPTE240M, payload=(int)96, ssrc=(uint)2272750581, timestamp-offset=(uint)1699493959, seqnum-offset=(uint)14107, a-framerate=(string)30" ! rtpvrawdepay ! videoconvert ! autovideosink
 *
 *
 *       SHM RAW RGB
 * SND
 * gst-launch-1.0 videotestsrc is-live=true ! video/x-raw, format=RGB, framerate=30/1 ! shmsink socket-path=/tmp/blah
 * RCV
 * gst-launch-1.0 shmsrc is-live=true socket-path=/tmp/blah ! video/x-raw, format=RGB, framerate=30/1, width=320, height=240 ! videoconvert ! autovideosink
 *
 * */

const char* NetworkToolkit::protocol_name[NetworkToolkit::DEFAULT] = {
    "Shared Memory",
    "UDP RTP Stream JPEG",
    "UDP RTP Stream H264",
    "TCP Broadcast JPEG",
    "TCP Broadcast H264"
};

const std::vector<std::string> NetworkToolkit::protocol_send_pipeline {

    "video/x-raw, format=RGB, framerate=30/1 ! queue max-size-buffers=3 ! shmsink buffer-time=100000 wait-for-connection=true name=sink",
    "video/x-raw, format=I420, framerate=30/1 ! queue max-size-buffers=3 ! jpegenc ! rtpjpegpay ! udpsink name=sink",
    "video/x-raw, format=I420, framerate=30/1 ! queue max-size-buffers=3 ! x264enc tune=\"zerolatency\" threads=2 ! rtph264pay ! udpsink name=sink",
    "video/x-raw, format=I420 ! queue max-size-buffers=3 ! jpegenc ! rtpjpegpay ! rtpstreampay ! tcpserversink name=sink",
    "video/x-raw, format=I420 ! queue max-size-buffers=3 ! x264enc tune=\"zerolatency\" threads=2 ! rtph264pay ! rtpstreampay ! tcpserversink name=sink"
};

const std::vector<std::string> NetworkToolkit::protocol_receive_pipeline {

    "shmsrc socket-path=XXXX ! video/x-raw, format=RGB, framerate=30/1 ! queue max-size-buffers=3",
    "udpsrc buffer-size=200000 port=XXXX ! application/x-rtp,encoding-name=JPEG,payload=26 ! queue max-size-buffers=3 ! rtpjpegdepay ! jpegdec",
    "udpsrc buffer-size=200000 port=XXXX ! application/x-rtp,media=video,encoding-name=H264,payload=96,clock-rate=90000 ! queue max-size-buffers=3 ! rtph264depay ! avdec_h264",
    "tcpclientsrc timeout=1 port=XXXX ! queue max-size-buffers=3 ! application/x-rtp-stream,media=video,encoding-name=JPEG,payload=26 ! rtpstreamdepay ! rtpjpegdepay ! jpegdec",
    "tcpclientsrc timeout=1 port=XXXX ! queue max-size-buffers=3 ! application/x-rtp-stream,media=video,encoding-name=H264,payload=96,clock-rate=90000 ! rtpstreamdepay ! rtph264depay ! avdec_h264"
};


std::vector<std::string> ipstrings;
std::vector<unsigned long> iplongs;

std::vector<std::string> NetworkToolkit::host_ips()
{
    // fill the list of IPs only once
    if (ipstrings.empty()) {

        int s = socket(AF_INET, SOCK_STREAM, 0);
        if (s > -1) {
            struct ifconf ifconf;
            struct ifreq ifr[50];
            int ifs;
            int i;

            ifconf.ifc_buf = (char *) ifr;
            ifconf.ifc_len = sizeof ifr;

            if (ioctl(s, SIOCGIFCONF, &ifconf) > -1) {
                ifs = ifconf.ifc_len / sizeof(ifr[0]);
                for (i = 0; i < ifs; i++) {
                    char ip[INET_ADDRSTRLEN];
                    struct sockaddr_in *s_in = (struct sockaddr_in *) &ifr[i].ifr_addr;

                    if (inet_ntop(AF_INET, &s_in->sin_addr, ip, sizeof(ip))) {
                        ipstrings.push_back( std::string(ip) );
                        iplongs.push_back( GetHostByName(ip) );
                    }
                }
                close(s);
            }
        }
    }

    return ipstrings;
}

bool NetworkToolkit::is_host_ip(const std::string &ip)
{
    if ( ip.compare("localhost") == 0)
        return true;

    if (ipstrings.empty())
        host_ips();

    return std::find(ipstrings.begin(), ipstrings.end(), ip) != ipstrings.end();
}

std::string NetworkToolkit::closest_host_ip(const std::string &ip)
{
    std::string address = "localhost";

    if (iplongs.empty())
        host_ips();

    // discard trivial case
    if ( ip.compare("localhost") != 0)
    {
        int index_mini = -1;
        unsigned long host = GetHostByName( ip.c_str() );
        unsigned long mini = host;

        for (int i=0; i < iplongs.size(); i++){
            unsigned long diff = host > iplongs[i] ? host-iplongs[i] : iplongs[i]-host;
            if (diff < mini) {
                mini = diff;
                index_mini = i;
            }
        }

        if (index_mini>0)
            address = ipstrings[index_mini];

    }

    return address;
}

std::string NetworkToolkit::hostname()
{
    char hostname[1024];
    hostname[1023] = '\0';
    gethostname(hostname, 1023);

    return std::string(hostname);
}
