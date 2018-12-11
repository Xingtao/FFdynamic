#pragma once

// ffdynamic
#include "davWave.h"
#include "davImpl.h"
// project
#include "objDetect.h"
#include "objDetectPeerDynaEvent.h"

#include "darknet.h"

namespace ff_dynamic {

/* options passing use AVDictionary */
class DarknetDetect : public DavImpl {
public:
    DarknetDetect(const DavWaveOption & options) : DavImpl(options) {
        implDefaultInstantiate();
    }
    virtual ~DarknetDetect() {onDestruct();}

private:
    DarknetDetect(const DarknetDetect &) = delete;
    DarknetDetect & operator= (const DarknetDetect &) = delete;
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
                    shared_ptr<DarknetDetectEvent> & detectEvent);

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
        int m_detectInterval = 1;
    };

private:
    cv::dnn::Net m_net;
    DetectParams m_dps;
    vector<cv::String> m_outBlobNames;
    vector<string> m_classNames;
    unsigned long m_inputCount = 0;
};

extern std::ostream & operator<<(std::ostream & os, const DarknetDetect::DetectParams & p);

} //namespace ff_dynamic
