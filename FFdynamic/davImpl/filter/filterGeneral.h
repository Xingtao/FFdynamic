#pragma once

#include <string>
#include <vector>
#include <memory>
#include <iostream>
#include "glog/logging.h"
#include "davUtil.h"
#include "ffmpegHeaders.h"
extern "C" {
#include "libavfilter/buffersrc.h"
#include "libavfilter/buffersink.h"
}

namespace ff_dynamic {
using ::std::string;
using ::std::vector;
using ::std::shared_ptr;

struct FilterGeneralParams {
    enum AVMediaType m_inMediaType = AVMEDIA_TYPE_UNKNOWN;
    enum AVMediaType m_outMediaType = AVMEDIA_TYPE_UNKNOWN;
    shared_ptr<AVBufferSrcParameters> m_bufsrcParams;
    shared_ptr<AVBufferSinkParams> m_bufsinkParams;
    shared_ptr<AVABufferSinkParams> m_abufsinkParams;
    string m_filterDesc;
    string m_logtag;
};

extern std::ostream & operator<<(std::ostream & os, const AVBufferSrcParameters & p);
extern std::ostream & operator<<(std::ostream & os, const FilterGeneralParams & p);

class FilterGeneral {
public:
    FilterGeneral() = default;
    virtual ~FilterGeneral() {close();}
    int initFilter(const FilterGeneralParams & fgp);
    int close();
    int sendFilterFrame(AVFrame *frame, int srcFlags = AV_BUFFERSRC_FLAG_KEEP_REF);
    int receiveFilterFrames(vector<shared_ptr<AVFrame>> & filterFrames, int sinkFlags = 0);

private:
    int prepareBufferSrc();
    int prepareBufferSink();
    int prepareFilter();

protected:
    string m_logtag = "[FilterGeneral] ";
    FilterGeneralParams m_fgp;

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

typedef struct AVABufferSinkParams {
    const enum AVSampleFormat *sample_fmts; ///< list of allowed sample formats, terminated by AV_SAMPLE_FMT_NONE
    const int64_t *channel_layouts;         ///< list of allowed channel layouts, terminated by -1
    const int *channel_counts;              ///< list of allowed channel counts, terminated by -1
    int all_channel_counts;                 ///< if not 0, accept any channel count or layout
    int *sample_rates;                      ///< list of allowed sample rates, terminated by -1
} AVABufferSinkParams;


filter是ffmpeg的libavfilter提供的基础单元。在同一个线性链中的filter使用逗号分隔，在不同线性链中的filter使用分号隔开，比如下面的例子：

   ffmpeg -i INPUT -vf "split [main][tmp]; [tmp] crop=iw:ih/2:0:0, vflip [flip]; [main][flip] overlay=0:H/2" OUTPUT

这里crop、vflip处于同一个线性链，split、overlay位于另一个线性链。二者连接通过命名的label实现（位于中括号中的是label的名字）。在上例中split filter有两个输出，依次命名为[main]和[tmp]；[tmp]作为crop filter输入，之后通过vflip filter输出[flip]；overlay的输入是[main]和[flilp]。如果filter需要输入参数，多个参数使用冒号分割。
对于没有音频、视频输入的filter称为source filter，没有音频、视频输出的filter称为sink filter。


///////
把一个问题转化为特定环境（上下文）下的一系列子问题，divide and conque


////////////
 * @defgroup lavfi_buffersink_accessors Buffer sink accessors
 * Get the properties of the stream
 * @{
enum AVMediaType av_buffersink_get_type                (const AVFilterContext *ctx);
AVRational       av_buffersink_get_time_base           (const AVFilterContext *ctx);
int              av_buffersink_get_format              (const AVFilterContext *ctx);

AVRational       av_buffersink_get_frame_rate          (const AVFilterContext *ctx);
int              av_buffersink_get_w                   (const AVFilterContext *ctx);
int              av_buffersink_get_h                   (const AVFilterContext *ctx);
AVRational       av_buffersink_get_sample_aspect_ratio (const AVFilterContext *ctx);

int              av_buffersink_get_channels            (const AVFilterContext *ctx);
uint64_t         av_buffersink_get_channel_layout      (const AVFilterContext *ctx);
int              av_buffersink_get_sample_rate         (const AVFilterContext *ctx);

AVBufferRef *    av_buffersink_get_hw_frames_ctx       (const AVFilterContext *ctx);


*/

} // namespace ff_dynamic
