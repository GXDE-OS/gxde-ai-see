#!/bin/sh
# Build the wlr-virtual-pointer test driver.
set -e
cd "$(dirname "$0")"
cc -O2 -Wall pointer.c virtual-pointer-protocol.c -o pointer $(pkg-config --cflags --libs wayland-client)
