#pragma once

#include <string>
#include <vector>
#include <memory>
#include <iostream>

#include <algorithm>
#include <cuda.h>
#include "NvDecoder.h"
#include "NvCodecUtils.h"

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/avutil.h"
#include "libavfilter/avfilter.h"
#include "libswscale/swscale.h"
#include "libswresample/swresample.h"
#include "libavutil/frame.h"
#include "libavutil/error.h"
#include "libavutil/opt.h"
#include "libavutil/samplefmt.h"
#include "libavutil/imgutils.h"
#include "libavutil/time.h"
}

#include "ffmpegHw.h"

namespace cuvid_reader {
using ::std::string;
using ::std::vector;
using ::std::shared_ptr;
using namespace ffmpeg_hwctx;

// Use this one to control NvDecoder interface or FFmpeg's cuvid interface
struct NvDecoderParams {
    bool m_bUseNvDecoderInterface = false;
    int m_deviceIdx = 0;
    bool m_bToBgr = true; // via Nv12ToBgrPlanar
    int m_targetWidth = -1;  // even number expected
    int m_targetHeight = -1;
};

class CuvidReader {
public:
    CuvidReader() = default;
    virtual ~CuvidReader() {close();}
    int init(const string & streamUrl, AVDictionary *demuxOptions = nullptr, AVDictionary *decodeOptions = nullptr);
    int getFrames(vector<shared_ptr<AVFrame>> & frames);

private:
    CuvidReader(const CuvidReader &) = delete;
    CuvidReader & operator= (const CuvidReader &) = delete;
    int close();
    int initDemux(const string & streamUrl, AVDictionary *options);
    int initDecode(AVDictionary *options);
    int getPacket(AVPacket *pkt);

private:
    AVFormatContext *m_fmtCtx = nullptr;
    AVCodecContext *m_decCtx = nullptr;
    AVCodecParameters *m_videoCodecpar = nullptr;
    string m_inputUrl;
    bool m_bFlush = false;

private:
    NvDecoder *m_nvDeocder = nullptr;
    NvDecoderParams m_ndp;
    CUdevice m_cuDevice = 0;
    CUcontext m_cuContext = nullptr;
};

} // namespace cuvid_reader
