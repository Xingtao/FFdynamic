#!/bin/sh

rm -rf build && mkdir -p build && cd build && cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=1 -DCMAKE_INSTALL_PREFIX=/usr/local/FFdynamic -DFFMPEG_CUSTOME_PATH=/home/kg/ffmpeg-git/build  -DCMAKE_BUILD_TYPE=Release ../ && make
