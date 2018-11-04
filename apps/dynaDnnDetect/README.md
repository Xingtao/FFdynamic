##  A play case - Change Object Detectors at run time with live streams
---------

This project provides a play groud one can change object detector types at run time while reading live streams (also local files). Those detectors are opencv supported one, yolo, ssd, face detector, etc.. or your own ones, which needs write a plugin wrapper. Here is an output stream gif, which run 2 detecors in parallle, draw boxes and texts when they locate objects intereted.

### Chnage detectors at run time

We can *add a detector* with a curl request while program is running:

``` shell
    Add a new detector:
    curl -X POST http://ip:port/api1/detectors/name_of_the_detector
    
    Delete an existing detector:
    curl -X DELETE http://ip:port/api1/detectors/name_of_the_detector
```

### Program Running pattern

```
VideoStreamDemux |-> AudioDecode -> |-> AudioEncode ------------------------------------> |
                 |                                                                        |
                 |                  |-> ObjDector 1 -> |    | Detector |                  |
                 |-> VideoDecode -> |-> ObjDector 2 -> | -> | Post     | -> VideoEncode ->| -> Mux stream out
                                    |-> ObjDector 3 -> |    | Draw     |                  |
                                        ..........
```

### Run the program

``` 
    under build folder:  ./dynaObjDetect dynaObjDetect.json inputStream outputStream
```

### Build the program

Depends on OpenCV (3.4.0 or above), glog, protobuf3.

#### *For mac*
Install FFmpeg as usal, then
``` shell
brew install cmake glog gflags protobuf boost 
```

#### *For Ubuntu*
Install FFmpeg as usal, then  
```
apt install -y cmake libgflags-dev libgoogle-glog-dev libboost-all-dev
```

#### *For CentOS*
Install FFmpeg as usal, then  
``` shell
yum install -y glog-devel gflags-devel cmake boost-devel
```

protobuf3 is not well supports by some linux distributions' package manager, here is a compile script (sudo required):
```
DIR=$(mktemp -d) && cd ${DIR} && \
git clone https://github.com/protocolbuffers/protobuf.git && cd protobuf && \
git submodule update --init --recursive && \
./autogen.sh && ./configure CXXFLAGS=-Wno-unused-variable && \
make && make check && \
sudo make install && sudo ldconfig
```
