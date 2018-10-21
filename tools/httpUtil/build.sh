#!/bin/sh

rm -rf build && mkdir -p build && cd build && cmake -DCMAKE_INSTALL_PREFIX=./go .. && cmake --build . --target install
