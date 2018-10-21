#include <string>
#include "testCommon.h"
#include "ffdynaDehazor.h"
#include "davStreamletBuilder.h"

using namespace ff_dynamic;

int main(int argc, char **argv) {
    if (argc != 2) {
        std::cout << "usage: testDehazor inputFile\n";
        return -1;
    }
    test_common::testInit(argv[0]);
    LOG(INFO) << "starting test: " << argv[1];

    // 1. create demux and get stream info
    DavWaveOption demuxOption((DavWaveClassDemux()));
    demuxOption.set(DavOptionInputUrl(), argv[1]);
    // 2. video decode
    DavWaveOption videoDecodeOption((DavWaveClassVideoDecode()));
    // 3. audio decode/encode/mix not needed
    // 4. video encode
    DavWaveOption videoEncodeOption((DavWaveClassVideoEncode()));
    videoEncodeOption.setVideoSize(1280, 720);
    videoEncodeOption.set("preset", "veryfast");
    // 5. mux
    DavWaveOption muxOption((DavWaveClassMux()));
    muxOption.set(DavOptionOutputUrl(), "dehaze.flv");
    // 6. video mix
    DavWaveOption videoMixOption((DavWaveClassVideoMix()));
    videoMixOption.setAVRational("framerate", {25, 1});
    videoMixOption.setVideoSize(1280, 720);
    videoMixOption.setBool("b_regenerate_pts", false, 0);
    videoMixOption.set("backgroud_image_path", "../ffdynamic-bg1.jpg");

    // 7. video dehaze: this is the one we jsut defined
    DavWaveOption videoDehazeOption((DavWaveClassDehaze()));
    videoDehazeOption.setDouble(DavOptionDehazeFogFactor(), 0.94);
    /* For options passing, beside using DavOption (which needs defined derived clsss from DavOption),
       we could also use AVDictionary (but this is not type checked, be aware of this):
       videoDehazeOption.setDouble("FogFactor", 0.94);
       // then in dehaze component, we could:
       double fogFactor;
       options.getDouble("FogFactor", fogFactor);
    */

    ////////////////////////////////////////////////////////////////////////////
    DavDefaultInputStreamletBuilder inputBuilder;
    DavMixStreamletBuilder mixBuilder;
    DavDefaultOutputStreamletBuilder outputBuilder;
    DavSingleWaveStreamletBuilder singleWaveBuilder;
    DavStreamletOption inputOption;
    DavStreamletOption mixOption;
    DavStreamletOption outputOption;
    DavStreamletOption singleWaveOption;
    // start build
    inputOption.setInt(DavOptionBufLimitNum(), 25);
    auto streamletInput = inputBuilder.build({demuxOption, videoDecodeOption},
                                             DavDefaultInputStreamletTag("input"), inputOption);
    CHECK(streamletInput != nullptr);
    auto streamletMix = mixBuilder.build({videoMixOption}, DavMixStreamletTag("mix"), mixOption);
    CHECK(streamletMix != nullptr);
    auto streamletOutput = outputBuilder.build({videoEncodeOption, muxOption},
                                               DavDefaultOutputStreamletTag("output"));
    CHECK(streamletOutput != nullptr);

    singleWaveOption.setCategory(DavOptionInputDataTypeCategory(), DavDataVideoRaw());
    singleWaveOption.setCategory(DavOptionOutputDataTypeCategory(), DavDataVideoRaw());
    auto dehazeStreamlet = singleWaveBuilder.build({videoDehazeOption},
                                                   DavSingleWaveStreamletTag("dehaze"), singleWaveOption);
    /* connect streamlets */
    streamletInput >> streamletMix;
    streamletInput >> dehazeStreamlet;
    dehazeStreamlet >> streamletMix;
    streamletMix >> streamletOutput;

    // start
    DavRiver river({streamletInput, dehazeStreamlet, streamletMix, streamletOutput});
    river.start();
    test_common::testRun(river);
    river.stop();
    river.clear();
    return 0;
}
