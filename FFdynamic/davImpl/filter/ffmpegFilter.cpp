#include "ffmpegFilter.h"
#include "davDict.h"

namespace ff_dynamic {

//// Register ////
static DavImplRegister s_audioFitlerReg(DavWaveClassAudioFilter(), vector<string>({"auto", "ffmpeg"}), {},
                                        [](const DavWaveOption & options) -> unique_ptr<DavImpl> {
                                            unique_ptr<DavImpl> p(new FFmpegFilter(options));
                                            return p;
                                        });
static DavImplRegister s_videoFilterReg(DavWaveClassVideoFilter(), vector<string>({"auto", "ffmpeg"}), {},
                                        [](const DavWaveOption & options) -> unique_ptr<DavImpl> {
                                                unique_ptr<DavImpl> p(new FFmpegFilter(options));
                                                return p;
                                        });

const DavRegisterProperties & FFmpegFilter::getRegisterProperties() const noexcept {
    DavWaveClassCategory classCategory((DavWaveClassNotACategory()));
    m_options.getCategory(DavOptionClassCategory(), classCategory);
    if (classCategory == DavWaveClassVideoFilter())
        return s_videoFilterReg.m_properties;
    return s_audioFitlerReg.m_properties;
}

//////////////////////////////////////////////////////////////////////////////////////////
int FFmpegFilter::onDynamicallyInitializeViaTravelStatic(DavProcCtx & ctx) {
    if (m_filterGraph)
        onDestruct();

    return 0;
}

int FFmpegFilter::onConstruct() {
    int ret = 0;
    m_graphDesc = m_options.get("dav_filterDesc", "null");
    m_inputs = avfilter_inout_alloc();
    m_outputs = avfilter_inout_alloc();

    m_filterGraph = avfilter_graph_alloc();
    if (!m_outputs || !m_inputs || !m_filterGraph) {
        ret = AVERROR(ENOMEM);
        goto end;
    }

    /* buffer video source: the decoded frames from the decoder will be inserted here. */
    // TODO: preparse buffersrc args
    ret = prepareBufferSrc();
    if (ret < 0)
        goto end;
    ret = prepareBufferSink();
    if (ret < 0)
        goto end;

    /*
     * Set the endpoints for the filter graph. The filter_graph will
     * be linked to the graph described by filters_descr.
     */

    /*
     * The buffer source output must be connected to the input pad of
     * the first filter described by filters_descr; since the first
     * filter input label is not specified, it is set to "in" by
     * default.
     */
    m_outputs->name = av_strdup("in");
    m_outputs->filter_ctx = m_srcCtx;
    m_outputs->pad_idx = 0;
    m_outputs->next = NULL;

    /*
     * The buffer sink input must be connected to the output pad of
     * the last filter described by filters_descr; since the last
     * filter output label is not specified, it is set to "out" by
     * default.
     */
    m_inputs->name = av_strdup("out");
    m_inputs->filter_ctx = m_sinkCtx;
    m_inputs->pad_idx = 0;
    m_inputs->next = NULL;

    if ((ret = avfilter_graph_parse_ptr(m_filterGraph, m_graphDesc.c_str(), &m_inputs, &m_outputs, NULL)) < 0)
        goto end;

    if ((ret = avfilter_graph_config(m_filterGraph, NULL)) < 0)
        goto end;

end:
    // release anyway
    avfilter_inout_free(&m_inputs);
    avfilter_inout_free(&m_outputs);
    return ret;
}

int FFmpegFilter::onDestruct() {
    if (m_filterGraph) {
        avfilter_graph_free(&m_filterGraph); // related filters will be set null
    }
    avfilter_inout_free(&m_inputs);
    avfilter_inout_free(&m_outputs);
    return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////
int FFmpegFilter::prepareBufferSrc() {
    char args[512];
    const AVFilter *buffersrc = avfilter_get_by_name("buffer"); // abuffersrc, buffersrc,
   //snprintf(args, sizeof(args),
   //        "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
   //        dec_ctx->width, dec_ctx->height, dec_ctx->pix_fmt,
   //        time_base.num, time_base.den,
   //        dec_ctx->sample_aspect_ratio.num, dec_ctx->sample_aspect_ratio.den);

    int ret = avfilter_graph_create_filter(&m_srcCtx, buffersrc, "in", args, NULL, m_filterGraph);
    if (ret < 0)
        av_log(NULL, AV_LOG_ERROR, "Cannot create buffer source\n");
    return ret;
}

int FFmpegFilter::prepareBufferSink() {
    char args[512];
    const AVFilter *sink = avfilter_get_by_name("buffersink");
    int ret = avfilter_graph_create_filter(&m_sinkCtx, sink, "out", args, NULL, m_filterGraph);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot create buffer sink");
        return ret;
    }

    //ret = av_opt_set_int_list(buffersink_ctx, "pix_fmts", pix_fmts,
    //                          AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN);
    //if (ret < 0) {
    //    av_log(NULL, AV_LOG_ERROR, "Cannot set output pixel format\n");
    //    return ret;
    //}
    return 0;
}

int FFmpegFilter::prepareFilter(){

    return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////
int FFmpegFilter::onProcess(DavProcCtx & ctx) {

    return 0;
}
} // namespace ff_dynamic
