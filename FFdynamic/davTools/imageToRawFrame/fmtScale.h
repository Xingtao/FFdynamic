#pragma once

#include <memory>

#include "glog/logging.h"
extern "C" {
#include "libavformat/avformat.h"
#include "libavutil/avutil.h"
#include "libavutil/pixdesc.h"
#include "libswscale/swscale.h"
#include "libavutil/imgutils.h"
#include "libavutil/frame.h"
}

namespace ff_dynamic {

struct FmtScale {
    static std::shared_ptr<AVFrame> fmtScale(std::shared_ptr<AVFrame> inFrame,
                                             int dstW, int dstH, enum AVPixelFormat dstFmt) {
        return fmtScale(inFrame.get(), dstW, dstH, dstFmt);
    }
    static std::shared_ptr<AVFrame> fmtScale(AVFrame *inFrame,
                                             int dstW, int dstH, enum AVPixelFormat dstFmt) {
        auto outFrame = std::shared_ptr<AVFrame>(av_frame_alloc(), [](AVFrame *p) {av_frame_free(&p);});
        if (!outFrame) {
            LOG(ERROR) <<  "Could not allocate out frame";
            return {};
        }
        if (fmtScale(inFrame, outFrame.get(), dstW, dstH, dstFmt) < 0)
            return {};
        return outFrame;
    }

    static int fmtScale(AVFrame *inFrame, AVFrame *outFrame, int dstW, int dstH, enum AVPixelFormat dstFmt) {
        CHECK(inFrame != nullptr && outFrame != nullptr);
        outFrame->width = dstW;
        outFrame->height = dstH;
        outFrame->format = (int)dstFmt;
        int ret = av_frame_get_buffer(outFrame, 16);
        if (ret < 0) {
            LOG(ERROR) <<  "Could not allocate out frame";
            return ret;
        }
        SwsContext *swsCtx = sws_getContext(inFrame->width, inFrame->height,
                                            (enum AVPixelFormat)inFrame->format,
                                            dstW, dstH, dstFmt, SWS_BICUBIC, nullptr, nullptr, nullptr);
        if (!swsCtx) {
            LOG(ERROR) <<  "Could not initialize the conversion context";
            return AVERROR(EINVAL);
        }
        /* return height of the dst frame */
        ret = sws_scale(swsCtx, (const uint8_t * const *) inFrame->data,
                        inFrame->linesize, 0, inFrame->height, outFrame->data, outFrame->linesize);
        sws_freeContext(swsCtx);
        if (ret <= 0)
            return AVERROR(EINVAL);
        return 0;
    }
};

}
