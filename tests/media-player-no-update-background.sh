#!/bin/sh -ex

test "x$1" != "x" && DELAY=$1 || DELAY=1

for stream in 239.0.21.1 239.0.22.1; do
	remote-control-client media-player-play udp://$stream:4444
	sleep $DELAY
	remote-control-client media-player-output 0 0 824 618
	sleep $DELAY
	remote-control-client media-player-output 630 35 340 255
	sleep $DELAY
	remote-control-client media-player-output 630 35 340 255
	sleep $DELAY
	remote-control-client media-player-stop
	sleep $DELAY
done
