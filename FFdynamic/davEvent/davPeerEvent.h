#pragma once

#include <vector>
#include <string>
#include <iostream>
#include <typeinfo>
#include <typeindex>

#include "davProcBuf.h"

namespace ff_dynamic {

/* Base class for Peer communication: Public-Subscribe event and Travel Dynamic Event */
struct DavPeerEvent {
    DavProcFrom m_procFrom;
    /* getSelf return different derived event (covariant return types) */
    virtual const DavPeerEvent & getSelf() const = 0;
    inline DavProcFrom & getAddress() noexcept {return m_procFrom;}
    inline void setAddress(const DavProcFrom & procFrom) noexcept {m_procFrom = procFrom;}
};

struct DavEventVideoMixSync : public DavPeerEvent {
    virtual const DavEventVideoMixSync & getSelf() const {return *this;}
    struct VideoStreamInfo {
        DavProcFrom m_from; /* normally, each video stream is in different group */
        int64_t m_curPts = 0; /* video stream's pts, not mixPts. in AV_TIME_BASE_Q */
    };
    vector<VideoStreamInfo> m_videoStreamInfos;
    int64_t m_videoMixCurPts = 0; /* in AV_TIME_BASE_Q */
};

//// Other basic structure could be used by derived events
struct DavRect {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
};

} // namespace ff_dynamic
