#pragma once

#include <iostream>
#include <string>
#include <memory>

#include "glog/logging.h"
extern "C" {
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libavutil/avutil.h"
#include "libavutil/pixdesc.h"
#include "libswresample/swresample.h"
#include "libavutil/error.h"
#include "libavutil/frame.h"
}

#include "fmtScale.h"

namespace  ff_dynamic {
using ::std::shared_ptr;
using ::std::string;

struct ImageToRawFrame {

/* NOTE: expect decoded format is AV_PIX_FMT_YUV420P */
static shared_ptr<AVFrame> loadImageToRawFrame(const string & imageUrl) {
    AVFormatContext *pFormatCtx = nullptr;
    AVCodecContext *pCodecCtx = nullptr;
    AVCodec *pCodec = nullptr;
    AVCodecParameters *codecpar = nullptr;
    auto frame = shared_ptr<AVFrame>(av_frame_alloc(), [](AVFrame *p) {av_frame_free(&p);});
    AVPacket pkt;
    av_init_packet(&pkt);

    int ret = avformat_open_input(&pFormatCtx, imageUrl.c_str(), nullptr, nullptr);
    if (ret < 0) {
        char buf[AV_ERROR_MAX_STRING_SIZE] = {0};
        av_strerror(ret, buf, AV_ERROR_MAX_STRING_SIZE);
        LOG(ERROR) << "[ImageToRawFrame] " << "open input " + imageUrl + " fail: " << buf;
        goto fail;
    }
    ret = avformat_find_stream_info(pFormatCtx, nullptr);
    if (ret < 0) {
        char buf[AV_ERROR_MAX_STRING_SIZE] = {0};
        av_strerror(ret, buf, AV_ERROR_MAX_STRING_SIZE);
        LOG(ERROR) << "[ImageToRawFrame] " << imageUrl + "find stream info fail: " << buf;
        goto fail;
    }

    for (unsigned int k=0; k < pFormatCtx->nb_streams; k++) {
        const AVStream *st = pFormatCtx->streams[k];
        if (st->codecpar->codec_type != AVMEDIA_TYPE_VIDEO)
            continue;
        /* expect only one video stream (image) */
        codecpar = st->codecpar;
    }
    /* Find the decoder for this image */
    pCodec = avcodec_find_decoder(codecpar->codec_id);
    if (!pCodec) {
        LOG(ERROR) << "[ImageToRawFrame] " << "find decoder for " + imageUrl + "fail: "
                   << (int)(codecpar->codec_id);
        goto fail;
    }

    pCodecCtx = avcodec_alloc_context3(pCodec);
    if (!pCodecCtx) {
        std::cerr << "open decode context of " + imageUrl + "fail";
        goto fail;
    }

    if (avcodec_parameters_to_context(pCodecCtx, codecpar) < 0) {
        LOG(ERROR) << "[ImageToRawFrame] copy codecpar to context fail";
        goto fail;
    }

    ret = avcodec_open2(pCodecCtx, pCodec, nullptr);
    if (ret < 0) {
        char buf[AV_ERROR_MAX_STRING_SIZE] = {0};
        av_strerror(ret, buf, AV_ERROR_MAX_STRING_SIZE);
        LOG(ERROR) << "[ImageToRawFrame]  avcodec_open2 fail " << buf;
        goto fail;
    }

    if (!frame) {
        LOG(ERROR) << "[ImageToRawFrame] cannot allocate avframe";
        goto fail;
    }

    do {
        ret = av_read_frame(pFormatCtx, &pkt);
        if (ret < 0 && ret != AVERROR_EOF)
            goto fail;
        else if (ret == AVERROR_EOF) {
            ret = avcodec_send_packet(pCodecCtx, nullptr);
        } else if (pFormatCtx->streams[pkt.stream_index]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            ret = avcodec_send_packet(pCodecCtx, &pkt);
        }

        if (ret < 0 && ret != AVERROR_EOF && ret != AVERROR(EAGAIN)) {
            av_packet_unref(&pkt);
            LOG(ERROR) << "[ImageToRawFrame] failed read / send data packet";
            goto fail;
        }

        ret = avcodec_receive_frame(pCodecCtx, frame.get());
        if (ret >= 0) { /* get the image */
            break;
        } else if (ret < 0 && ret != AVERROR(EAGAIN)) {
            av_packet_unref(&pkt);
            LOG(ERROR) << "[ImageToRawFrame] decode no data and eof or error, fail to get image";
            goto fail;
        }
    } while(true);

    avcodec_free_context(&pCodecCtx);
    avformat_close_input(&pFormatCtx);

    return frame;

fail:
    avcodec_free_context(&pCodecCtx);
    avformat_close_input(&pFormatCtx);
    return {};
}

};
} // namespace ff_dynamic
