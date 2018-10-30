#pragma once

#include "ffmpegHeaders.h"
#include "davUtil.h"
#include "davDict.h"
#include "davImpl.h"
#include "davImplTravel.h"

namespace ff_dynamic {

class FFmpegDemux : public DavImpl {
public:
    FFmpegDemux(const DavWaveOption & options) : DavImpl(options) {
        implDefaultInstantiate();
    }
    virtual ~FFmpegDemux() {onDestruct();}

private:
    FFmpegDemux(const FFmpegDemux &) = delete;
    FFmpegDemux & operator= (const FFmpegDemux &) = delete;
    int dynamicallyInitialize();
    virtual int onConstruct();
    virtual int onDestruct();
    virtual int onProcess(DavProcCtx & ctx);
    virtual int onDynamicallyInitializeViaTravelStatic(DavProcCtx & ctx) {return 0;}
    virtual int onProcessTravelDynamic(DavProcCtx & ctx) {return 0;}
    virtual const DavRegisterProperties & getRegisterProperties() const noexcept;

private: // helpers
    int hardSettings();

private:
    AVFormatContext *m_fmtCtx = nullptr;
    string m_inputUrl;
    int m_reconnectRetries = 0;
    int m_rwTimeoutMs = 5000; /* default use 5s */

    bool m_bInputFpsEmulate = false;
    vector<int64_t> m_streamStartTime;
    int64_t m_lastLogTime = -1;

    // only stat for audio & video, no other streams
    uint64_t m_inBytes = 0;
    uint64_t m_discardBytes = 0;
    uint64_t m_outPacket[2] = {0};
    uint64_t m_discardPacket[2] = {0};
};

} //namespace ff_dynamic
