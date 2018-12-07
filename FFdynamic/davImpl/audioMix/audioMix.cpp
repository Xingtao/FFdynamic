#include "audioMix.h"

namespace ff_dynamic {
//// [Register] ////
static DavImplRegister s_audioMixReg(DavWaveClassAudioMix(), vector<string>({"auto", "AudioMix"}), {},
                                     [](const DavWaveOption & options) -> unique_ptr<DavImpl> {
                                         unique_ptr<DavImpl> p(new AudioMix(options));
                                         return p;
                                     });

const DavRegisterProperties & AudioMix::getRegisterProperties() const noexcept {
    return s_audioMixReg.m_properties;
}

////////////////////////////////////
/* The base assumptions is:
     0. video send sync event, then audio can output with ts bigger than sync ts;
     1. video's sync pts monotonic increase
   Then we have following rules:
     1. each video stream maintain its own pts, could be smaller then video's CurMixPts;
     2. if video not coming and audio data is accumulated bigger than a max value, oldest audio data will be discarded
     3. jitter buffer is on video side, which will use 300ms - 500ms jitter
   Others:
     1. all audio use 1/dstSamplerate as timebase, then videoPeerPts will convert to this timebase;
*/

//////////////////////////////////////////////////////////////////////////////////////////
// [Process Event]

int AudioMix::processVideoMixSync(const DavEventVideoMixSync & event) {
    m_lastVideoSync = event;
    if (m_startMixPts == AV_NOPTS_VALUE) {
        m_startMixPts = av_rescale_q(event.m_videoMixCurPts, AV_TIME_BASE_Q, AVRational {1, m_dstSamplerate});
        m_curMixPts = m_startMixPts; /* this may result throw away few of starting audio data */
        LOG(INFO) << m_logtag << "audio startMixPts "  << m_startMixPts << ", " << event.m_videoMixCurPts;
    }
    m_videoMixCurPts = av_rescale_q(event.m_videoMixCurPts, AV_TIME_BASE_Q, AVRational {1, m_dstSamplerate});
    for (auto & vsi : event.m_videoStreamInfos) {
        for (auto & syncer : m_syncers) {/* some groups' SyncEvent may be discarded, it is ok */
            if (vsi.m_from.m_groupId == syncer.second->getGroupId()) {
                syncer.second->processVideoPeerSyncEvent(m_videoMixCurPts,
                             av_rescale_q(vsi.m_curPts, AV_TIME_BASE_Q, AVRational{1, m_dstSamplerate}));
            }
        }
    }
    return 0;
}

/* dynamic event process and data process are in the same thread, no race */
int AudioMix::processMuteUnute(const DavDynaEventAudioMixMuteUnmute & event) {
    /* unmute first */
    for (auto gid : event.m_unmuteGroupIds) {
        m_muteGroups.erase(std::remove(m_muteGroups.begin(), m_muteGroups.end(), gid),
                           m_muteGroups.end());
        LOG(INFO) << m_logtag << "unmute stream of group " << gid;
    }
    for (auto gid : event.m_muteGroupIds) {
        m_muteGroups.emplace_back(gid);
        LOG(INFO) << m_logtag << "will mute stream of group " << gid;
    }
    return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////
// [audio sync object]
int AudioMix::addOneSyncerStream(DavProcCtx & ctx) {
    int ret = 0;
    const DavProcFrom & from = ctx.m_inBuf->getAddress();
    auto in = m_inputTravelStatic.at(from);

    /* output */
    m_timestampMgr.emplace(from, DavImplTimestamp(in->m_timebase, {1, m_dstSamplerate}));

    unique_ptr<AudioSyncer> syncer(new AudioSyncer());
    CHECK(syncer != nullptr);
    AudioResampleParams arp;
    arp.m_logtag = m_logtag + toStringViaOss(from);
    arp.m_srcFmt = in->m_samplefmt;
    arp.m_srcSamplerate = in->m_samplerate;
    arp.m_srcLayout = in->m_channelLayout == 0 ?
        av_get_default_channel_layout (in->m_channels) : in->m_channelLayout;
    arp.m_dstFmt = m_dstFmt;
    arp.m_dstSamplerate = m_dstSamplerate;
    arp.m_dstLayout = m_dstLayout;
    ret = syncer->initAudioSyncer(arp, from.m_groupId);
    if (ret < 0) {
        ERRORIT(ret, "failed to create audio syncer with " + toStringViaOss(arp));
        return ret;
    }
    m_syncers.emplace(from, std::move(syncer));
    if (m_bMuteAtStart) {
        m_muteGroups.emplace_back(from.m_groupId);
    }
    LOG(INFO) << m_logtag << "add new audio syncer " << from << (m_bMuteAtStart ? "muted at starting" : "");
    return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////
// [construct - destruct - process]
int AudioMix::onConstruct() {
    /* how many samples that we would like to output as a whole frame */
    m_options.getInt("frame_size", m_frameSize);
    m_options.getBool("b_mute_at_start", m_bMuteAtStart);

    /* register event */
    std::function<int (const DavEventVideoMixSync &)> f =
        [this] (const DavEventVideoMixSync & e) {return processVideoMixSync(e);};
    m_implEvent.registerEvent(f);
    std::function<int (const DavDynaEventAudioMixMuteUnmute &)> m =
        [this] (const DavDynaEventAudioMixMuteUnmute & e) {return processMuteUnute(e);};
    m_implEvent.registerEvent(m);

    /* */
    auto out = make_shared<DavTravelStatic>();
    out->setupAudioStatic(m_dstFmt, {1, m_dstSamplerate}, m_dstSamplerate,
                          av_get_channel_layout_nb_channels(m_dstLayout), m_dstLayout);
    m_outputTravelStatic.emplace(IMPL_SINGLE_OUTPUT_STREAM_INDEX, out);
    m_outputMediaMap.emplace(IMPL_SINGLE_OUTPUT_STREAM_INDEX, AVMEDIA_TYPE_AUDIO);

    /* mark as initialized and process dynamic input peer in onProcess */
    m_bDynamicallyInitialized = true;
    LOG(INFO) << m_logtag << "audio mix will deal with dynamic join in";
    return 0;
}

int AudioMix::onDestruct() {
    m_syncers.clear();
    LOG(WARNING) << m_logtag << "AudioMix closed, output mix frames " << m_outputMixFrames << ", discard input "
                 << m_discardInput << ", discard output " << m_discardOutput << ", startMixPts"
                 << m_startMixPts << ", curMixPts " << m_curMixPts;
    return 0;
}

int AudioMix::onProcess(DavProcCtx & ctx) {
    ctx.m_expect.m_expectOrder = {EDavExpect::eDavExpectAnyOne};
    if (!ctx.m_inBuf)
        return 0;

    int ret = 0;
    const DavProcFrom & from = ctx.m_inBuf->getAddress();
    /* check newly joined stream */
    if (m_syncers.count(from) == 0) {
        ret = addOneSyncerStream(ctx);
        if (ret < 0)
            return AVERROR(EAGAIN);
    }

    auto & inFrame = ctx.m_inRefFrame;
    if (!inFrame) {
        ctx.m_bInputFlush = true;
        if (m_syncers.count(from))
            m_muteGroups.erase(std::remove(m_muteGroups.begin(), m_muteGroups.end(),
                                           m_syncers.at(from)->getGroupId()),
                               m_muteGroups.end());
        /* TODO: won't release here, but wait for video's end event */
        // m_syncers.erase(from);
        LOG(INFO) << m_logtag << "audio mix recive one flush frame from " << from;
    } else {
        ret = m_syncers.at(from)->sendFrame(inFrame);
        if (ret < 0) {
            ERRORIT(ret, m_logtag + "send frame fail " + toStringViaOss(from));
            m_discardInput++;
        }
    }

    if (m_curMixPts == AV_NOPTS_VALUE && !ctx.m_bInputFlush)
        return AVERROR(EAGAIN);

    while (m_curMixPts + m_frameSize <= m_videoMixCurPts) {
        vector<bool> bMixContinue;
        for (auto & syncer : m_syncers) {
            bool bContinue = syncer.second->processSync(m_frameSize, m_curMixPts);
            bMixContinue.push_back(bContinue);
        }
        if (!all(bMixContinue))
            break;
        mixFrameByFramePts(ctx);
        /* the order matters, increase pts after mix this frame */
        m_curMixPts += m_frameSize;
    }

    if (ctx.m_bInputFlush) {
        INFOIT(DAV_INFO_ONE_FINISHED_MIX_STREAM, toStringViaOss(from) + " finished in audio mix");
        if (ctx.m_froms.size() > 1)
            return 0;
        if (!m_bAutoQuit) { /* the only input left */
            INFOIT(ret, "video mix has no inputs, auto quit is false, stay still");
            ret = 0;
        } else
            ret = AVERROR_EOF;
    }
    return ret;
}

/////////////////////
// [internal helpers]

int AudioMix::mixFrameByFramePts(DavProcCtx & ctx) {
    auto outBuf = make_shared<DavProcBuf>();
    CHECK(outBuf != nullptr);
    AVFrame *mixFrame = outBuf->mkAVFrame();
    CHECK(mixFrame != nullptr);
    int ret = setupMixFrame(mixFrame);
    if (ret < 0) {
        m_discardOutput++;
        ERRORIT(ret, "cannot allocate mixed frame data, discard one mixed frame");
        return ret;
    }
    for (auto & syncer : m_syncers){
        shared_ptr<AVFrame> frame = syncer.second->receiveFrame();
        if (!frame) /* could return null, if syncer in skip status */
            continue;
        /* check muted participant here (skip mixing then) */
        if (std::find(m_muteGroups.begin(), m_muteGroups.end(), syncer.second->getGroupId()) != m_muteGroups.end())
            continue;
        toMixFrame(mixFrame, frame.get());
    }
    mixFrame->pts = m_curMixPts;
    m_outputMixFrames++;
    outBuf->m_travelStatic = m_outputTravelStatic.at(IMPL_SINGLE_OUTPUT_STREAM_INDEX);
    ctx.m_outBufs.push_back(outBuf);
    return 0;
}

int AudioMix::toMixFrame(AVFrame *mixFrame, const AVFrame *frame) {
    /* right now, only AV_SAMPLE_FMT_FLTP and AV_CH_LAYOUT_STEREO supported */
    /* frame->linesize[0] is one channel's size, it may not equal to m_frameSize */
    const int offset = mixFrame->nb_samples - frame->nb_samples;
    CHECK(offset >= 0 && m_dstFmt == AV_SAMPLE_FMT_FLTP && m_dstLayout == AV_CH_LAYOUT_STEREO);
    const int channels = av_get_channel_layout_nb_channels(m_dstLayout);
    for (int k=0; k < channels; k++) {
        float *dataDst = (float *)mixFrame->data[k] + offset;
        float *dataSrc = (float *)frame->data[k];
        for (int j=0; j < frame->nb_samples; j++) {
            dataDst[j] = (dataDst[j] + dataSrc[j]) / 2;
        }
    }
    return 0;
}

int AudioMix::setupMixFrame(AVFrame *mixFrame) {
    CHECK(mixFrame != nullptr);
    mixFrame->nb_samples = m_frameSize;
    mixFrame->channel_layout = m_dstLayout;
    mixFrame->format = m_dstFmt;
    mixFrame->sample_rate = m_dstSamplerate;
    int ret = av_frame_get_buffer(mixFrame, 0);
    if (ret < 0)
        return ret;
    const int channels = av_get_channel_layout_nb_channels(m_dstLayout);
    for (int k=0; k < channels; k++) {
        memset(mixFrame->data[k], 0, mixFrame->linesize[0]);
    }
    return 0;
}

} // namespace
