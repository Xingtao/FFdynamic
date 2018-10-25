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

/* already exist one
struct DavTravelROI : DavTravelDynamic {
    virtual const DavTravelROI & getSelf() const {return *this;}
    struct ROI {
        int x = 0;
        int y = 0;
        int w = 0;
        int h = 0;
        string roiTag;
    };
    vector<ROI> rois;
}; */

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
    unique_ptr<Dehazor> m_dehazor;
};

} //namespace ff_dynamic
