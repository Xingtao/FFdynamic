#pragma once

// ffdynamic
#include "davWave.h"
#include "davImpl.h"
// project
#include "objDetect.h"
#include "objDetectPeerDynaEvent.h"
// darknet
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
    int processChangeConfThreshold(const DynaEventChangeConfThreshold & e);

private:
    network *m_net = nullptr;
    ObjDetectParams m_dps;
    cv::Scalar m_means;
    vector<string> m_classNames;
    unsigned long m_inputCount = 0;
};

} //namespace ff_dynamic
