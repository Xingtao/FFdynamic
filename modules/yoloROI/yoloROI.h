#pragma once

#include "davWave.h"
#include "davImpl.h"
#include "dehazor.h"

namespace ff_dynamic {
using namespace dehaze;

/* create dehaze component category */
struct DavWaveClassYoloROI : public DavWaveClassCategory {
    DavWaveClassYoloROI () :
        DavWaveClassCategory(type_index(typeid(*this)), type_index(typeid(std::string)), "YoloROI") {}
};

/* define dehaze options to illustrate how to use option passing */
struct DavOptionYoloROIFogFactor : public DavOption {
    DavOptionYoloROIFogFactor() :
        DavOption(type_index(typeid(*this)), type_index(typeid(double)), "YoloROIFogFactor") {}
};

/* define an event to illustrate how to use dynamic event */
struct FogFactorChangeEvent {
    double m_newFogFactor = 0.1;
};

/* dehaze component may have diffrent implementation;
   so we would also register impl later, refer to register part at bottom of ffdynaDehazor.cpp's file */

/* here is one implementation called, YoloROI */
class YoloROI : public DavImpl {
public:
    YoloROI(const DavWaveOption & options) : DavImpl(options) {
        implDefaultInstantiate();
    }
    virtual ~YoloROI() {onDestruct();}

private: /* Interface we should implement */
    YoloROI(const YoloROI &) = delete;
    YoloROI & operator= (const YoloROI &) = delete;
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
