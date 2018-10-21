#pragma once

#include <deque>
#include <string>
#include <utility>
#include <mutex>
#include "davMessager.h"
#include "ffmpegHeaders.h"
#include "scaleFilter.h"
#include "cellSetting.h"

namespace ff_dynamic {
using ::std::string;
using ::std::pair;
using ::std::deque;

class VideoMix;
static constexpr double DEFAULT_MAX_VIDEO_SIZE_IN_MS = 500.0; /* 500ms */

/////////////
class CellScaleSyncer {
public:
    explicit CellScaleSyncer(const string & logtag) : m_logtag (logtag) {}
    virtual ~CellScaleSyncer() {closeCellScaleSyncer();}

public:
    int sendFrame(AVFrame *frame);
    shared_ptr<AVFrame> receiveFrame(const int64_t curMixPts);
    bool processSync(const int64_t curMixPts);
    pair<int64_t, int64_t> getReadyFramePtsRange();
    int updateSyncerScaleParam(const ScaleFilterParams & sfp) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_scaleParams = sfp;
        AVRational m_outFramerate = {0, 1};
        m_withinOneFrameRange = av_rescale_q(1, av_inv_q(m_outFramerate), AV_TIME_BASE_Q);
        return 0;
    }

public: /* helpers */

private:
    CellScaleSyncer(const CellScaleSyncer &) = delete;
    CellScaleSyncer & operator= (const CellScaleSyncer &) = delete;
    int reopenCellScale();
    int closeCellScaleSyncer();
    inline bool sclaeFilterParamsChanged() { /* under lock call */
        return m_scaleParams == m_scaleFilter->getScaleFilterParams();
    }

private:
    const string m_logtag {"CellScaleSyncer"};
    std::mutex m_mutex;
    int64_t m_startPts = AV_NOPTS_VALUE;
    int64_t m_curPts = AV_NOPTS_VALUE;
    int64_t m_startMixPts = AV_NOPTS_VALUE;
    int64_t m_curMixPts = AV_NOPTS_VALUE;
    /* calculate from framerate, within one frame in  AV_TIME_BASE_Q timebase */
    int64_t m_withinOneFrameRange = 40000;
    uint64_t m_discardFrames = 0;

private: /* cell scale part */
    ScaleFilter *m_scaleFilter = nullptr;
    ScaleFilterParams m_scaleParams;
    deque<shared_ptr<AVFrame>> m_scaledFrames;
    deque<shared_ptr<AVFrame>> m_inputFrames;
    bool m_bEof = false;
};

} // namespace ff_dynamic
