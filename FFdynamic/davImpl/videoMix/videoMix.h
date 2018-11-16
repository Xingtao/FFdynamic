#pragma once

#include "davImpl.h"
#include "cellMixer.h"
#include "cellSetting.h"

namespace ff_dynamic {

class VideoMix : public DavImpl {
public:
    /* please keep this class as concise as possible, move related settings to related class */
    VideoMix(const DavWaveOption & options) : DavImpl(options) {
        implDefaultInstantiate();
    }
    virtual ~VideoMix() {onDestruct();}

private:
    VideoMix(const VideoMix &) = delete;
    VideoMix & operator= (const VideoMix &) = delete;
    virtual int onConstruct();
    virtual int onDestruct();
    virtual int onProcess(DavProcCtx & ctx);
    virtual int onDynamicallyInitializeViaTravelStatic(DavProcCtx & ctx) {return 0;};
    virtual int onProcessTravelDynamic(DavProcCtx & ctx) {return 0;}
    virtual const DavRegisterProperties & getRegisterProperties() const noexcept;
    int constructVideoMixWithOptions();

private: // event process
    int onUpdateLayoutEvent(const DavDynaEventVideoMixLayoutUpdate & e);
    int onUpdateBackgroudEvent(const DavDynaEventVideoMixSetNewBackgroud & event);

private:
    uint64_t m_outputCount = 0;
    /* do the real mix works */
    CellMixer m_cellMixer;
    EDavVideoMixLayout m_layout = EDavVideoMixLayout::eLayoutAuto;
    /* mix cell adornment setting, set via DavWaveOption and pass to each cell */
    CellAdornment m_adornment; /* each cell use the same adornment at start, could be changed */
    string m_backgroudPath;
    bool m_bReGeneratePts = true;
    bool m_bStartAfterAllJoin = false; /* whether wait for all peers joined, useful for fixed inputs */
    bool m_bQuitIfNoInput = true; /* whether quit when no connected input peers */

    ///////////////////////////////////////////////////////////////////
    /* parameters set via DavOptions */
    /* mix basic parameters settings, write to: m_outputTravelStatic */
    int m_width = 1920;
    int m_height = 1080;
    enum AVPixelFormat m_pixfmt = AV_PIX_FMT_YUV420P; /* also could be AV_PIX_FMT_CUDA */
    AVRational m_framerate {25, 1};
    static constexpr AVRational s_sar{1, 1};
    static constexpr AVRational s_timebase {1, AV_TIME_BASE}; /* will use this one as output timebase */
};

} //namespace ff_dynamic
