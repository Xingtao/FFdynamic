#include <string>
#include "testCommon.h"
#include "cvDnnDetect.h"
#include "cvStreamlet.h"

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
    // 2. video/audio decode
    DavWaveOption videoDecodeOption((DavWaveClassVideoDecode()));
    DavWaveOption audioDecodeOption((DavWaveClassAudioDecode()));
    // 3. video encode
    DavWaveOption videoEncodeOption((DavWaveClassVideoEncode()));
    videoEncodeOption.setVideoSize(1280, 720);
    videoEncodeOption.set("preset", "veryfast");
    // 4. audio encode
    DavWaveOption audioEncodeOption((DavWaveClassAudioEncode()));
    // 5. mux
    DavWaveOption muxOption((DavWaveClassMux()));
    muxOption.set(DavOptionOutputUrl(), "dynaDetect.flv");

    // 6. dynamic dnn detection streamlet
    DavWaveOption cvDnnDetectOption((DavWaveClassCvDnnDetect()));
    cvDnnDetectOption.set(DavOptionImplType(), "auto");
    cvDnnDetectOption.set("detector_type", );
    cvDnnDetectOption.set("detector_framework_tag", );
    cvDnnDetectOption.set("model_path", );
    cvDnnDetectOption.set("config_path", );
    cvDnnDetectOption.set("classname_path", );
    cvDnnDetectOption.setInt("backend_id", );
    cvDnnDetectOption.setInt("target_id", );
    cvDnnDetectOption.setDouble("scale_factor", );
    cvDnnDetectOption.setBool("swap_rb", );
    cvDnnDetectOption.setInt("width", );
    cvDnnDetectOption.setInt("height", );
    cvDnnDetectOption.setDouble("conf_threshold", 0.7);
    cvDnnDetectOption.setDoubleArray("means", {});

    ////////////////////////////////////////////////////////////////////////////
    DavDefaultInputStreamletBuilder inputBuilder;
    DavDefaultOutputStreamletBuilder outputBuilder;
    CvDnnDetectStreamletBuilder cvDnnBuilder;
    DavStreamletOption inputOption;
    DavStreamletOption outputOption;
    DavStreamletOption cvDnnBuildOption;
    // start build
    inputOption.setInt(DavOptionBufLimitNum(), 25);
    auto streamletInput = inputBuilder.build({demuxOption, videoDecodeOption, audioDecodeOption},
                                             DavDefaultInputStreamletTag("input"), inputOption);
    CHECK(streamletInput != nullptr);
    auto streamletOutput = outputBuilder.build({videoEncodeOption, audioEncodeOption, muxOption},
                                               DavDefaultOutputStreamletTag("output"));
    CHECK(streamletOutput != nullptr);
    // TODO:
    auto cvDnnStreamlet = cvDnnBuilder.build({videoDehazeOption},
                                              DavSingleWaveStreamletTag("dehaze"), singleWaveOption);
    /* connect streamlets */
    streamletInput >> cvDnnStreamlet;
    cvDnnStreamlet >> streamletOutput;

    // start
    DavRiver river({streamletInput, dehazeStreamlet, streamletMix, streamletOutput});
    river.start();
    test_common::testRun(river);
    river.stop();
    river.clear();
    return 0;
}
