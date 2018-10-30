#pragma once

#include "davDict.h"
#include "ffmpegHeaders.h"
#include "davImpl.h"

namespace ff_dynamic {

class FFmpegFilter : public DavImpl {
public:
    FFmpegFilter(const DavWaveOption & filterOptions) : DavImpl(filterOptions) {
        implDefaultInstantiate();
    }
    virtual ~FFmpegFilter() {onDestruct();}

private:
    FFmpegFilter(FFmpegFilter const &) = delete;
    FFmpegFilter & operator= (const FFmpegFilter &) = delete;
    virtual int onConstruct();
    virtual int onDestruct();
    virtual int onProcess(DavProcCtx & ctx);
    virtual int onDynamicallyInitializeViaTravelStatic(DavProcCtx & ctx);
    virtual int onProcessTravelDynamic(DavProcCtx & ctx) {return 0;}
    virtual const DavRegisterProperties & getRegisterProperties() const noexcept;
    int dynamicallyInitialize();

private:
    int prepareBufferSrc();
    int prepareBufferSink();
    int prepareFilter();

private:
    string m_graphDesc;
    AVFilterGraph *m_filterGraph = nullptr;
    AVFilterContext *m_srcCtx = nullptr;
    AVFilterContext *m_sinkCtx = nullptr;
    AVFilterInOut *m_inputs = nullptr;
    AVFilterInOut *m_outputs = nullptr;
};

} // namespace ff_dynamic
