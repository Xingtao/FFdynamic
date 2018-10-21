#pragma once

#include <mutex>
#include "ffmpegHeaders.h"
#include "audioResample.h"

namespace ff_dynamic {

static constexpr double DEFAULT_MAX_AUDIO_SIZE_IN_MS = 3000.0; /* 3 seconds */

class AudioSyncer {
public:
    AudioSyncer() = default;
    virtual ~AudioSyncer() {closeAudioSyncer();}

public:
    int initAudioSyncer(const AudioResampleParams & arp, const size_t groupId);
    int closeAudioSyncer();
    int sendFrame(AVFrame *frame);
    /* do sync process, return whether ready to call receiveFrame, also compute output data samples len.
       must be called before receiveFrame */
    bool processSync(const int desiredSize, const int64_t curMixPts);
    shared_ptr<AVFrame> receiveFrame();
    int processVideoPeerSyncEvent(const int64_t curVideoMixPts, const int64_t videoPeerCurPts);

public:
    inline const AudioResampleParams & getSyncerResampleParams() const {return m_arp;}
    inline size_t getGroupId() {return m_groupId;}

private:
    AudioSyncer(const AudioSyncer &) = delete;
    AudioSyncer & operator= (const AudioSyncer &) = delete;
    std::mutex m_mutex;
    size_t m_groupId = 0;
    string m_logtag;
    /* audio first pts / audio cur pts / audio fifo size */
    AudioResample *m_resampler = nullptr;
    AudioResampleParams m_arp;
    /* sync formular: Vmix - Amix = Vpeer - Acur */
    int64_t m_curVideoMixPts = AV_NOPTS_VALUE;
    int64_t m_videoPeerCurPts = AV_NOPTS_VALUE;
    int64_t m_curAudioMixPts = AV_NOPTS_VALUE;
    int64_t m_startPts = AV_NOPTS_VALUE; /* pts of audio itself */
    int64_t m_curPts = AV_NOPTS_VALUE;
    /* calculate in 'processSync' and used in 'receiveFrame' call */
    int m_audioOutputDataLen = 0;
};

} // namespace ff_dynamic
