#pragma once

#include "ffmpegHeaders.h"
#include "davImpl.h"
#include "audioResample.h"

namespace ff_dynamic {

class FFmpegAudioEncode : public DavImpl {
public:
    FFmpegAudioEncode(const DavWaveOption & options) : DavImpl(options) {
        implDefaultInstantiate();
    }
    virtual ~FFmpegAudioEncode() {onDestruct();}

private:
    FFmpegAudioEncode(const FFmpegAudioEncode &) = delete;
    FFmpegAudioEncode & operator= (const FFmpegAudioEncode &) = delete;
    virtual int onConstruct();
    virtual int onDestruct();
    virtual int onProcess(DavProcCtx & ctx);
    virtual int onDynamicallyInitializeViaTravelStatic(DavProcCtx & ctx);
    virtual int onProcessTravelDynamic(DavProcCtx & ctx) {return 0;}
    virtual const DavRegisterProperties & getRegisterProperties() const noexcept;
    int dynamicallyInitialize();

private:
    AVCodecContext *m_encCtx = nullptr;
    AVAudioFifo *m_inFifo = nullptr;
    uint64_t m_inFrames = 0;
    uint64_t m_outFrames = 0;
    uint64_t m_discardInput = 0;
    uint64_t m_discardOutput = 0;

    int64_t m_totalInputSamples = 0;    /* in srcSamplerate */
    int64_t m_totalResampleSamples = 0; /* in dstSamplerate */
    uint64_t m_totalOutputBytes = 0;
    int64_t m_encoderDelayInMs = 0;     /* input / output cached samples in micro second */

private:
    AudioResample *m_resampler = nullptr;
    int openAudioResample(const DavTravelStatic & in, const DavTravelStatic & out);
    int sampleFormatSet(AVCodec *codec);
    int receiveEncodeFrames(DavProcCtx & ctx);
};

} //namespace ff_dynamic
