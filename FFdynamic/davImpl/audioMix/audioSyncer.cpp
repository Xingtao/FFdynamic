#include "audioSyncer.h"

namespace ff_dynamic {

int AudioSyncer::
processVideoPeerSyncEvent(const int64_t curVideoMixPts, const int64_t videoPeerCurPts) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_curVideoMixPts = curVideoMixPts;
    m_videoPeerCurPts = videoPeerCurPts;
    // LOG(INFO) << m_logtag << " " << m_curVideoMixPts << "|" << m_videoPeerCurPts << "|" << m_curAudioMixPts;
    return 0;
}

int AudioSyncer::initAudioSyncer(const AudioResampleParams & arp, const size_t groupId) {
    int ret = 0;
    if (m_resampler) {
        delete m_resampler;
        m_resampler = nullptr;
    }
    m_groupId = groupId;
    m_logtag = "AudioSyncer" + std::to_string(m_groupId);
    m_resampler = new AudioResample();
    CHECK(m_resampler !=nullptr);
    ret = m_resampler->initResampler(arp);
    if (ret < 0) {
        LOG(ERROR) << "failed create audio resample: " << ret;
        return ret;
    }
    return 0;
}

int AudioSyncer::closeAudioSyncer() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_resampler) {
        delete m_resampler;
        m_resampler = nullptr;
    }
    return 0;
}

int AudioSyncer::sendFrame(AVFrame *frame) {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_resampler->sendResampleFrame(frame);
}

shared_ptr<AVFrame> AudioSyncer::receiveFrame() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_audioOutputDataLen <= 0)
        return {};
    return m_resampler->receiveResampledFrame(m_audioOutputDataLen);
}

bool AudioSyncer::processSync(const int desiredSize, const int64_t curMixPts) {
    CHECK(desiredSize >= 0);
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_videoPeerCurPts == AV_NOPTS_VALUE) {
        /* video not coming, and we have audio data more than MAX, throw away one frame */
        const int64_t curPts = m_resampler->getCurPts();
        if (m_resampler->getFifoCurSizeInMs() > DEFAULT_MAX_AUDIO_SIZE_IN_MS) {
            m_resampler->receiveResampledFrame(desiredSize);
            LOG(INFO) << m_logtag << " skip one frame with size " << desiredSize << ", pts " << curPts;
        }
        m_audioOutputDataLen = 0;
        return true;
    }

    /* sync formular: Vmix - Amix = Vpeer - Acur, and Vpeer is the frame's pts mixed to Vmix */
    m_curAudioMixPts = curMixPts;
    int64_t desiredAudioDataPts = m_videoPeerCurPts - m_curVideoMixPts + curMixPts;
    int64_t curAudioPts = m_resampler->getCurPts();
    int curDataSampleSize = m_resampler->getFifoCurSize();

    /* we have 3 situations: data in future, data ready, data in past; process them case by case */
    /* 1. data in future */
    if (curAudioPts >= desiredAudioDataPts + desiredSize) {
        LOG(INFO) << m_logtag << "Audio data is in future, cur " << curAudioPts << ", desired "
                  << desiredAudioDataPts << ". video peer " << m_videoPeerCurPts
                  << ", video mix " << m_curVideoMixPts << ", audio mix " << curMixPts;
        m_audioOutputDataLen = 0;
        return true;
    }

    /* 2. data ready */
    if (curAudioPts + curDataSampleSize - desiredAudioDataPts - desiredSize > 0) {
        if (curAudioPts >= desiredAudioDataPts) {
            m_audioOutputDataLen = desiredAudioDataPts + desiredSize - curAudioPts;
        }
        else {
            m_audioOutputDataLen = desiredSize;
            const int throwDataSize = (int)(desiredAudioDataPts - curAudioPts);
            m_resampler->receiveResampledFrame(throwDataSize);
            LOG(INFO) << m_logtag << " Throw dealyed data with size " << throwDataSize
                      << ", pts " << curAudioPts;
        }
        return true;
    }

    // 3. not ready and data may in past
    int throwDataSize = (int)(desiredAudioDataPts - curAudioPts);
    throwDataSize = throwDataSize > curDataSampleSize ? curDataSampleSize : throwDataSize;
    if (throwDataSize > 0) {
        m_resampler->receiveResampledFrame(throwDataSize);
        LOG(INFO) << m_logtag << "In Past. Throw dealyed data with size "
                  << throwDataSize << ", pts " << curAudioPts;
    }
    m_audioOutputDataLen = 0;
    return false;
}

} // namespace ff_dynamic
