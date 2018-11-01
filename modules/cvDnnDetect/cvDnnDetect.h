#pragma once

#include <opencv2/dnn.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>
#include "davWave.h"
#include "davImpl.h"
#include "cvPeerDynaEvent.h"

namespace ff_dynamic {

/* create CvDnnDetect class category */
struct DavWaveClassCvDnnDetect : public DavWaveClassCategory {
    DavWaveClassCvDnnDetect () :
        DavWaveClassCategory(type_index(typeid(*this)), type_index(typeid(std::string)), "CvDnnDetect") {}
};

/* options passing use AVDictionary */
class CvDnnDetect : public DavImpl {
public:
    CvDnnDetect(const DavWaveOption & options) : DavImpl(options) {
        implDefaultInstantiate();
    }
    virtual ~CvDnnDetect() {onDestruct();}

private:
    CvDnnDetect(const CvDnnDetect &) = delete;
    CvDnnDetect & operator= (const CvDnnDetect &) = delete;
    virtual int onConstruct();
    virtual int onDestruct();
    virtual int onProcess(DavProcCtx & ctx);
    virtual int onProcessTravelDynamic(DavProcCtx & ctx) {return 0;}
    virtual int onDynamicallyInitializeViaTravelStatic(DavProcCtx & ctx);

private: // event process
    int processChangeConfidenceThreshold(const CvDynaEventChangeConfThreshold & e);

private:
    unique_ptr<cv::dnn::Net> m_net;
    struct DetectParams { /* internal use, for clearity */
        string m_detectorType;
        string m_detectorFrameworkTag;
        int m_backend = 3;
        int m_targetId = 0;
        double m_scaleFactor;
        double means = 12; // Scalar mean;
        bool swap_rb = 13;
        int32 width = 14;
        int32 height = 15;
        string model_path = 20;
        string config_path = 21;
    };
    DetectParams m_dps;
};

} //namespace ff_dynamic
