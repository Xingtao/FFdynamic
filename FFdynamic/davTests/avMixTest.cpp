#include <unistd.h>

#include <string>
#include <cstdint>
#include <vector>
#include <memory>
#include <atomic>
#include <iostream>
#include <typeinfo>
#include <typeindex>

#include <glog/logging.h>
#include "davWave.h"
#include "davStreamlet.h"
#include "ffmpegHeaders.h"
#include "globalSignalHandle.h"
#include "davStreamlet.h"
#include "testCommon.h"

using std::string;
using std::vector;
using std::shared_ptr;
using std::type_index;
using namespace test_common;
using namespace ff_dynamic;

int main(int argc, char **argv) {
    testInit(argv[0]);

    string inUrl1 = "http://118.89.196.57/nba_ad.mp4";
    string inUrl2 = "http://118.89.196.57/sd.flv";;
    if (argc >= 2) {
        int theTestType = std::stoi(argv[1]);
        if (theTestType == 1) {
            inUrl1 = "rtmp://live.hkstv.hk.lxdns.com/live/hks"; // 480x288 sar,16:15
            inUrl2 = "rtmp://202.69.69.180:443/webcast/bshdlive-pc"; // 1027x576 sar,1:1
        } else if (theTestType == 2) {
            // inUrl2 = "rtmp://live.hkstv.hk.lxdns.com/live/hks"; // 480x288 sar,16:15
            inUrl2 = "rtmp://202.69.69.180:443/webcast/bshdlive-pc"; // 1027x576 sar,1:1
        }
    }

    LOG(INFO) << "starting test: " << inUrl1 << " & " << inUrl2;

    // 1. create demux and get stream info
    DavWaveOption demuxOption1((DavWaveClassDemux()));
    demuxOption1.set(DavOptionInputUrl(), inUrl1);
    DavWaveOption demuxOption2((DavWaveClassDemux()));
    demuxOption2.set(DavOptionInputUrl(), inUrl2);
    auto demux1 = std::make_shared<DavWave>(demuxOption1);
    if (demux1->hasErr()) {
        LOG(INFO) << demux1->getErr();
        return -1;
    }
    auto demux2 = std::make_shared<DavWave>(demuxOption2);
    if (demux2->hasErr()) {
        LOG(INFO) << demux2->getErr();
        return -1;
    }

    // 2. video decode
    DavWaveOption videoDecodeOption1((DavWaveClassVideoDecode()));
    auto videoDecode1 = std::make_shared<DavWave>(videoDecodeOption1);
    if (videoDecode1->hasErr()) {
        LOG(INFO) << videoDecode1->getErr();
        return -1;
    }
    DavWaveOption videoDecodeOption2((DavWaveClassVideoDecode()));
    auto videoDecode2 = std::make_shared<DavWave>(videoDecodeOption2);
    if (videoDecode2->hasErr()) {
        LOG(INFO) << videoDecode2->getErr();
        return -1;
    }

    // 3. audio decode
    DavWaveOption audioDecodeOption1((DavWaveClassAudioDecode()));
    auto audioDecode1 = std::make_shared<DavWave>(audioDecodeOption1);
    if (audioDecode1->hasErr()) {
        LOG(INFO) << audioDecode1->getErr();
        return -1;
    }
    DavWaveOption audioDecodeOption2((DavWaveClassAudioDecode()));
    auto audioDecode2 = std::make_shared<DavWave>(audioDecodeOption2);
    if (audioDecode2->hasErr()) {
        LOG(INFO) << audioDecode2->getErr();
        return -2;
    }

    // 4. video encode
    DavWaveOption videoEncodeOption((DavWaveClassVideoEncode()));
    videoEncodeOption.setVideoSize(1920, 1080);
    videoEncodeOption.setAVRational("framerate", {30000, 1001});
    auto videoEncode = std::make_shared<DavWave>(videoEncodeOption);
    if (videoEncode->hasErr()) {
        LOG(INFO) << videoEncode->getErr();
        return -1;
    }

    // 5. audio encode
    DavWaveOption audioEncodeOption((DavWaveClassAudioEncode()));
    // audioEncodeOption.set("ar", "22050");
    // audioEncodeOption.set("channel_layout", AV_CH_LAYOUT_STEREO);
    // audioEncodeOption.set(DavOptionCodecName, "aac"); // others just use default
    auto audioEncode = std::make_shared<DavWave>(audioEncodeOption);
    if (audioEncode->hasErr()) {
        LOG(INFO) << audioEncode->getErr();
        return -1;
    }

    // 6. mux1 & mux2
    DavWaveOption muxOption1((DavWaveClassMux()));
    muxOption1.set(DavOptionOutputUrl(), "mix1.flv");
    auto mux1 = std::make_shared<DavWave>(muxOption1);
    DavWaveOption muxOption2((DavWaveClassMux()));
    muxOption2.set(DavOptionOutputUrl(), "mix2.mp4");
    auto mux2 = std::make_shared<DavWave>(muxOption2);

    // other properties
    demux1->setMaxNumOfProcBuf(30);
    videoDecode1->setMaxNumOfProcBuf(25);
    demux2->setMaxNumOfProcBuf(30);
    videoDecode2->setMaxNumOfProcBuf(25);
    LOG(INFO) << "Create DavWaves Done. Set demux/decode with 20 buffers at most";

    // 7. audio mix
    DavWaveOption audioMixOption((DavWaveClassAudioMix()));
    // audio mix has default settings: fltp, 44100, 2 channels
    auto audioMix = std::make_shared<DavWave>(audioMixOption);

    // 8. video mix
    DavWaveOption videoMixOption((DavWaveClassVideoMix()));
    videoMixOption.setAVRational("framerate", {30, 1});
    videoMixOption.setVideoSize(1280, 720);
    auto videoMix = std::make_shared<DavWave>(videoMixOption);

    ////////////////////////////////////////////////////////////////////////////
    // gropu them and then connect
    // before we go further, set group for each
    auto streamletInput1 = std::make_shared<DavStreamlet>(DavDefaultInputStreamletTag(inUrl1));
    auto streamletInput2 = std::make_shared<DavStreamlet>(DavDefaultInputStreamletTag(inUrl2));
    // input group 1 & 2
    streamletInput1->setWaves({demux1, videoDecode1, audioDecode1});
    streamletInput2->setWaves({demux2, videoDecode2, audioDecode2});
    // mix group
    auto streamletMix = std::make_shared<DavStreamlet>(DavMixStreamletTag());
    streamletMix->setWaves({audioMix, videoMix});

    auto streamletOutput = std::make_shared<DavStreamlet>(DavDefaultOutputStreamletTag());
    streamletOutput->setWaves({audioEncode, videoEncode, mux1, mux2});

    auto demuxOutStreams1 = demux1->getOutputMediaMap();
    auto demuxOutStreams2 = demux2->getOutputMediaMap();
    if (demuxOutStreams1.size() <= 1 || demuxOutStreams2.size() <= 1) {
        LOG(ERROR) << "input stream should contain both audio and video streams";
        return -1;
    }

    bool bConnVideo = false;
    bool bConnAudio = false;
    for (auto & os : demuxOutStreams1) {
        if (os.second == AVMEDIA_TYPE_VIDEO && !bConnVideo) {
            DavWave::connect(demux1.get(), videoDecode1.get(), os.first);
            bConnVideo = true;
        }
        if (os.second == AVMEDIA_TYPE_AUDIO && !bConnAudio) {
             DavWave::connect(demux1.get(), audioDecode1.get(), os.first);
             bConnAudio = true;
        }
        if (bConnVideo && bConnAudio)
            break;
    }
    if (!bConnVideo || !bConnAudio) {
        LOG(ERROR) << "no audio/video stream found";
        return -1;
    }

    bConnVideo = false;
    bConnAudio = false;
    for (auto & os : demuxOutStreams2) {
        if (os.second == AVMEDIA_TYPE_VIDEO && !bConnVideo) {
            DavWave::connect(demux2.get(), videoDecode2.get(), os.first);
            bConnVideo = true;
        }
        if (os.second == AVMEDIA_TYPE_AUDIO && !bConnAudio) {
             DavWave::connect(demux2.get(), audioDecode2.get(), os.first);
             bConnAudio = true;
        }
        if (bConnVideo && bConnAudio)
            break;
    }
    if (!bConnVideo || !bConnAudio) {
        LOG(ERROR) << "no audio/video stream found";
        return -1;
    }
    // mix in connect
    DavWave::connect(videoDecode1.get(), videoMix.get());
    DavWave::connect(videoDecode2.get(), videoMix.get());
    DavWave::connect(audioDecode1.get(), audioMix.get());
    DavWave::connect(audioDecode2.get(), audioMix.get());

    // mix out connect
    DavWave::connect(videoMix.get(), videoEncode.get());
    DavWave::connect(audioMix.get(), audioEncode.get());

    // output connect
    DavWave::connect(videoEncode.get(), mux1.get());
    DavWave::connect(audioEncode.get(), mux1.get());
    DavWave::connect(videoEncode.get(), mux2.get());
    DavWave::connect(audioEncode.get(), mux2.get());

    // peer event subscribe: audio mix subscribe video mix
    DavWave::subscribe(videoMix.get(), audioMix.get());

    // start
    DavRiver river({streamletInput1, streamletInput2, streamletMix, streamletOutput});
    river.start();
    testRun(river);
    river.stop();
    river.clear();
    return 0;
}
