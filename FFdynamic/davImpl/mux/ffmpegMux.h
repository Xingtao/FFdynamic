#pragma once

#include <map>
#include "ffmpegHeaders.h"
#include "davImpl.h"

namespace ff_dynamic {
using ::std::map;

class FFmpegMux : public DavImpl {
public:
    FFmpegMux(const DavWaveOption & options) : DavImpl(options) {
        implDefaultInstantiate();
    }
    virtual ~FFmpegMux () {onDestruct();}

private:
    FFmpegMux(FFmpegMux const &) = delete;
    FFmpegMux & operator= (FFmpegMux const &) = delete;
    virtual int onConstruct();
    virtual int onDestruct();
    virtual int onProcess(DavProcCtx & ctx);
    virtual int onDynamicallyInitializeViaTravelStatic(DavProcCtx & ctx);
    virtual int onProcessTravelDynamic(DavProcCtx & ctx) {return 0;}
    virtual const DavRegisterProperties & getRegisterProperties() const noexcept;

private:
    int dynamicallyInitialize(DavProcCtx & ctx);
    AVStream* addOneStream(const DavTravelStatic & travelStatic);
    int muxMetaDataSettings();

private:
    string m_outputUrl;
    string m_outputMuxFmt;
    AVFormatContext *m_fmtCtx = nullptr;
    vector<int> m_inPacketCount;
    uint64_t m_outputCount = 0;
    uint64_t m_outputDiscardCount = 0;
    map<DavProcFrom, AVStream *> m_muxStreamsMap;
};

} // namespace ff_dynamic
