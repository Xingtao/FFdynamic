#pragma once

#include "davWave.h"
#include "davImpl.h"
#include "dehazor.h"

namespace ff_dynamic {

/* create cvDnnDetect component category */
struct DavWaveClassCVDnnDetect : public DavWaveClassCategory {
    DavWaveClassCVDnnDetect () :
        DavWaveClassCategory(type_index(typeid(*this)), type_index(typeid(std::string)), "CVDnnDetect") {}
};

/* options to control */
struct DavOptionCVDnnDetectNetModel : public DavOption {
    DavOptionCVDnnDetectFogFactor() :
        DavOption(type_index(typeid(*this)), type_index(typeid(std::string)), "CVDnnDetectNetModel") {}
};

struct CVDnnDetectorAddEvent {
    string m_netModelType; /* yolo3, ssd, tensorflow, pytorch, caffe */
    string m_modelPath;
    string m_
};

/* cvDnnDetect component may have diffrent implementation;
   so we would also register impl later, refer to register part at bottom of ffdynaDehazor.cpp's file */

/* here is one implementation called, CVDnnDetect */
class CVDnnDetect : public DavImpl {
public:
    CVDnnDetect(const DavWaveOption & options) : DavImpl(options) {
        implDefaultInstantiate();
    }
    virtual ~CVDnnDetect() {onDestruct();}

private: /* Interface we should implement */
    CVDnnDetect(const CVDnnDetect &) = delete;
    CVDnnDetect & operator= (const CVDnnDetect &) = delete;
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
