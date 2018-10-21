#include "audioResample.h"

namespace ff_dynamic {

std::ostream & operator<<(std::ostream & os, const AudioResampleParams & arp) {
    const char *srcFmtStr = av_get_sample_fmt_name(arp.m_srcFmt);
    const char *dstFmtStr = av_get_sample_fmt_name(arp.m_dstFmt);
    int srcChannels = av_get_channel_layout_nb_channels(arp.m_srcLayout);
    int dstChannels = av_get_channel_layout_nb_channels(arp.m_dstLayout);
    os << "AudioResampleParams: \n"
       << "SRC: layout " << arp.m_srcLayout << ", channels "<< srcChannels
       << ", samplerate " << arp.m_srcSamplerate << ", fmt " << srcFmtStr
       << "\nDST: layout " << arp.m_dstLayout << ", channels " << dstChannels
       << ", samplerate " << arp.m_dstSamplerate << ", fmt " << dstFmtStr;
    return os;
}

int AudioResample::initResampler(const AudioResampleParams & arp) {
    int ret = 0;
    m_arp = arp;
    m_logtag = m_arp.m_logtag;
    /* fifo of output sample format */
    m_srcChannels = av_get_channel_layout_nb_channels(m_arp.m_srcLayout);
    m_dstChannels = av_get_channel_layout_nb_channels(m_arp.m_dstLayout);
    m_fifo = av_audio_fifo_alloc(m_arp.m_dstFmt, m_dstChannels, 1);
    CHECK(m_fifo != nullptr);

    if (m_arp.m_srcFmt == m_arp.m_dstFmt &&
        m_arp.m_srcSamplerate == m_arp.m_dstSamplerate &&
        m_arp.m_srcLayout == m_arp.m_dstLayout) {
        LOG(INFO) << m_logtag << "no resample needed, just use audio fifo: " << m_arp;
        m_bFifoOnly = true;
        LOG(INFO) << m_logtag << "Fifo Only mode, just write/read to fifo, no actual resample";
        return 0;
    }

    m_swrCtx = swr_alloc();
    CHECK(m_swrCtx != nullptr);
    /* set options */
    av_opt_set_sample_fmt(m_swrCtx, "in_sample_fmt",      m_arp.m_srcFmt, 0);
    av_opt_set_int(m_swrCtx,        "in_channel_layout",  m_arp.m_srcLayout, 0);
    av_opt_set_int(m_swrCtx,        "in_sample_rate",     m_arp.m_srcSamplerate, 0);
    av_opt_set_sample_fmt(m_swrCtx, "out_sample_fmt",     m_arp.m_dstFmt, 0);
    av_opt_set_int(m_swrCtx,        "out_channel_layout", m_arp.m_dstLayout, 0);
    av_opt_set_int(m_swrCtx,        "out_sample_rate",    m_arp.m_dstSamplerate, 0);
    /* initialize the resampling context */
    ret = swr_init(m_swrCtx);
    if (ret < 0) {
        LOG(ERROR) << m_logtag << " failed to initialize the resampling context. " << m_arp;
        return ret;
    }
    if (initResampledData() < 0)
        return AVERROR(ENOMEM);

    LOG(INFO) << m_logtag << " init done " << m_arp;
    return 0;
}

int AudioResample::closeResample() {
    if (m_swrCtx)
        swr_free(&m_swrCtx);
    if (m_fifo) {
        av_audio_fifo_free(m_fifo);
        m_fifo = nullptr;
    }
    if (m_resampledData)
        av_freep(&m_resampledData[0]);
    av_freep(&m_resampledData);
    return 0;
}

int AudioResample::sendResampleFrame(AVFrame *frame) {
    std::lock_guard<std::mutex> lock(m_mutex);
    int srcNbSamples = 0;
    uint8_t **srcData = nullptr;
    if (frame) {
        srcNbSamples = frame->nb_samples;
        srcData = frame->extended_data;
        if (m_startPts == AV_NOPTS_VALUE && frame->pts != AV_NOPTS_VALUE) {
            m_startPts = frame->pts;
            m_curPts = frame->pts;
        }
    } else {
        m_bFlushed = true;
    }

    if (m_bFifoOnly) {
        return srcData ? av_audio_fifo_write(m_fifo, (void **)srcData, srcNbSamples) : 0;
    }

    const int dstNbSamples = av_rescale_rnd(swr_get_delay(m_swrCtx, m_arp.m_srcSamplerate) + srcNbSamples,
                                            m_arp.m_srcSamplerate, m_arp.m_dstSamplerate, AV_ROUND_UP);
    if (dstNbSamples > m_resampledDataSize) {
        //m_resampledData
        m_resampledDataSize = dstNbSamples;
        if (initResampledData() < 0)
            return AVERROR(ENOMEM);
    }
    int nbSamples = swr_convert(m_swrCtx, m_resampledData, dstNbSamples, (const uint8_t **)srcData, srcNbSamples);
    return av_audio_fifo_write(m_fifo, (void **)m_resampledData, nbSamples);
}

shared_ptr<AVFrame> AudioResample::receiveResampledFrame(int desiredSize) {
    std::lock_guard<std::mutex> lock(m_mutex);
    desiredSize = desiredSize == 0 ? av_audio_fifo_size(m_fifo) : desiredSize;
    if (av_audio_fifo_size(m_fifo) < desiredSize || desiredSize == 0)
        return {};
    /* this call cannot identify the right time of flush, the caller should keep this state */
    return getOneFrame(desiredSize);
}

int AudioResample::receiveResampledFrame(vector<shared_ptr<AVFrame>> & frames, int desiredSize) {
    std::lock_guard<std::mutex> lock(m_mutex);
    int ret = 0;
    desiredSize = desiredSize == 0 ? av_audio_fifo_size(m_fifo) : desiredSize;
    do {
        if (av_audio_fifo_size(m_fifo) < desiredSize || desiredSize == 0)
            break;
        auto frame = getOneFrame(desiredSize);
        if (frame) {
            frames.push_back(frame);
        } else {
            ret = AVERROR(ENOMEM);
            break;
        }
    } while(true);

    if (m_bFlushed) {
        ret = AVERROR_EOF;
        frames.push_back(shared_ptr<AVFrame>()); // insert an empty frame
    }
    return ret;
}

///////////////////////////////
// [ helpers]
shared_ptr<AVFrame> AudioResample::getOneFrame(const int desiredSize) {
    auto frame = allocOutFrame(desiredSize);
    if (frame) {
        av_audio_fifo_read(m_fifo, (void **)frame->data, desiredSize);
        frame->pts = m_curPts;
        m_curPts += desiredSize;
        m_totalResampledNum += desiredSize;
    }
    return frame;
}

int AudioResample::initResampledData() {
    if (m_resampledData)
        av_freep(&m_resampledData[0]);
    av_freep(&m_resampledData);
    int linesize = 0;
    int ret=  av_samples_alloc_array_and_samples(&m_resampledData, &linesize, m_dstChannels,
                                                 m_resampledDataSize, m_arp.m_dstFmt, 0);
    if (ret < 0)
        LOG(INFO) << m_logtag << "fail accocate audio resampled data buffer";
    return ret;
}

shared_ptr<AVFrame> AudioResample::allocOutFrame(const int nbSamples) {
    int ret;
    auto frame = shared_ptr<AVFrame>(av_frame_alloc(), [](AVFrame *p) {if (p) av_frame_free(&p);});
    CHECK(frame != nullptr);
    frame->nb_samples = nbSamples;
    frame->channel_layout = m_arp.m_dstLayout;
    frame->format = m_arp.m_dstFmt;
    frame->sample_rate = m_arp.m_dstSamplerate;
    ret = av_frame_get_buffer(frame.get(), 0);
    if (ret < 0) {
        LOG(ERROR) << m_logtag << "cannot allocate audio data buffer";
        return {};
    }
    return frame;
}

} // namespace ff_dynamic
