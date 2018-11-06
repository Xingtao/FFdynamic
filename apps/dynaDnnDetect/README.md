##  A play case - Change Object Detectors at run time with video streams
---------

This little project is a playgroud one can change object detector types at run time while reading video streams. Those detectors are loaded via OpenCV api. Models of *darknet* yolo3, *caffe* vgg-ssd, and *tensorflow* mobilenet-ssd (all in coco dataset) are tested. Here is an output stream gif, which run 2 detecors in parallle, draw boxes and texts when they locate objects intereted.
![dynaDetect](../../asset/dynaDetect.gif)

### Chnage detectors at run time

You can test it by *add/delete/stop detector* with curl request while program is running.

``` shell
    Add a new detector, yolo3 with darknet's model
    curl -X POST http://ip:port/api1/detector/darknet_yolo3
    
    Delete an existing detector:
    curl -X DELETE http://ip:port/api1/detector/caffe_vgg_ssd_300
    
    Stop program:
    curl -X POST http://ip:port/api1/stop
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
    under build folder:  ./dynaObjDetect dynaObjDetect.json
```
The input and output video stream are specified in json configure file.   
Also customized detect models are added in configure file. The detector model name in configure file is used as part of add/delete url name. For instance, if one add a detect model named "check_tiger_only", then following request will enable it.
``` shell
    curl -X POST http://ip:port/api1/detector/check_tiger_only
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
