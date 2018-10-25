 #pragma once

#include "ffmpegHeaders.h"
#include "davImpl.h"
#include "x264.h"

namespace ff_dynamic {

class X264Encode : public DavImpl {
public:
    X264Encode(const DavWaveOption & options) : DavImpl(options) {
        implDefaultInstantiate();
    }
    virtual ~X264Encode() {onDestruct();}

private: /* data process */
    X264Encode(const X264Encode &) = delete;
    X264Encode & operator= (const X264Encode &) = delete;
    virtual int onConstruct();
    virtual int onDestruct();
    virtual int onProcess(DavProcCtx & ctx);
    virtual int onDynamicallyInitializeViaTravelStatic(DavProcCtx & ctx);
    virtual int onProcessTravelDynamic(DavProcCtx & ctx) {return 0;}
    int dynamicallyInitialize(const DavImplTravel::TravelStatic & in);
    int setupScaleFilter(const DavImplTravel::TravelStatic & in, const DavImplTravel::TravelStatic & out);
    int receiveEncodeFrames(DavProcCtx & ctx);

private: /* event process */

private:
    AVCodecContext *m_encCtx = nullptr;
    ScaleFilter *m_scaleFilter = nullptr;
    ScaleFilterParams m_sfp;
    uint64_t m_discardFrames = 0;
    bool m_bForcedKeyFrame = false;
};

} //namespace ff_dynamic
