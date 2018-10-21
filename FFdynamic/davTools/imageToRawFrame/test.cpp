#include <string>
#include <iostream>
#include <fstream>

#include "imageToRawFrame.h"

using namespace std;
using namespace ff_dynamic;

int main(int argc, char ** argv) {
    if (argc != 2) {
        cout << "usage: ./a.out imageUrl";
        return -1;
    }
    #if (LIBAVFORMAT_VERSION_MAJOR < 59)
    av_register_all();
    avformat_network_init();
    #endif

    google::InitGoogleLogging("imageToRawFrame");
    FLAGS_stderrthreshold = 0;
    FLAGS_logtostderr = 1;
    string imageUrl(argv[1]);
    auto frame = ImageToRawFrame::loadImageToRawFrame(imageUrl);
    if (!frame)
        return -1;
    if (frame->format != (int) AV_PIX_FMT_YUV420P) {
        LOG(INFO) << "it is not a yuv420p format, " << frame->format << " "
                   << av_get_pix_fmt_name((enum AVPixelFormat)frame->format);
        auto dstFrame = FmtScale::fmtScale(frame, 1280, 720, AV_PIX_FMT_YUV420P);
        if (!dstFrame) {
            LOG(ERROR) << "failed convert format to " << av_get_pix_fmt_name(AV_PIX_FMT_YUV420P);
            return -1;
        }
        frame.swap(dstFrame);
    }

    ofstream dumpFrame("testDumpFrame.yuv");
    for (int k=0; k < frame->height; k++)
        dumpFrame.write((const char *)frame->data[0] + k * frame->linesize[0], frame->width);
    for (int k=0; k < frame->height/2; k++)
        dumpFrame.write((const char *)frame->data[1] + k * frame->linesize[1], frame->width/2);
    for (int k=0; k < frame->height/2; k++)
        dumpFrame.write((const char *)frame->data[2] + k * frame->linesize[2], frame->width/2);
    dumpFrame.close();
    return 0;
}
