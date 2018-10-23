#pragma once

#include "davWave.h"
#include "davImpl.h"
#include "dehazor.h"

namespace ff_dynamic {
using namespace dehaze;

/* create dehaze component category */
struct DavWaveClassFaceDetect : public DavWaveClassCategory {
    DavWaveClassFaceDetect () :
        DavWaveClassCategory(type_index(typeid(*this)), type_index(typeid(std::string)), "FaceDetect") {}
};

/* define dehaze options to illustrate how to use option passing */
struct DavOptionFaceDetectFogFactor : public DavOption {
    DavOptionFaceDetectFogFactor() :
        DavOption(type_index(typeid(*this)), type_index(typeid(double)), "FaceDetectFogFactor") {}
};

/* define an event to illustrate how to use dynamic event */
struct FogFactorChangeEvent {
    double m_newFogFactor = 0.1;
};

/* dehaze component may have diffrent implementation;
   so we would also register impl later, refer to register part at bottom of ffdynaDehazor.cpp's file */

/* here is one implementation called, FaceDetect */
class FaceDetect : public DavImpl {
public:
    FaceDetect(const DavWaveOption & options) : DavImpl(options) {
        implDefaultInstantiate();
    }
    virtual ~FaceDetect() {onDestruct();}

private: /* Interface we should implement */
    FaceDetect(const FaceDetect &) = delete;
    FaceDetect & operator= (const FaceDetect &) = delete;
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
