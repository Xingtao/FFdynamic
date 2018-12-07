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
#include "ffmpegHeaders.h"
#include "globalSignalHandle.h"
#include "davStreamletBuilder.h"
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
    if (argc != 3) {
        LOG(ERROR) << "Usage: mixTest url1 url2";
        return -1;
    }
    string inUrl1(argv[1]);
    string inUrl2(argv[2]);
    LOG(INFO) << "starting test: " << inUrl1 << " & " << inUrl2;

    // 1. create demux and get stream info
    DavWaveOption demuxOption1((DavWaveClassDemux()));
    demuxOption1.set(DavOptionInputUrl(), inUrl1);
    DavWaveOption demuxOption2((DavWaveClassDemux()));
    demuxOption2.set(DavOptionInputUrl(), inUrl2);

    // 2. video decode
    DavWaveOption videoDecodeOption1((DavWaveClassVideoDecode()));
    DavWaveOption videoDecodeOption2((DavWaveClassVideoDecode()));
    // 3. audio decode
    DavWaveOption audioDecodeOption1((DavWaveClassAudioDecode()));
    DavWaveOption audioDecodeOption2((DavWaveClassAudioDecode()));

    // 4. video encode
    DavWaveOption videoEncodeOption((DavWaveClassVideoEncode()));
    videoEncodeOption.setVideoSize(1920, 1080);
    videoEncodeOption.setAVRational("framerate", {30000, 1001});
    // 5. audio encode
    DavWaveOption audioEncodeOption((DavWaveClassAudioEncode()));
    // 6. mux1 & mux2
    DavWaveOption muxOption1((DavWaveClassMux()));
    muxOption1.set(DavOptionOutputUrl(), "mix.flv");
    DavWaveOption muxOption2((DavWaveClassMux()));
    muxOption2.set(DavOptionOutputUrl(), "mix.mp4");
    // 7. audio mix
    DavWaveOption audioMixOption((DavWaveClassAudioMix()));
    audioMixOption.setBool("b_mute_at_start", false);
    // audio mix has default settings: fltp, 44100, 2 channels
    // 8. video mix
    DavWaveOption videoMixOption((DavWaveClassVideoMix()));
    videoMixOption.setBool(DavOptionVideoMixStartAfterAllJoin(), true);
    videoMixOption.setAVRational("framerate", {30, 1});
    videoMixOption.setVideoSize(1280, 720);

    ////////////////////////////////////////////////////////////////////////////
    DavDefaultInputStreamletBuilder inputBuilder1;
    DavDefaultInputStreamletBuilder inputBuilder2;
    DavMixStreamletBuilder mixBuilder;
    DavDefaultOutputStreamletBuilder outputBuilder;
    DavStreamletOption input1Option;
    DavStreamletOption input2Option;
    DavStreamletOption mixOption;
    DavStreamletOption outputOption;
    input1Option.setInt(DavOptionBufLimitNum(), 30);
    input2Option.setInt(DavOptionBufLimitNum(), 30);
    auto streamletInput1 = inputBuilder1.build({demuxOption1, videoDecodeOption1, audioDecodeOption1},
                                               DavDefaultInputStreamletTag("input1"), input1Option);
    CHECK(streamletInput1 != nullptr);
    auto streamletInput2 = inputBuilder2.build({demuxOption2, videoDecodeOption2, audioDecodeOption2},
                                               DavDefaultInputStreamletTag("input2"), input2Option);
    CHECK(streamletInput2 != nullptr);
    mixOption.setInt(DavOptionBufLimitNum(), 25);
    auto streamletMix = mixBuilder.build({videoMixOption, audioMixOption},
                                         DavMixStreamletTag("mix"), mixOption);
    CHECK(streamletMix != nullptr);
    auto streamletOutput = outputBuilder.build({videoEncodeOption, audioEncodeOption, muxOption1, muxOption2},
                                               DavDefaultOutputStreamletTag("output1"));
    CHECK(streamletOutput != nullptr);

    /* connect streamlets */
    streamletInput1 >> streamletMix;
    streamletInput2 >> streamletMix;
    streamletMix >> streamletOutput;

    // start
    DavRiver river({streamletInput1, streamletInput2, streamletMix, streamletOutput});
    river.start();
    testRun(river);
    river.stop();
    river.clear();
    return 0;
}
