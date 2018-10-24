#pragma once

#include <iostream>
#include <memory>
#include "glog/logging.h"
#include "ffmpegHeaders.h"
#include "davDict.h"

namespace ff_dynamic {
using ::std::ostream;
using ::std::shared_ptr;

//////////////////////////////////////////////////////////////////////////////////////////
/* DavImplTravel:
   Passing impl's two types of exported info to its connected peer (static and dynamic export info)
   Normally, 'static' exports impl's peroperties, such as codecpar, timebase, format, etc.., and
   'static' info is alway outputed;
   'dynamic' is event driven, such as object detection, ROIs region, etc...,
   which is generated and output only in one specific process call.
*/

struct DavTravelStatic {
    AVMediaType m_mediaType = AVMEDIA_TYPE_UNKNOWN;
    /* codecpar is convenient for audio/video codec open (and muxer add stream) */
    shared_ptr<AVCodecParameters> m_codecpar = nullptr;
    AVRational m_timebase = {0, 1}; /* this field also use with codecpar */

    /* video specific */
    enum AVPixelFormat m_pixfmt = AV_PIX_FMT_NONE;
    int m_width = 0;
    int m_height = 0;
    AVRational m_sar = {0, 1};
    AVRational m_framerate = {0, 1}; /* this field also used if codecpar exists */
    AVBufferRef *m_hwFramesCtx = nullptr; /* this field also used if codecpar exists */

    /* audio specific */
    enum AVSampleFormat m_samplefmt = AV_SAMPLE_FMT_NONE;
    int m_samplerate = 0;
    int m_channels = 0;
    uint64_t m_channelLayout = 0;

    /////////////////////////////////////////////////////////////////////////////////////////
    /* merge/setup part: use videopar/audiopar when options not exist, normally used by encoders */
    int mergeVideoDavTravelStaticToDict(DavWaveOption & options);
    int mergeAudioDavTravelStaticToDict(DavWaveOption & options);
    /* */
    int setupVideoStatic(AVCodecContext *codecCtx, const AVRational & timebase,
                             const AVRational & framerate = {0, 1},
                             AVBufferRef *hwFramesCtx = nullptr) noexcept;
    int setupVideoStatic(AVCodecParameters *codecpar, const AVRational & timebase,
                             const AVRational & framerate = {0, 1},
                             AVBufferRef *hwFramesCtx = nullptr) noexcept;
    int setupVideoStatic(enum AVPixelFormat pixfmt, int width, int height,
                             const AVRational & timebase, const AVRational & framerate = {0, 1},
                             const AVRational & sampleAspectRatio = {0, 1},
                             AVBufferRef *hwFramesCtx = nullptr) noexcept;
    /* */
    int setupAudioStatic(AVCodecContext *codecCtx, const AVRational & timebase) noexcept;
    int setupAudioStatic(AVCodecParameters *codecpar, const AVRational & timebase) noexcept;
    int setupAudioStatic(enum AVSampleFormat samplefmt, const AVRational & timebase,
                             int samplerate, int channels, uint64_t channelLayout) noexcept;
};

extern ostream & operator<<(ostream & os, const DavTravelStatic & s);

/* base class for travel dynamic: take advantage of covariant return type to do polymorphism */
struct DavTravelDynamic {
    virtual const DavTravelDynamic & getSelf() const = 0;
};

struct DavTravelROI : DavTravelDynamic {
    virtual const DavTravelROI & getSelf() const {return *this;}
    struct ROI {
        int x = 0;
        int y = 0;
        int w = 0;
        int h = 0;
        string roiTag;
    };
    vector<ROI> rois;
};

/* TODO: use it directly or break it down */
struct FFmpegAVPacketSideDataEvent {
    AVPacketSideData m_avPacketSideData;
};

} // namespace ff_dynamic
