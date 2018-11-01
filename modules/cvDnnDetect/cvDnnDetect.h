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
    int processChangeConfidenceThreshold(const CvDynaEventChangeConfidenceThreshold & e);

private:
    unique_ptr<cv::dnn::Net> m_net;
};

} //namespace ff_dynamic
