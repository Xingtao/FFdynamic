#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <memory>
#include <iostream>
#include <glog/logging.h>

extern "C" {
#include "libavutil/audio_fifo.h"
#include "libavutil/opt.h"
#include "libavutil/avutil.h"
#include "libswresample/swresample.h"
#include "libavutil/error.h"
#include "libavutil/frame.h"
}

namespace ff_dynamic {
using::std::string;
using::std::vector;
using::std::shared_ptr;

struct AudioResampleParams {
    enum AVSampleFormat m_srcFmt;
    enum AVSampleFormat m_dstFmt;
    int m_srcSamplerate = 0;
    int m_dstSamplerate  = 0;
    uint64_t m_srcLayout = 0;
    uint64_t m_dstLayout = 0;
    string m_logtag = "audioResample";
};

extern std::ostream & operator<<(std::ostream & os, const AudioResampleParams & arp);

class AudioResample {
public:
    AudioResample() = default;
    virtual ~AudioResample() {closeResample();}
    int initResampler(const AudioResampleParams & arp);
    int sendResampleFrame(AVFrame *frame);
    shared_ptr<AVFrame> receiveResampledFrame(int desiredSize = 0);
    int receiveResampledFrame(vector<shared_ptr<AVFrame>> & frames, int desiredSize);

    inline int getFifoCurSize() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return av_audio_fifo_size(m_fifo);
    }
    inline double getFifoCurSizeInMs() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return av_audio_fifo_size(m_fifo) * 1000.0 / m_arp.m_dstSamplerate;
    }
    inline int64_t getStartPts() const {return m_startPts;}
    inline int64_t getCurPts() {std::lock_guard<std::mutex> lock(m_mutex); return m_curPts;}

private:
    int closeResample();
    int initResampledData();
    shared_ptr<AVFrame> allocOutFrame(const int nbSamples);
    shared_ptr<AVFrame> getOneFrame(const int desiredSize);

private:
    AudioResample(const AudioResample &) = delete;
    AudioResample & operator= (const AudioResample &) = delete;
    std::mutex m_mutex;
    struct SwrContext *m_swrCtx = nullptr;
    AudioResampleParams m_arp;
    bool m_bFifoOnly = false;
    bool m_bFlushed = false;
    AVAudioFifo *m_fifo = nullptr;
    int64_t m_startPts = AV_NOPTS_VALUE;
    int64_t m_curPts = AV_NOPTS_VALUE;

    uint8_t **m_resampledData = nullptr;
    int m_resampledDataSize = 8192;
    int m_srcChannels = 0;
    int m_dstChannels = 0;
    int64_t m_totalResampledNum = 0;
    string m_logtag;
};

} // namespace audio_resample
