#pragma once

#include "ffmpegHeaders.h"
#include "davImpl.h"

namespace ff_dynamic {

class FFmpegAudioDecode : public DavImpl {
public:
    FFmpegAudioDecode(const DavWaveOption & options) : DavImpl(options) {
        implDefaultInstantiate();
    }
    virtual ~FFmpegAudioDecode() {onDestruct();}

private:
    FFmpegAudioDecode(const FFmpegAudioDecode &) = delete;
    FFmpegAudioDecode & operator= (const FFmpegAudioDecode &) = delete;
    virtual int onConstruct();
    virtual int onDestruct();
    virtual int onProcess(DavProcCtx & ctx);
    virtual int onDynamicallyInitializeViaTravelStatic(DavProcCtx & ctx);
    virtual int onProcessTravelDynamic(DavProcCtx & ctx) {return 0;}
    int dynamicallyInitialize(const AVCodecParameters *codecpar);
    virtual const DavRegisterProperties & getRegisterProperties() const noexcept;

private:
    AVCodecContext *m_decCtx = nullptr;
    uint64_t m_outputFrames = 0;
    uint64_t m_discardFrames = 0;
};

} //namespace ff_dynamic
