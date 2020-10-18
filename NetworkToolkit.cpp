
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>

#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/netdevice.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>

#include "NetworkToolkit.h"


const char* NetworkToolkit::protocol_name[NetworkToolkit::DEFAULT] = {
    "TCP Broadcast JPEG",
    "TCP Broadcast H264",
    "Shared Memory"
};

const std::vector<std::string> NetworkToolkit::protocol_broadcast_pipeline {

    "video/x-raw, format=I420 ! jpegenc ! rtpjpegpay ! rtpstreampay ! tcpserversink name=sink",
    "video/x-raw, format=I420 ! x264enc pass=4 quantizer=26 speed-preset=3 threads=4 ! rtph264pay ! rtpstreampay ! tcpserversink name=sink",
    "video/x-raw, format=I420 ! jpegenc ! shmsink name=sink"
};


const std::vector<std::string> NetworkToolkit::protocol_receive_pipeline {

    "application/x-rtp-stream,media=video,encoding-name=JPEG,payload=26 ! rtpstreamdepay ! rtpjitterbuffer ! rtpjpegdepay ! jpegdec",
    "application/x-rtp-stream,media=video,encoding-name=H264,payload=96,clock-rate=90000 ! rtpstreamdepay ! rtpjitterbuffer ! rtph264depay ! avdec_h264",
    "jpegdec"
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

// localhost127.0.0.1, 192.168.0.30, 10.164.239.1,



//    char hostbuffer[256];
//    // retrieve hostname
//    if ( gethostname(hostbuffer, sizeof(hostbuffer)) != -1 )
//    {
//        // retrieve host information
//        struct hostent *host_entry;
//        host_entry = gethostbyname(hostbuffer);
//        if ( host_entry != NULL ) {
//            // convert an Internet network
//            // address into ASCII string
//            char *IPbuffer = inet_ntoa(*((struct in_addr*) host_entry->h_addr_list[0]));
//            ipstring = IPbuffer;
//        }
//    }

    return ipstrings;
}
