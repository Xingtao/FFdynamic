#pragma once

#include "davWave.h"
#include "davImpl.h"
#include "dehazor.h"

namespace ff_dynamic {

/* create cvObjDetect component category */
struct DavWaveClassCVObjDetect : public DavWaveClassCategory {
    DavWaveClassCVObjDetect () :
        DavWaveClassCategory(type_index(typeid(*this)), type_index(typeid(std::string)), "CVObjDetect") {}
};

/* options to control */
struct DavOptionCVObjDetectNetModel : public DavOption {
    DavOptionCVObjDetectFogFactor() :
        DavOption(type_index(typeid(*this)), type_index(typeid(std::string)), "CVObjDetectNetModel") {}
};

struct CVObjDetectorAddEvent {
    string m_netModelType; /* yolo3, ssd, tensorflow, pytorch, caffe */
    string m_modelPath;
    string m_
};

/* cvObjDetect component may have diffrent implementation;
   so we would also register impl later, refer to register part at bottom of ffdynaDehazor.cpp's file */

/* here is one implementation called, CVObjDetect */
class CVObjDetect : public DavImpl {
public:
    CVObjDetect(const DavWaveOption & options) : DavImpl(options) {
        implDefaultInstantiate();
    }
    virtual ~CVObjDetect() {onDestruct();}

private: /* Interface we should implement */
    CVObjDetect(const CVObjDetect &) = delete;
    CVObjDetect & operator= (const CVObjDetect &) = delete;
    virtual int onConstruct();
    virtual int onDestruct();
    virtual int onProcess(DavProcCtx & ctx);
    virtual int onDynamicallyInitializeViaTravelStatic(DavProcCtx & ctx);
    /* if no travel dynamic needed, leave it empty */
    virtual int onProcessTravelDynamic(DavProcCtx & ctx) {return 0;}

private:
    int processFogFactorUpdate(const FogFactorChangeEvent & e);

private:
    unique_ptr<Dehazor> m_dehazor;
};

} //namespace ff_dynamic
