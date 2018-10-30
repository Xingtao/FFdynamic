#pragma once

#include "ffmpegHeaders.h"
#include "davImpl.h"

namespace ff_dynamic {

class FFmpegVideoDecode : public DavImpl {
public:
    FFmpegVideoDecode(const DavWaveOption & options) : DavImpl(options) {
        implDefaultInstantiate();
    }
    virtual ~FFmpegVideoDecode() {onDestruct();}

private:
    FFmpegVideoDecode(const FFmpegVideoDecode &) = delete;
    FFmpegVideoDecode & operator= (const FFmpegVideoDecode &) = delete;
    virtual int onConstruct();
    virtual int onDestruct();
    virtual int onProcess(DavProcCtx & ctx);
    virtual int onDynamicallyInitializeViaTravelStatic(DavProcCtx & ctx);
    virtual int onProcessTravelDynamic(DavProcCtx & ctx) {return 0;}
    virtual const DavRegisterProperties & getRegisterProperties() const noexcept;
    int dynamicallyInitialize(const AVCodecParameters *codecpar);

private:
    AVCodecContext *m_decCtx = nullptr;
    uint64_t m_discardFrames = 0;
};

} //namespace ff_dynamic
