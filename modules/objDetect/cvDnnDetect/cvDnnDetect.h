#pragma once

#include <opencv2/dnn.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>
#include "davWave.h"
#include "davImpl.h"

#include "objDetect.h"
#include "objDetectPeerDynaEvent.h"

namespace ff_dynamic {

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
    virtual const DavRegisterProperties & getRegisterProperties() const noexcept;

private: // event process
    int processChangeConfThreshold(const DynaEventChangeConfThreshold & e);

private: /* helpers */
    const vector<cv::String> & getOutputsNames();
    int postprocess(const cv::Mat & image, const vector<cv::Mat> & outs,
                    shared_ptr<ObjDetectEvent> & detectEvent);

private:
    cv::dnn::Net m_net;
    cv::Scalar m_means;
    ObjDetectParams m_dps;
    vector<cv::String> m_outBlobNames;
    vector<string> m_classNames;
    unsigned long m_inputCount = 0;
};

} //namespace ff_dynamic
