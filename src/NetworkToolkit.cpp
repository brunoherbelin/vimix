/*
 * This file is part of vimix - video live mixer
 *
 * **Copyright** (C) 2019-2022 Bruno Herbelin <bruno.herbelin@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
**/

#include <algorithm>

#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netdb.h>

#ifdef linux
#include <linux/netdevice.h>
#endif

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
 *       RTP UDP JPEG
 *
 * SND
 * gst-launch-1.0 videotestsrc is-live=true ! video/x-raw, format=RGB, framerate=30/1 ! videoconvert ! video/x-raw, format=I420 ! jpegenc quality=95 ! rtpjpegpay ! udpsink port=5000 host=127.0.0
 * RCV
 * gst-launch-1.0 udpsrc buffer-size=200000 port=5000 ! application/x-rtp,encoding-name=JPEG ! rtpjpegdepay ! queue max-size-buffers=10 ! jpegdec ! videoconvert ! video/x-raw, format=RGB ! autovideosink
 *
 * */

const char* NetworkToolkit::stream_protocol_label[NetworkToolkit::DEFAULT] = {
    "RAW Images",
    "JPEG Stream",
    "H264 Stream",
    "RGB Shared Memory"
};

const std::vector<std::string> NetworkToolkit::stream_send_pipeline {
    "video/x-raw, format=RGB,  framerate=30/1 ! queue max-size-buffers=10 ! rtpvrawpay ! application/x-rtp,sampling=RGB ! udpsink name=sink",
    "video/x-raw, format=NV12, framerate=30/1 ! queue max-size-buffers=10 ! jpegenc ! rtpjpegpay ! udpsink name=sink",
    "video/x-raw, format=NV12, framerate=30/1 ! queue max-size-buffers=10 ! x264enc tune=zerolatency pass=4 quantizer=22 speed-preset=2 ! rtph264pay aggregate-mode=1 ! udpsink name=sink",
    "video/x-raw, format=RGB,  framerate=30/1 ! queue max-size-buffers=10 ! shmsink buffer-time=100000 wait-for-connection=true name=sink"
};

const std::vector<std::string> NetworkToolkit::stream_receive_pipeline {
    "udpsrc port=XXXX caps=\"application/x-rtp,media=(string)video,encoding-name=(string)RAW,sampling=(string)RGB,width=(string)WWWW,height=(string)HHHH\" ! rtpvrawdepay ! queue max-size-buffers=10",
    "udpsrc port=XXXX caps=\"application/x-rtp,media=(string)video,encoding-name=(string)JPEG\" ! rtpjpegdepay ! decodebin",
    "udpsrc port=XXXX caps=\"application/x-rtp,media=(string)video,encoding-name=(string)H264\" ! rtph264depay ! decodebin",
    "shmsrc socket-path=XXXX ! video/x-raw, format=RGB, framerate=30/1 ! queue max-size-buffers=10",
};

const std::vector< std::pair<std::string, std::string> > NetworkToolkit::stream_h264_send_pipeline {
//    {"vtenc_h264_hw", "video/x-raw, format=I420, framerate=30/1 ! queue max-size-buffers=10 ! vtenc_h264_hw realtime=1 allow-frame-reordering=0 ! rtph264pay aggregate-mode=1 ! udpsink name=sink"},
    {"nvh264enc",     "video/x-raw, format=RGBA, framerate=30/1 ! queue max-size-buffers=10 ! nvh264enc rc-mode=cbr-ld-hq zerolatency=true ! video/x-h264, profile=(string)main ! rtph264pay aggregate-mode=1 ! udpsink name=sink"},
    {"vaapih264enc",  "video/x-raw, format=NV12, framerate=30/1 ! queue max-size-buffers=10 ! vaapih264enc rate-control=cqp init-qp=26 ! video/x-h264, profile=(string)main ! rtph264pay aggregate-mode=1 ! udpsink name=sink"}
};

bool initialized_ = false;
std::vector<std::string> ipstrings_;
std::vector<unsigned long> iplongs_;


void add_interface(int fd, const char *name) {
    struct ifreq ifreq;
    memset(&ifreq, 0, sizeof ifreq);
    strncpy(ifreq.ifr_name, name, IFNAMSIZ);
    if(ioctl(fd, SIOCGIFADDR, &ifreq)==0) {
        char host[128];
        switch(ifreq.ifr_addr.sa_family) {
            case AF_INET:
            case AF_INET6:
                getnameinfo(&ifreq.ifr_addr, sizeof ifreq.ifr_addr, host, sizeof host, 0, 0, NI_NUMERICHOST);
                break;
            default:
            case AF_UNSPEC:
                return; /* ignore */
        }
        // add only if not already listed
        std::string hostip(host);
        if ( std::find(ipstrings_.begin(), ipstrings_.end(), hostip) == ipstrings_.end() )
        {
            ipstrings_.push_back( hostip );
            iplongs_.push_back( GetHostByName(host) );
//            printf("%s %s %lu\n", name, host, GetHostByName(host));
        }
    }
}

void list_interfaces()
{
    char buf[16384];
    int fd=socket(PF_INET, SOCK_DGRAM, 0);
    if(fd > -1) {
        struct ifconf ifconf;
        ifconf.ifc_len=sizeof buf;
        ifconf.ifc_buf=buf;
        if(ioctl(fd, SIOCGIFCONF, &ifconf)==0) {
            struct ifreq *ifreq;
            ifreq=ifconf.ifc_req;
            for(int i=0;i<ifconf.ifc_len;) {
                size_t len;
#ifndef linux
                len=IFNAMSIZ + ifreq->ifr_addr.sa_len;
#else
                len=sizeof *ifreq;
#endif
                add_interface(fd, ifreq->ifr_name);
                ifreq=(struct ifreq*)((char*)ifreq+len);
                i+=len;
            }
        }
    }
    close(fd);
    initialized_ = true;
}

std::vector<std::string> NetworkToolkit::host_ips()
{
    if (!initialized_)
        list_interfaces();

    return ipstrings_;
}


bool NetworkToolkit::is_host_ip(const std::string &ip)
{
    if ( ip.compare("localhost") == 0)
        return true;

    if (!initialized_)
        list_interfaces();

    return std::find(ipstrings_.begin(), ipstrings_.end(), ip) != ipstrings_.end();
}

std::string NetworkToolkit::closest_host_ip(const std::string &ip)
{
    std::string address = "localhost";

    if (!initialized_)
        list_interfaces();

    // discard trivial case
    if ( ip.compare("localhost") != 0)
    {
        int index_mini = -1;
        unsigned long host = GetHostByName( ip.c_str() );
        unsigned long mini = host;

        for (size_t i=0; i < iplongs_.size(); i++){
            unsigned long diff = host > iplongs_[i] ? host-iplongs_[i] : iplongs_[i]-host;
            if (diff < mini) {
                mini = diff;
                index_mini = (int) i;
            }
        }

        if (index_mini>0)
            address = ipstrings_[index_mini];

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
