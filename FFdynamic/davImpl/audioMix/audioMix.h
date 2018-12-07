#pragma once

#include "davImpl.h"
#include "audioResample.h"
#include "audioSyncer.h"

namespace ff_dynamic {

class AudioMix : public DavImpl {
public:
    AudioMix(const DavWaveOption & options) : DavImpl(options) {
        implDefaultInstantiate();
    }
    virtual ~AudioMix() {onDestruct();}

private:
    AudioMix(const AudioMix &) = delete;
    AudioMix & operator= (const AudioMix &) = delete;
    virtual int onConstruct();
    virtual int onDestruct();
    virtual int onProcess(DavProcCtx & ctx);
    virtual int onDynamicallyInitializeViaTravelStatic(DavProcCtx & ctx) {
        return 0;
    }
    virtual int onProcessTravelDynamic(DavProcCtx & ctx) {return 0;}
    virtual const DavRegisterProperties & getRegisterProperties() const noexcept;

    int processVideoMixSync(const DavEventVideoMixSync &);
    int processMuteUnute(const DavDynaEventAudioMixMuteUnmute & event);

private:
    int addOneSyncerStream(DavProcCtx & ctx);
    int processVideoSyncEvent() {return 0;}
    int mixFrameByFramePts(DavProcCtx & ctx);
    int toMixFrame(AVFrame *mixFrame, const AVFrame *frame);
    int setupMixFrame(AVFrame *mixFrame);

private:
    map<DavProcFrom, unique_ptr<AudioSyncer>> m_syncers;
    vector<size_t> m_muteGroups;
    bool m_bMuteAtStart = false;
    int m_frameSize = 1024;
    enum AVSampleFormat m_dstFmt = AV_SAMPLE_FMT_FLTP;
    int m_dstSamplerate = 44100;
    uint64_t m_dstLayout = AV_CH_LAYOUT_STEREO;
    int m_bytesPerSample = 4;
    uint64_t m_outputMixFrames = 0;
    uint64_t m_discardInput = 0;
    uint64_t m_discardOutput = 0;
    /* this value is calculated according to first video sync, for both absolute timestamp and
       re-generate timestamp */
    int64_t m_startMixPts = AV_NOPTS_VALUE;
    int64_t m_curMixPts = AV_NOPTS_VALUE;
    int64_t m_videoMixStartPts = AV_NOPTS_VALUE;
    int64_t m_videoMixCurPts = AV_NOPTS_VALUE;
    DavEventVideoMixSync m_lastVideoSync;
    bool m_bAutoQuit = true;
};

} //namespace ff_dynamic
