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
    virtual const DavRegisterProperties & getRegisterProperties() const noexcept;

private: // event process
    int processChangeConfThreshold(const CvDynaEventChangeConfThreshold & e);

private: /* helpers */
    const vector<cv::String> & getOutputsNames();
    int postprocess(const cv::Mat & image, const vector<cv::Mat> & outs,
                    shared_ptr<CvDnnDetectEvent> & detectEvent);

public:
    struct DetectParams { /* internal use, for clearity */
        string m_detectorType;
        string m_detectorFrameworkTag;
        string m_modelPath;
        string m_configPath;
        string m_classnamePath;
        int m_backendId = 3;
        int m_targetId = 0;
        double m_scaleFactor;
        cv::Scalar m_means; // Scalar mean, BGR order
        bool m_bSwapRb = false;
        int m_width = -1;
        int m_height = -1;
        double m_confThreshold = 0.7;
    };

private:
    cv::dnn::Net m_net;
    DetectParams m_dps;
    vector<cv::String> m_outBlobNames;
    vector<string> m_classNames;
};

extern std::ostream & operator<<(std::ostream & os, const CvDnnDetect::DetectParams & p);

} //namespace ff_dynamic
