#pragma once

#include "opencv2/imgproc.hpp"
#include "ffmpegHeaders.h"

namespace ff_dynamic {

struct FrameMat {
    static int frameToMatYuv420(const AVFrame *inFrame, cv::Mat & outMat) {
        outMat.create(inFrame->height * 3 / 2, inFrame->width, CV_8UC1);
        for (int k=0; k < inFrame->height; k++)
            memcpy(outMat.data + k * inFrame->width, inFrame->data[0] + k * inFrame->linesize[0], inFrame->width);
        const auto u = outMat.data + inFrame->width * inFrame->height;
        const auto v = outMat.data + inFrame->width * inFrame->height * 5 / 4 ;
        for (int k=0; k < inFrame->height/2; k++) {
            memcpy(u + k * inFrame->width/2, inFrame->data[1] + k * inFrame->linesize[1], inFrame->width/2);
            memcpy(v + k * inFrame->width/2, inFrame->data[2] + k * inFrame->linesize[2], inFrame->width/2);
        }
        return 0;
    }

    static int matToFrameYuv420(const cv::Mat & inMat, AVFrame *outFrame) {
        outFrame->width = inMat.cols;
        outFrame->height = inMat.rows;
        outFrame->format = AV_PIX_FMT_YUV420P;
        av_frame_get_buffer(outFrame, 16);
        const auto y = inMat.data;
        const auto u = y + outFrame->width * outFrame->height;
        const auto v = u + outFrame->width * outFrame->height / 4 ;
        for (int k=0; k < outFrame->height; k++)
            memcpy(outFrame->data[0] + k * outFrame->linesize[0], y + k * outFrame->width, outFrame->width);
        for (int k=0; k < outFrame->height/2; k++) {
            memcpy(outFrame->data[1] + k * outFrame->linesize[1], u + k * outFrame->width/2, outFrame->width/2);
            memcpy(outFrame->data[2] + k * outFrame->linesize[2], v + k * outFrame->width/2, outFrame->width/2);
        }
        return 0;
    }
};

} // namespace ff_dynamic
