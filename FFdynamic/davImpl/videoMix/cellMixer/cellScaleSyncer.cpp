#include <cstdlib> // fabs
#include "cellScaleSyncer.h"

namespace ff_dynamic {

///////////////////////////////////////
// [process sync - send/receive frames]

int CellScaleSyncer::sendFrame(AVFrame *sendFrame) {
    /* 'sendFrame' won't process any mix timestamp issues. it just receives original frame, do scale,
       store scaled frame (include fps filter and set proper timestamp, in AV_TIME_BASE_Q) */
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!sendFrame) {
        m_bEof = true;
        return AVERROR_EOF;
    }
    if (m_startPts == AV_NOPTS_VALUE) {
        m_startPts = sendFrame->pts; /* fps scale won't change the first pts */
        m_curPts = sendFrame->pts;
    }
    /* shortcut for av_alloc_frame & av_frame_ref (copy props & ref buf) */
    AVFrame *refFrame = av_frame_clone(sendFrame); /* could be null */
    m_inputFrames.push_back(shared_ptr<AVFrame>(refFrame, [](AVFrame *p){av_frame_free(&p);}));
    return 0;
}

bool CellScaleSyncer::processSync(const int64_t curMixPts) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_bEof)
        return false;

    int ret = 0;
    if (!m_scaleFilter || sclaeFilterParamsChanged()) { /* open the scaler */
        m_scaledFrames.clear();
        ret = reopenCellScale();
        if (ret < 0) {
            LOG(ERROR) << m_logtag << "fail to open cell scale " << davMsg2str(ret)
                       << ", scale params: " << m_scaleParams;
            return false;
        }
    }

    if (m_startMixPts == AV_NOPTS_VALUE) {
        m_startMixPts = curMixPts;
        m_curMixPts = curMixPts;
        LOG(INFO) << m_logtag << "first receive frame with startMixPts " << m_startMixPts;
    }

    /* skip expired frames if exist, most of the time it is not */
    while (m_scaledFrames.size() > 0) {
        if (m_scaledFrames.front()->pts < m_startPts + curMixPts - m_startMixPts - m_withinOneFrameRange)
            m_scaledFrames.pop_front();
        else {
            break;
        }
    }

    /* feed one frame and stopped after there are scaled frame out */
    while (m_scaledFrames.size() == 0 && m_inputFrames.size() > 0) {
        auto & frame = m_inputFrames.front();
        ret = m_scaleFilter->sendFrame(frame.get());
        if (ret < 0 && ret != AVERROR_EOF) {
            LOG(ERROR) << m_logtag << "video syncer's scale filter send frame failed: " << davMsg2str(ret);
            m_discardFrames++;
        }
        m_inputFrames.pop_front();

        // receive scaled frames
        vector<shared_ptr<AVFrame>> scaledFrames;
        m_scaleFilter->receiveFrames(scaledFrames);
        m_scaledFrames.insert(std::end(m_scaledFrames), std::begin(scaledFrames), std::end(scaledFrames));
    };

    return m_scaledFrames.size() == 0 ? false : true;
}

/* formular for timestamp:
   video stream maintain its own timestam and this timestamp will be packed in VideoSyncEvent, then broadcast.
   VideoMix timestamp is passed in every 'receiveFrame' call, to determine:
     1. throw away old data (this happens at starting of VideoMix and use absolute timestamp)
     2. TODO
 */
shared_ptr<AVFrame> CellScaleSyncer::receiveFrame(const int64_t curMixPts) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_scaledFrames.size() == 0)
        return {};

    /* curMixPts - startMixPts == frame->pts - m_startPts */
    int64_t expectPts = m_startPts + (curMixPts - m_startMixPts);
    while (m_scaledFrames.size() > 0) {
        auto & frame = m_scaledFrames.front();
        if ((int64_t)(fabs(m_scaledFrames.front()->pts - expectPts)) <= m_withinOneFrameRange) {
            // found expect one
            m_curMixPts = curMixPts;
            m_curPts = frame->pts;
            m_scaledFrames.pop_front();
            return frame;
        } else if (frame->pts < expectPts) {
            m_scaledFrames.pop_front();
            continue;
        } else { /* frame in the future, just use it, but won't pop */
            return frame;
        }
    }
    return {};
}

///////////////////////////////
// [trival helpers]

int CellScaleSyncer::reopenCellScale() {
    if (m_scaleFilter) { // close scale filter if opened
        delete m_scaleFilter;
        m_scaleFilter = nullptr;
    }
    m_scaleFilter = new ScaleFilter();
    CHECK(m_scaleFilter != nullptr);
    LOG(INFO) << m_logtag << " reopen cell scale ";
    return m_scaleFilter->initScaleFilter(m_scaleParams);
}

int CellScaleSyncer::closeCellScaleSyncer() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_scaleFilter) {
        delete m_scaleFilter;
        m_scaleFilter = nullptr;
    }
    m_inputFrames.clear();
    m_scaledFrames.clear();
    return 0;
}

pair<int64_t, int64_t> CellScaleSyncer::getReadyFramePtsRange() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_startPts == AV_NOPTS_VALUE || m_startMixPts == AV_NOPTS_VALUE)
        return std::make_pair(-1, -1);
    if (m_scaledFrames.size() == 0 || !m_scaledFrames.front())
        return std::make_pair(-1, -1);
    int64_t ptsFront = m_scaledFrames.front()->pts - m_startPts + m_startMixPts;
    int64_t ptsBack = m_scaledFrames.back()->pts - m_startPts + m_startMixPts;
    return {ptsFront, ptsBack};
}

//int CellScaleSyncer::removeInputCounterpart(const int64_t removePts) {
//    /* TODO: should also refer to framerate to determine input frame need remove*/
//    m_inputFrames.erase(std::remove_if(m_inputFrames.begin(), m_inputFrames.end(),
//                                           [removePts](const shared_ptr<AVFrame> & f) {
//                                               return f && (f->pts <= removePts);}),
//                            m_inputFrames.end());
//    return 0;
//}

} // namespace ff_dynamic
