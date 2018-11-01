#pragma once

#include "davPeerEvent.h"

namespace ff_dynamic {

/* Peer events (Public-Subscribe) */
struct CvDnnDetectEvent : public DavPeerEvent {
    virtual const CvObjDetectEvent & getSelf() const {return *this;}
    int64_t m_framePts = 0;
    string m_detectorType; /* for simplicity, use string. only two right now: 'classify' or 'detect' */
    string m_detectorFrameworkTag; /* detailed tag of the model: yolo, ssd, etc.. */
    double m_inferTime = -1.0; /* negative if unknown */
    struct DetectResult {
        string m_className;
        double m_confidence;
        DavRect m_rect; /* not used for classify */
    };
    vector<DetectResult> m_results;
};

/* external dynamic events */
struct CvDynaEventChangeConfThreshold {
    double m_confThreshold = 0.7;
};
