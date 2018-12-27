#pragma once

#include <iostream>
#include <iomanip>
#include "davPeerEvent.h"

namespace ff_dynamic {

/* Peer events (Public-Subscribe) */
struct ObjDetectEvent : public DavPeerEvent {
    virtual const ObjDetectEvent & getSelf() const {return *this;}
    int64_t m_framePts = 0;
    string m_detectOrClassify; /* for simplicity, use string. only two right now: 'classify' or 'detect' */
    string m_detectorFrameworkTag; /* detailed tag of the model: yolo, ssd, etc.. */
    double m_inferTime = -1.0; /* negative if unknown */
    struct DetectResult {
        string m_className{"unknown"};
        double m_confidence = 0.0;
        DavRect m_rect; /* not used for classify */
    };
    vector<DetectResult> m_results;
};

inline std::ostream & operator<<(std::ostream & os, const ObjDetectEvent & e) {
    os << "{detector " << e.m_detectorFrameworkTag << ", inferTime " << e.m_inferTime << ", pts " << e.m_framePts;
    for (auto & r : e.m_results)
        os << " [className " << r.m_className << ", conf " << std::setprecision(3)
           << r.m_confidence << " " << r.m_rect << "]";
    os << "}\n";
    return os;
}

/* external dynamic events */
struct DynaEventChangeConfThreshold {
    double m_confThreshold = 0.7;
};

} // namespace ff_dynamic
