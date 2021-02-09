#include <glog/logging.h>
#include <unistd.h>

#include <atomic>
#include <iostream>
#include <memory>
#include <string>
#include <typeindex>
#include <typeinfo>
#include <vector>

#include "davStreamlet.h"
#include "davWave.h"
#include "ffmpegHeaders.h"
#include "globalSignalHandle.h"
#include "testCommon.h"

using std::string;
using std::type_index;
using std::unique_ptr;
using std::vector;
using namespace test_common;
using namespace ff_dynamic;
using namespace global_sighandle;

int main(int argc, char **argv) {
    testInit(argv[0]);

    string inUrl = "rtmp://live.hkstv.hk.lxdns.com/live/hks";
    if (argc >= 2) inUrl = argv[1];

    // 1. create demux and get stream info
    DavWaveOption demuxOption((DavWaveClassDemux()));
    demuxOption.set(DavOptionInputUrl(), inUrl);
    auto demux = std::make_shared<DavWave>(demuxOption);
    if (demux->hasErr()) {
        LOG(ERROR) << demux->getErr();
        return -1;
    }

    // 2. video decode
    DavWaveOption videoDecodeOption((DavWaveClassVideoDecode()));
    auto videoDecode = std::make_shared<DavWave>(videoDecodeOption);
    if (videoDecode->hasErr()) {
        LOG(ERROR) << videoDecode->getErr();
        return -1;
    }

    // 3. audio decode
    DavWaveOption audioDecodeOption((DavWaveClassAudioDecode()));
    auto audioDecode = std::make_shared<DavWave>(audioDecodeOption);
    if (audioDecode->hasErr()) {
        LOG(ERROR) << audioDecode->getErr();
        return -1;
    }

    // 4. video filter
    string filterChainStr{
        "crop=in_w/2:in_h/2:(in_w-out_w)/2+((in_w-out_w)/2)*sin(t*10):(in_h-out_h)/2 "
        "+((in_h-out_h)/2)*sin(t*13)"};
    DavWaveOption videoFilterOption((DavWaveClassVideoFilter()));
    videoFilterOption.set(DavOptionFilterDesc(), filterChainStr);
    shared_ptr<DavWave> videoFilter(new DavWave(videoFilterOption));
    if (videoFilter->hasErr()) {
        LOG(INFO) << videoFilter->getErr();
        return -1;
    }

    //// 5. audio filter
    // DavWaveOption audioFilterOption((DavWaveClassAudioFilter()));
    // audioFilterOption.set(DavOptionFilterDesc, "null");
    // shared_ptr<DavWave> audioFilter(new DavWave(audioFilterOption));
    // if (audioFilter->hasErr()) {
    //    LOG(INFO) << audioFilter->getErr();
    //    return -1;
    //}

    // 6. video encode
    DavWaveOption videoEncodeOption((DavWaveClassVideoEncode()));
    videoEncodeOption.set(DavOptionCodecName(), "h264");
    videoEncodeOption.setVideoSize(640, 480);
    videoEncodeOption.setAVRational("framerate", {30000, 1001});
    auto videoEncode = std::make_shared<DavWave>(videoEncodeOption);
    if (videoEncode->hasErr()) {
        LOG(ERROR) << videoEncode->getErr();
        return -1;
    }

    // 7. audio encode
    DavWaveOption audioEncodeOption((DavWaveClassAudioEncode()));
    audioEncodeOption.set("ar", "22050");
    audioEncodeOption.set(DavOptionCodecName(), "aac");  // others just use default
    auto audioEncode = std::make_shared<DavWave>(audioEncodeOption);
    if (audioEncode->hasErr()) {
        LOG(INFO) << audioEncode->getErr();
        return -1;
    }

    DavWaveOption muxOption((DavWaveClassMux()));
    muxOption.set(DavOptionOutputUrl(), "test.flv");
    shared_ptr<DavWave> mux(new DavWave(muxOption));

    DavWaveOption mux2Option((DavWaveClassMux()));
    mux2Option.set(DavOptionOutputUrl(), "test.ts");
    shared_ptr<DavWave> mux2(new DavWave(mux2Option));

    // other properties
    demux->setMaxNumOfProcBuf(32);
    videoDecode->setMaxNumOfProcBuf(16);
    LOG(INFO) << "Create DavWaves Done. Set demux with 16 buffers at most";

    // connect
    auto demuxOutStreams = demux->getOutputMediaMap();
    if (demuxOutStreams.size() == 0) {
        LOG(ERROR) << "demux get nothing from input stream";
        return -1;
    }

    // connect the first video stream to video decode
    bool bConnVideo = false;
    bool bConnAudio = false;
    for (auto &os : demuxOutStreams) {
        if (os.second == AVMEDIA_TYPE_VIDEO && !bConnVideo) {
            DavWave::connect(demux.get(), videoDecode.get(), os.first);
            bConnVideo = true;
        }
        if (os.second == AVMEDIA_TYPE_AUDIO && !bConnAudio) {
            DavWave::connect(demux.get(), audioDecode.get(), os.first);
            // DavWave::connect(demux.get(), mux.get(), AVMEDIA_TYPE_AUDIO, os.first);
            bConnAudio = true;
        }
        if (bConnVideo && bConnAudio) break;
    }
    std::cout << "connect demux  " << std::endl;

    if (!bConnVideo) {
        LOG(ERROR) << "no video stream found";
        return -1;
    }

    if (!bConnAudio) LOG(WARNING) << " no audio stream found, will process video only";

    DavWave::subscribeData(videoDecode, videoFilter);
    DavWave::subscribeData(videoDecode, videoEncode);
    DavWave::subscribeData(videoEncode, mux);
    DavWave::subscribeData(videoEncode, mux2);
    if (bConnAudio) {
        // DavWave::subscribeData(audioDecode.get(), audioFilter.get(),
        // AVMEDIA_TYPE_AUDIO); DavWave::subscribeData(audioFilter.get(),
        // audioEncode.get(), AVMEDIA_TYPE_AUDIO);
        DavWave::subscribeData(audioDecode, audioEncode);
        DavWave::subscribeData(audioEncode, mux);
        DavWave::subscribeData(audioEncode, mux2);
    }

    // start
    DavStreamlet streamlet;
    /*videoFilter, audioFilter,  */
    streamlet.setWaves(vector<shared_ptr<DavWave>>(
        {demux, videoDecode, videoEncode, audioDecode, audioEncode, mux, mux2}));

    streamlet.start();
    testRun(streamlet);
    streamlet.stop();
    return 0;
}
