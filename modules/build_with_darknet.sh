mkdir -p build && cd build && cmake -D DARKNET=ON -D DARKNET_LIB=${HOME}/opensources/darknet/libdarknet.so -D DARKNET_INC_DIR=${HOME}/opensources/darknet/include ../ && make
