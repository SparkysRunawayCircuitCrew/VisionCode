#!/bin/bash

# IP Address to send to (default to 192.168.7.1 if not specified on command line)
declare DST="${1:-192.168.7.1}";
declare URL="udp://${DST}:123";
declare W=640
declare H=480

echo "Video Stream to ${URL}";
v4l2-ctl --set-fmt-video=width=${W},height=${H},pixelformat=1
./capture -o -c0 | avconv -i - -vcodec copy -f mjpeg ${URL};
