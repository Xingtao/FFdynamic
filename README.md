[![Build Status](https://www.travis-ci.org/Xingtao/FFdynamic.svg?branch=master)](https://travis-ci.org/Xingtao/FFdynamic)

### FFdynamic - Extend FFmpeg with run time control and audio/video process composition.

This project shipped with two parts: **FFdynamic** library and applications build on **FFdynamic**

------------
### Contents
- [An application *Interactive Live*](#an-application-interactive-live)
- [Another application *Dynamic Detect*](#another-application-dynamic-detect)
- [FFdynamic library Overview](#ffdynamic-library-overview)
- [Getting start with simple application Transcoding](#getting-start-with-simple-application-transcoding)
- [Write a plugin component](#write-a-plugin-component)
- [Installation](#installation)
- [Contribution](#contribution)

-----------
## An application *Interactive Live*

**Interactive Live** (Ial for short) is an application based on FFdynamic.  
Ial does multiple video and audio mixing, then streams it out. It could be run in phones or cloud servers.  
Here is an image got from an mobile app show its using scenario. Two streams are decoded, then mixed together and broadcast to audiences as one stream.

**Interactive Live** gives flexiable control over the mixing process (dynamical layout change, backgroup change, mute/unmute, etc..), shown in the Following gifs:

#### *Layout auto change or set to certain pattern during mixing broadcast by request*
This picture shows auto layout change when a new stream joined in (from 2 cells to 3 cells); then manually set the layout to 4 and 9 cells. Changes are quite smooth, without any frozen or stuck.

![Layout auto change or set as request](asset/layoutChange.gif)

#### For more details, please refer to [the application](apps/interactiveLive/README.md)

-----------
## Another application *Dynamic Detect*

This little project is a playgroud one can change object detector types at run time while reading video streams. Those detectors are loaded via OpenCV api. Models of *darknet* yolo3, *caffe* vgg-ssd, and *tensorflow* mobilenet-ssd (all in coco dataset) are tested. Here is an output stream gif, which run 2 detecors in parallle, draw boxes and texts when they locate objects intereted.

#### For more details, please refer to [the application](apps/dynaDnnDetect/README.md) and its according unit [test](modules/moduleTest/testCvDnnDetect.cpp)

-----------------

### `FFdynamic library Overview`

* **Extending**: FFdynamic extends FFmpeg in the manner of doing video/audio process **compositionally** and each component's state can be **dynamically** changed on the fly.

- **compositional**
  _FFdynamic_ is structured in a modular way. It takes each component (demux, decode, filter, encode, muxer, and cutomized component) as a building block and they can be combined together as desired at creationg time or run time.  
  For instance, if we are developing a dehaze algorithm and would like to know how good the dehazed algorithm visually (in compare to original one). FFdynamic provides facilities that allow one to easily realize following composition:

```
Demux |-> Audio Decode -> |-> Audio Encode ------------------------------------------> |
      |                                                                                | -> Muxer
      |                   |-> Dehaze Filter -> |                                       |
      |-> Video Decode -> |                    | Mix original and dehazed ->| Encode ->|
                          | -----------------> |
```
  As shown, after demux the input stream, we do video decode which will output to two components: 'Dehaze Filter' component and 'mix video' component; after dehaze, its image also output to 'mix video' component, in there we mix original and dehazed image into one. The whole example is [here](#write-a-plugin-component). 
  Normally, one can freely combine components as long as the input data can be processed.

* **on the fly**
  _FFdynamic_ has a runtime event dispatch module, which can pass request to the component needs dynamical state change. For instance, one could set dynamical 'Key Frame' request to video encoder or 'mute' one audio stream.  
  _FFdynamic_ also has a runtime components pub-sub module, which each component can subscribe interesed events from other components. For instance, one video encoder in a live show is willing to know the region of people faces in the incoming image', so that it could set more bitrate to this region. We can do this by subscribe events to a face detecting component and get published event with ROI.

- **customization**
   One can define their own components, for instance
   - a RTP demuxer with private fields
   - a object detection module
   - a packet sending control module
   Those components are plugins. Once they are done, they can be composed with other components. 

In short, *FFdynamic* is a scaffold allows develop complex audio/video application in a higher and flexiable manner.   
It is suitable for two kind of applications:
* real time audio/video process: live broadcast, video conference backend, transcoding, etc.. with run time control;
* develop new video/audio process algorithm which needs video clips as inputs and video clips as outputs, and communication or coorperation needed between video and audio streams;

-----------
## [Getting start with simple application Transcoding](#getting-start-with-simple-application-transcoding)
[see here](asset/transcode.md)

-----------
## [Write a plugin component](#write-a-plugin-component)

Here we introduce how to write a plugin. We develop a dehaze algorithm and make it as a FFdynamic's component. Then we could compose it with other components freely. the following image shows the diagram we mentioned in the 'Overview' part, mix original and dehazed image together to check the result visually.

![dehazed mix image](asset/dehaze.gif)

Refer to [here](examplePlugin/README.md) for plugin source files.

-----------
## `Installation`

### Dependency Required
* FFMpeg, glog, cmake (minimal version 3.2)
- compiler supports at least c++14 (GCC version 5 or above, Clang 3.4 or above, MSVC 19.0 or above)
* boost, protobuf3 (optional, only for the application 'Interactive Live')
- opencv (optional, if you would like to run plugin example)

protobuf3 is not well supports by some linux distributions' package manager, here is how to manually compile it(sudo required):
```
DIR=$(mktemp -d) && cd ${DIR} && \
git clone https://github.com/protocolbuffers/protobuf.git && cd protobuf && \
git submodule update --init --recursive && \
./autogen.sh && ./configure && \
make && make check && \
sudo make install && sudo ldconfig
```

### Build after install dependencies

``` sh
    Under FFdynamic folder: 
          'sh build.sh'  will build FFdynamic library (need sudo when install)
    Under app/interactiveLive folder: 
          'sh build.sh'  will build FFdynamic library and Ial program.
```

#### For Ubuntu / CentOS
Install FFmpeg as usal, then  

``` sh
apt install -y cmake libgflags-dev libgoogle-glog-dev libboost-all-dev
or 
yum install -y glog-devel gflags-devel cmake boost-devel  
```

#### For Mac
Install FFmpeg as usal, then  
brew install cmake glog gflags protobuf boost 

#### Others 
iOS and Android build is not implemented, pull request is welcome.

#### Optional Installation - TODO
* nvidia driver, cuda tookit, if you prefer using nvidia codec

### `A docker build`
To alleviate the build process, there is a [docker](tools/dockerlize/README.md) with all dependencies installed that you can play with.

-----------------
## `Contribution`

All contributions are welcome. Some TODOs:

- A webui that can easily operate on Interactive Live application;
* Nvidia codec supports (it is ongoing, will do it via ffmpeg);
- 'Interactive live' set video cell's border line, border color;
* Statistics for each component, process time, latency time, detailed info, etc..;
- An auto data format negotiate module between components;
