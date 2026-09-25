#!/bin/sh
if [ -z "$1" ]
    then
        echo "Usage: "
        echo $0 "<IP address> [PORT]"
        exit 0
fi

if [ -z "$2" ]
then
    PORT=9000
else
    PORT=$2
fi

## 1. request to vimix
## Requires liblo-tools to be installed (Linux: apt install liblo-tools, OSX: brew install liblo)
oscsend $1 7000 /vimix/peertopeer/request i "$PORT"

## 2. start gstreamer client
## Requires gstreamer to be installed (Linux: apt install gstreamer1.0-tools, OSX: brew install gstreamer)
gst-launch-1.0 udpsrc port="$PORT" caps="application/x-rtp,media=(string)video,encoding-name=(string)JPEG" ! rtpjpegdepay ! queue ! decodebin ! videoconvert ! autovideosink sync=false

## if vimix is configured for H246 peer to peer, use this command instead:
## gst-launch-1.0 udpsrc port=9000 caps="application/x-rtp,media=(string)video,clock-rate=(int)90000,encoding-name=(string)H264" ! queue ! rtph264depay ! h264parse ! decodebin ! videoconvert ! autovideosink sync=false

## 3. disconnect from vimix
oscsend $1 7000 /vimix/peertopeer/disconnect i "$PORT"

