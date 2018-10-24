#include "davImplTravel.h"

namespace ff_dynamic {

ostream & operator<<(ostream & os, const DavTravelStatic & s) {
    os << "Media Type: " << av_get_media_type_string(s.m_mediaType);
    if (s.m_mediaType == AVMEDIA_TYPE_VIDEO) {
        os  << ", pixel format " << (int)s.m_pixfmt << ", timebase " << s.m_timebase << ", width " << s.m_width
            << ", height " << s.m_height << ", sar " << s.m_sar << ", framerate " << s.m_framerate
            << ", hwFramesctx " << s.m_hwFramesCtx;
    } else if (s.m_mediaType == AVMEDIA_TYPE_AUDIO) {
        os  << ", sample format " << av_get_sample_fmt_name(s.m_samplefmt) << ", timebase "
            << s.m_timebase << ", sample rate " << s.m_samplerate << ", channels "
            << s.m_channels << ", channel layout " << s.m_channelLayout;
    } else {
        os << "unsupported media type " << av_get_media_type_string(s.m_mediaType)
           << " of travel static";
    }
    return os;
}

//////////////////////////////////////////////
// [merge travel static to DavWaveOption options]

/* merge part: use videopar/audiopar when options not exist, normally used by encoders */
int DavTravelStatic::mergeVideoDavTravelStaticToDict(DavWaveOption & options) {
    // TODO: twice of get_buffer_fmt, so it may not right for the first time
    options.set("pixel_format", std::to_string(m_pixfmt), AV_DICT_DONT_OVERWRITE);
    options.setVideoSize(m_width, m_height, AV_DICT_DONT_OVERWRITE);
    options.setAVRational("time_base", m_timebase, AV_DICT_DONT_OVERWRITE);
    options.setAVRational("sar", m_sar, AV_DICT_DONT_OVERWRITE);
    options.setAVRational("framerate", m_framerate, AV_DICT_DONT_OVERWRITE);
    return 0;
}

int DavTravelStatic::mergeAudioDavTravelStaticToDict(DavWaveOption & options) {
    options.setAVRational("time_base", m_timebase, AV_DICT_DONT_OVERWRITE);
    options.set("sample_fmt", std::to_string(m_samplefmt), AV_DICT_DONT_OVERWRITE);
    options.set("ar", std::to_string(m_samplerate), AV_DICT_DONT_OVERWRITE);
    options.set("ac", std::to_string(m_channels), AV_DICT_DONT_OVERWRITE);
    options.set("channel_layout", std::to_string(m_channelLayout), AV_DICT_DONT_OVERWRITE);
    return 0;
}

////////////////////////////////////////////
// [travel static video settings]

int DavTravelStatic::
setupVideoStatic(AVCodecContext *codecCtx, const AVRational & timebase,
                 const AVRational & framerate, AVBufferRef *hwFramesCtx) noexcept {
    CHECK(codecCtx != nullptr);
    m_codecpar.reset(avcodec_parameters_alloc(),
                     [](AVCodecParameters *p) {avcodec_parameters_free(&p);});
    avcodec_parameters_from_context(m_codecpar.get(), codecCtx);
    return setupVideoStatic(m_codecpar.get(), timebase, framerate, hwFramesCtx);
}

int DavTravelStatic::
setupVideoStatic(AVCodecParameters *codecpar, const AVRational & timebase,
                 const AVRational & framerate, AVBufferRef *hwFramesCtx) noexcept {
    m_mediaType = AVMEDIA_TYPE_VIDEO;
    m_timebase = timebase;
    m_framerate = framerate;
    m_hwFramesCtx = hwFramesCtx;
    if (!m_codecpar) {
        m_codecpar.reset(avcodec_parameters_alloc(),
                         [](AVCodecParameters *p) {avcodec_parameters_free(&p);});
        avcodec_parameters_copy(m_codecpar.get(), codecpar);
    }
    return setupVideoStatic((enum AVPixelFormat)codecpar->format, codecpar->width, codecpar->height,
                            timebase, framerate, codecpar->sample_aspect_ratio, hwFramesCtx);
}

int DavTravelStatic::
setupVideoStatic(enum AVPixelFormat pixfmt, int width, int height,
                     const AVRational & timebase, const AVRational & framerate,
                     const AVRational & sampleAspectRatio, AVBufferRef *hwFramesCtx) noexcept {
    m_mediaType = AVMEDIA_TYPE_VIDEO;
    m_pixfmt = pixfmt;
    m_width = width;
    m_height = height;
    m_timebase = timebase;
    m_framerate = framerate;
    m_sar = sampleAspectRatio;
    m_hwFramesCtx = hwFramesCtx;
    return 0;
}

////////////////////////////////////////////
//  [audio travel static settings]

int DavTravelStatic::
setupAudioStatic(AVCodecContext *codecCtx, const AVRational & timebase) noexcept {
    CHECK(codecCtx != nullptr);
    m_codecpar.reset(avcodec_parameters_alloc(),
                     [](AVCodecParameters *p) {avcodec_parameters_free(&p);});
    avcodec_parameters_from_context(m_codecpar.get(), codecCtx);
    return setupAudioStatic(m_codecpar.get(), timebase);
}

int DavTravelStatic::
setupAudioStatic(AVCodecParameters *codecpar, const AVRational & timebase) noexcept {
    m_mediaType = AVMEDIA_TYPE_AUDIO;
    if (!m_codecpar) {
        m_codecpar.reset(avcodec_parameters_alloc(),
                         [](AVCodecParameters *p) {avcodec_parameters_free(&p);});
        avcodec_parameters_copy(m_codecpar.get(), codecpar);
    }
    m_timebase = timebase;
    return setupAudioStatic((enum AVSampleFormat)codecpar->format, timebase, codecpar->sample_rate,
                            codecpar->channels, codecpar->channel_layout);
}

int DavTravelStatic::
setupAudioStatic(enum AVSampleFormat samplefmt, const AVRational & timebase,
                     int samplerate, int channels, uint64_t channelLayout) noexcept {
    m_mediaType = AVMEDIA_TYPE_AUDIO;
    m_samplefmt = samplefmt;
    m_timebase = timebase;
    m_samplerate = samplerate;
    m_channels = channels;
    m_channelLayout = channelLayout;
    return 0;
}

} // namespace ff_dynamic
