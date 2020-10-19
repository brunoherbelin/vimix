
#include <stdio.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/netdevice.h>
#include <arpa/inet.h>

#include "NetworkToolkit.h"


const char* NetworkToolkit::protocol_name[NetworkToolkit::DEFAULT] = {
    "TCP Broadcast JPEG",
    "TCP Broadcast H264",
    "Shared Memory"
};

const std::vector<std::string> NetworkToolkit::protocol_broadcast_pipeline {

    "video/x-raw, format=I420 ! queue max-size-buffers=3 ! jpegenc ! rtpjpegpay ! rtpstreampay ! tcpserversink name=sink",
    "video/x-raw, format=I420 ! queue max-size-buffers=3 ! x264enc tune=\"zerolatency\" threads=2 ! rtph264pay ! rtpstreampay ! tcpserversink name=sink",
    "video/x-raw, format=RGB, framerate=30/1 ! queue max-size-buffers=3 ! shmsink buffer-time=100000 name=sink"
};


const std::vector<std::string> NetworkToolkit::protocol_receive_pipeline {

    " ! application/x-rtp-stream,media=video,encoding-name=JPEG,payload=26 ! rtpstreamdepay ! rtpjpegdepay ! jpegdec",
    " ! application/x-rtp-stream,media=video,encoding-name=H264,payload=96,clock-rate=90000 ! rtpstreamdepay ! rtph264depay ! avdec_h264",
    " ! video/x-raw, format=RGB, framerate=30/1"
};


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
 * gst-launch-1.0 videotestsrc is-live=true ! jpegenc ! rtpjpegpay ! udpsink port=5000 host=127.0.0.1 sync=false
 * RCV
 * gst-launch-1.0 udpsrc port=5000 ! application/x-rtp,encoding-name=JPEG,payload=26 ! rtpjpegdepay ! jpegdec ! autovideosink
 *
 * *      UDP multicast : hass to know the PORT and IP of all clients
 * SND
 * gst-launch-1.0 videotestsrc is-live=true ! jpegenc ! rtpjpegpay ! multiudpsink clients="127.0.0.1:5000,127.0.0.1:5001"
 * RCV
 * gst-launch-1.0 -v udpsrc address=127.0.0.1 port=5000 ! application/x-rtp,encoding-name=JPEG,payload=26 ! rtpjpegdepay ! jpegdec ! autovideosink
 * gst-launch-1.0 -v udpsrc address=127.0.0.1 port=5001 ! application/x-rtp,encoding-name=JPEG,payload=26 ! rtpjpegdepay ! jpegdec ! autovideosink
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



std::vector<std::string> NetworkToolkit::host_ips()
{
    std::vector<std::string> ipstrings;

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
                    if ( std::string(ip).compare("127.0.0.1") == 0 )
                        ipstrings.push_back( "localhost" );
                    else
                        ipstrings.push_back( std::string(ip) );
                }
            }
            close(s);
        }
    }
    return ipstrings;
}
