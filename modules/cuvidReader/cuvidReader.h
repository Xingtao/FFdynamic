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
#include "libavutil/frame.h"
}

namespace cuvid_reader {
using ::std::string;
using ::std::vector;
using ::std::shared_ptr;

struct NvDecoderParams {
    int m_deviceIdx = 0;
    int m_targetWidth = -1;  // even number expected
    int m_targetHeight = -1;
    bool m_bToBgr = true; // via Nv12ToBgrPlanar
    bool m_bWithDemux = true;
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
    /* */
    NvDecoderParams m_ndp;
    bool m_bFlush = false;
    /* nvdecoder */
    NvDecoder *m_nvDeocder = nullptr;
    CUdevice m_cuDevice = 0;
    CUcontext m_cuContext = nullptr;
    /* demuxer inside */
    AVFormatContext *m_fmtCtx = nullptr;
    AVCodecContext *m_decCtx = nullptr;
    AVCodecParameters *m_videoCodecpar = nullptr;
    string m_inputUrl; /* could be null, if uses separate demuxer */
};

} // namespace cuvid_reader
