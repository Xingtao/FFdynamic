#include <string>
#include "cliCommon.h"
#include "cvDnnDetect.h"
#include "cvStreamlet.h"

using namespace ff_dynamic;

int main(int argc, char **argv) {
    if (argc != 2) {
        std::cout << "usage: cliDehazor inputFile\n";
        return -1;
    }

    cli_common::cliInit(argv[0]);
    LOG(INFO) << "starting cli: " << argv[1];
    // 1. create demux and get stream info
    DavWaveOption demuxOption((DavWaveClassDemux()));
    demuxOption.set(DavOptionInputUrl(), argv[1]);
    // 2. video decode
    DavWaveOption videoDecodeOption((DavWaveClassVideoDecode()));
    DavWaveOption audioDecodeOption((DavWaveClassAudioDecode()));
    // 4. video encode
    DavWaveOption videoEncodeOption((DavWaveClassVideoEncode()));
    videoEncodeOption.setVideoSize(1280, 720);
    videoEncodeOption.set("preset", "veryfast");
    // 5. mux
    DavWaveOption muxOption((DavWaveClassMux()));
    muxOption.set(DavOptionOutputUrl(), "dynaDetect.flv");

    // 7. dynamic dnn detection streamlet

    ////////////////////////////////////////////////////////////////////////////
    DavDefaultInputStreamletBuilder inputBuilder;
    DavDefaultOutputStreamletBuilder outputBuilder;
    DavSingleWaveStreamletBuilder singleWaveBuilder;
    CvDnnDetectStreamletBuilder cvDnnBuilder;
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

    singleWaveOption.setCategory(DavOptionInputDataTypeCategory(), DavDataInVideoRaw());
    singleWaveOption.setCategory(DavOptionOutputDataTypeCategory(), DavDataOutVideoRaw());
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
    cli_common::cliRun(river);
    river.stop();
    river.clear();
    return 0;
}
