#include <map>
#include "ffmpegHeaders.h"
#include "davMessager.h"

namespace  ff_dynamic {

struct DavImplTimestamp {
    DavImplTimestamp(const AVRational & tbSrc, const AVRational & tbDst)
        : m_tbSrc(tbSrc), m_tbDst(tbDst) {
    }
    virtual ~DavImplTimestamp() = default;
    int packetRescaleTs(AVPacket *pkt) {
        if (pkt->dts == AV_NOPTS_VALUE)
            return DAV_ERROR_IMPL_PKT_NO_DTS;
        av_packet_rescale_ts(pkt, m_tbSrc, m_tbDst);
        if (m_lastDts != AV_NOPTS_VALUE && pkt->dts < m_lastDts)
            return DAV_ERROR_IMPL_DTS_NOT_MONOTONIC;
        m_lastDts = pkt->dts;
        m_lastPts = pkt->pts;
        if (m_firstDts == AV_NOPTS_VALUE)
            m_firstDts = pkt->dts;
        if (m_firstPts == AV_NOPTS_VALUE)
            m_firstPts = pkt->pts;
        return 0;
    }
    int frameRescaleTs(AVFrame *frame) {
        if (frame->pts != AV_NOPTS_VALUE) {
            frame->pts = av_rescale_q(frame->pts, m_tbSrc, m_tbDst);
            m_lastFramePts = frame->pts;
            if (m_firstFramePts == AV_NOPTS_VALUE)
                m_firstFramePts = frame->pts;
        }
        return 0;
    }

    inline int64_t rescale(int64_t input) {
        return av_rescale_q(input, m_tbSrc, m_tbDst);
    }
    inline int64_t getFirstDts () {return m_firstDts;}
    inline int64_t getFirstPts () {return m_firstPts;}
    inline void setLastDts (int64_t dts) {m_lastDts = dts;}
    inline void setLastPts (int64_t pts) {m_lastPts = pts;}
    inline void setLastFramePts(int64_t pts) {m_lastFramePts = pts;}
    inline int64_t getLastDts () {return m_lastDts;}
    inline int64_t getLastPts () {return m_lastPts;}
    inline int64_t getLastFramePts() {return m_lastFramePts;}
    inline void setTbSrc (AVRational & s) {m_tbSrc = s;}
    inline void setTbDst (AVRational & d) {m_tbDst = d;}
    inline const AVRational & getTbSrc () const {return m_tbSrc;}
    inline const AVRational & getTbDst () const {return m_tbDst;}

private:
    AVRational m_tbSrc = {0, -1};
    AVRational m_tbDst = {0, -1};
    int64_t m_lastDts = AV_NOPTS_VALUE;
    int64_t m_lastPts = AV_NOPTS_VALUE;
    int64_t m_lastFramePts = AV_NOPTS_VALUE;
    int64_t m_firstDts = AV_NOPTS_VALUE;
    int64_t m_firstPts = AV_NOPTS_VALUE;
    int64_t m_firstFramePts = AV_NOPTS_VALUE;
};

} // namespace ff_dynamic
