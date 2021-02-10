#pragma once

#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <glog/logging.h>
#include "ffmpegHeaders.h"
extern "C" {
#include "libavfilter/buffersink.h"
#include "libavfilter/buffersrc.h"
}

#include "davUtil.h"

namespace ff_dynamic {
using ::std::shared_ptr;
using ::std::string;
using ::std::vector;

struct FilterGraphParams {
    enum AVMediaType m_inMediaType = AVMEDIA_TYPE_UNKNOWN;
    enum AVMediaType m_outMediaType = AVMEDIA_TYPE_UNKNOWN;
    shared_ptr<AVBufferSrcParameters> m_bufsrcParams;
    shared_ptr<AVBufferSinkParams> m_bufsinkParams;
    shared_ptr<AVABufferSinkParams> m_abufsinkParams;
    string m_filterDesc;
    string m_logtag;
};

extern std::ostream &operator<<(std::ostream &os, const AVBufferSrcParameters &p);
extern std::ostream &operator<<(std::ostream &os, const FilterGraphParams &p);

class FilterGraph {
   public:
    FilterGraph() = default;
    virtual ~FilterGraph() { close(); }
    int initFilter(const FilterGraphParams &fgp);
    int close();
    int sendFilterFrame(AVFrame *frame, int srcFlags = AV_BUFFERSRC_FLAG_KEEP_REF);
    int receiveFilterFrames(vector<shared_ptr<AVFrame>> &filterFrames, int sinkFlags = 0);

   private:
    int prepareBufferSrc();
    int prepareBufferSink();
    int prepareFilter();

   protected:
    string m_logtag = "[FilterGraph] ";
    FilterGraphParams m_fgp;

   private:
    AVFilterGraph *m_filterGraph = nullptr;
    AVFilterContext *m_srcCtx = nullptr;
    AVFilterContext *m_sinkCtx = nullptr;
    AVFilter *m_src = nullptr;
    AVFilter *m_sink = nullptr;
    AVFilterInOut *m_inputs = nullptr;
    AVFilterInOut *m_outputs = nullptr;
};

/*
typedef struct AVBufferSrcParameters {
    int format;
    AVRational time_base;
    int width, height;
    AVRational sample_aspect_ratio;
    AVRational frame_rate;
    AVBufferRef *hw_frames_ctx;
    int sample_rate;
    uint64_t channel_layout;
} AVBufferSrcParameters;
*/
}  // namespace ff_dynamic
