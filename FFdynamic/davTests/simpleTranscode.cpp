#include <glog/logging.h>
#include "davStreamlet.h"
#include "davStreamletBuilder.h"

using namespace ff_dynamic;

int main(int argc, char **argv) {
    if (argc != 2) {
        std::cerr << "Usage: transcode inputUrl\n";
        return -1;
    }
    google::InitGoogleLogging("Transcoding Example");
    FLAGS_stderrthreshold = 0;
    FLAGS_logtostderr = 1;
    /* create demux, decode, encode, mux */
    DavWaveOption demuxOption((DavWaveClassDemux()));
    demuxOption.set(DavOptionInputUrl(), argv[1]);

    DavWaveOption videoDecodeOption((DavWaveClassVideoDecode()));
    DavWaveOption audioDecodeOption((DavWaveClassAudioDecode()));

    DavWaveOption videoEncodeOption((DavWaveClassVideoEncode()));
    videoEncodeOption.setVideoSize(1280, 720);
    videoEncodeOption.setAVRational("framerate", {30000, 1001});
    DavWaveOption audioEncodeOption((DavWaveClassAudioEncode()));

    DavWaveOption muxOption((DavWaveClassMux()));
    muxOption.set(DavOptionOutputUrl(), "test-transcode.flv");

    ////////////////////////////////////////////////////////////////////////////
    DavDefaultInputStreamletBuilder inputBuilder;
    DavDefaultOutputStreamletBuilder outputBuilder;
    DavStreamletOption inputOption;
    inputOption.setInt(DavOptionBufLimitNum(), 20);
    auto streamletInput = inputBuilder.build({demuxOption, videoDecodeOption, audioDecodeOption},
                                             DavDefaultInputStreamletTag("test_input"), inputOption);
    auto streamletOutput = outputBuilder.build({videoEncodeOption, audioEncodeOption, muxOption},
                                               DavDefaultOutputStreamletTag("output_output"));
    CHECK(streamletInput != nullptr && streamletOutput != nullptr);
    /* connect streamlets */
    streamletInput >> streamletOutput;

    DavRiver river({streamletInput, streamletOutput});
    river.start();
    /* wait and check errors */
    do {
        if (river.isStopped()) {
            break;
        } else {
            int ret = river.getErr();
            if (ret < 0 && ret != AVERROR_EOF && ret != AVERROR(EAGAIN)) {
                DavMessager err(ret, "find a process error, will stop");
                LOG(ERROR) << "process failed " << err;
                break;
            }
        }
    } while(true);
    river.stop();
    river.clear();
    return 0;
}
