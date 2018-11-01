#pragma once

#include "davPeerEvent.h"

namespace ff_dynamic {

/* Peer communication: Public-Subscribe events */
/* object detector broadcast its detect result to peers (include classify) */
struct CvObjDetectEvent : public DavPeerEvent {
    virtual const CvObjDetectEvent & getSelf() const {return *this;}
    int64_t m_framePts = 0;
    string m_detectorType; /* for simplicity, use string. only two right now: 'classify' or 'detect' */
    string m_detectorFrameworkTag; /* detailed tag of the model: yolo, ssd, etc.. */
    struct DetectResult {
        string m_objName;
        double m_confidence;
        DavRect m_rect; /* not used for classify */
    };
    DetectResult m_results;
};

/* external dynamic events */
struct CvDynaEventChangeConfidenceThreshold {
    double m_confidenceThreshold = 0.7;
};
