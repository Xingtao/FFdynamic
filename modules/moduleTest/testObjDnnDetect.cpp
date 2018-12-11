#include <string>
#include "testCommon.h"
#include "objDetect.h"
#include "cvPostDraw.h"
#include "objDetectStreamlet.h"

using namespace ff_dynamic;

int main(int argc, char **argv) {
    if (argc != 2) {
        std::cout << "usage: objDetect inputFile\n";
        return -1;
    }

    test_common::testInit(argv[0]);
    LOG(INFO) << "starting test: " << argv[1];
    // 1. create demux and get stream info
    DavWaveOption demuxOption((DavWaveClassDemux()));
    demuxOption.set(DavOptionInputUrl(), argv[1]);
    // 2. video decode (audio will use copy)
    DavWaveOption videoDecodeOption((DavWaveClassVideoDecode()));
    // 3. video encode
    DavWaveOption videoEncodeOption((DavWaveClassVideoEncode()));
    videoEncodeOption.setVideoSize(1280, 720);
    videoEncodeOption.set("preset", "veryfast");
    // 5. mux
    DavWaveOption muxOption((DavWaveClassMux()));
    muxOption.set(DavOptionOutputUrl(), "dynaDetect.flv");

    // 6. dynamic dnn detection streamlet
    DavWaveOption dataRelayOption((DavWaveClassDataRelay()));
    DavWaveOption cvPostDrawOption((DavWaveClassCvPostDraw()));

    // we craete two dnn detectors
    DavWaveOption objDetectOption1((DavWaveClassObjDetect()));
    objDetectOption1.set(DavOptionImplType(), "auto"); // if use darknet, change auto -> darknetDetect
    objDetectOption1.set("detector_type", "detect");
    objDetectOption1.set("detector_framework_tag", "darknet/yolov3");
    objDetectOption1.set("model_path", "/Users/pengxingtao/practice/psn/FFdynamic/modules/moduleTest/yolov3.weights");
    objDetectOption1.set("config_path", "/Users/pengxingtao/practice/psn/FFdynamic/modules/moduleTest/yolov3.cfg");
    objDetectOption1.set("classname_path", "../moduleTest/coco.names");
    objDetectOption1.setInt("backend_id", 0);
    objDetectOption1.setInt("target_id", 1);
    objDetectOption1.setDouble("scale_factor", 1.0/255.0);
    objDetectOption1.setBool("swap_rb", true);
    objDetectOption1.setInt("width", 416);
    objDetectOption1.setInt("height", 416);
    objDetectOption1.setDouble("conf_threshold", 0.7);
    objDetectOption1.setDoubleArray("means", {0,0,0});

    DavWaveOption objDetectOption2((DavWaveClassObjDetect()));
    objDetectOption2.set(DavOptionImplType(), "auto");
    objDetectOption2.set("detector_type", "detect");
    objDetectOption2.set("detector_framework_tag", "caffemodel/vgg_ssd_300");
    objDetectOption2.set("model_path", "../moduleTest/VGG_coco_SSD_300x300_iter_400000.caffemodel");
    objDetectOption2.set("config_path", "../moduleTest/vgg_ssd_300_deploy.prototxt");
    objDetectOption2.set("classname_path", "../moduleTest/coco.names");
    objDetectOption2.setInt("backend_id", 0);
    objDetectOption2.setInt("target_id", 1);
    objDetectOption2.setDouble("scale_factor", 1); //1.0/255.0
    objDetectOption2.setBool("swap_rb", false);
    objDetectOption2.setInt("width", 300);
    objDetectOption2.setInt("height", 300);
    objDetectOption2.setDouble("conf_threshold", 0.7);
    objDetectOption2.setDoubleArray("means", {0,0,0});

    ////////////////////////////////////////////////////////////////////////////
    DavDefaultInputStreamletBuilder inputBuilder;
    DavDefaultOutputStreamletBuilder outputBuilder;
    ObjDetectStreamletBuilder objDetectBuilder;
    DavStreamletOption inputOption;
    DavStreamletOption outputOption;
    DavStreamletOption objDetectBuildOption;
    // start build
    inputOption.setInt(DavOptionBufLimitNum(), 20);
    auto streamletInput = inputBuilder.build({demuxOption, videoDecodeOption},
                                             DavDefaultInputStreamletTag("input"), inputOption);
    CHECK(streamletInput != nullptr);
    objDetectBuildOption.setInt(DavOptionBufLimitNum(), 20);
    auto streamletOutput = outputBuilder.build({videoEncodeOption, muxOption},
                                               DavDefaultOutputStreamletTag("output"));
    CHECK(streamletOutput != nullptr); // , cvDnnDetectOption2
    auto objDetectStreamlet = objDetectBuilder.build({dataRelayOption, cvPostDrawOption, objDetectOption1,
                objDetectOption2}, ObjDetectStreamletTag("objDetect"), objDetectBuildOption);
    /* connect streamlets */
    /* video part */
    streamletInput >= objDetectStreamlet >= streamletOutput;
    /* audio bitstream TODO:  streamletInput * streamletOutput; */

    // start
    DavRiver river({streamletInput, objDetectStreamlet, streamletOutput});
    river.start();
    test_common::testRun(river);
    river.stop();
    river.clear();
    return 0;
}
