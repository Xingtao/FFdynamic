#include <iostream>
#include "filterGeneral.h"

namespace ff_dynamic {

//////////////////////////////////////////////////////////////////////////////////////////
std::ostream & operator<<(std::ostream & os, const AVBufferSrcParameters & p) {
    os << "pixel fmt " << p.format << ", timebase " << p.time_base
       << ", width " << p.width << ", height " << p.height << ", sar " << p.sample_aspect_ratio
       << ", frame_rate " << p.frame_rate << ", hwFramesCtx " << p.hw_frames_ctx << ", samplerate "
       << p.sample_rate << ", channel_layout " << p.channel_layout;
    return os;
}

std::ostream & operator<<(std::ostream & os, const FilterGeneralParams & p) {
    os << p.m_logtag << " of FilterGeneral Params: \nin-format " << av_get_media_type_string(p.m_inMediaType)
       << ", out-format " << av_get_media_type_string(p.m_outMediaType)
       << ", buffersrc: " << (*p.m_bufsrcParams.get()) << ", filter desc: " << p.m_filterDesc;
    return os;
}

int FilterGeneral::initFilter(const FilterGeneralParams & fgp) {
    int ret = 0;
    m_fgp = fgp;
    LOG(INFO) << m_logtag << "FilterGeneral incoming params: " << m_fgp;
    if (!fgp.m_logtag.empty())
        m_logtag = fgp.m_logtag;

    if (!fgp.m_bufsrcParams) {
        LOG(ERROR) << m_logtag << "buffer src params cannot be null";
        return AVERROR(EINVAL);
    }

    if ((fgp.m_inMediaType != AVMEDIA_TYPE_AUDIO && fgp.m_inMediaType != AVMEDIA_TYPE_VIDEO) ||
        (fgp.m_outMediaType != AVMEDIA_TYPE_AUDIO && fgp.m_outMediaType != AVMEDIA_TYPE_VIDEO)) {
        LOG(ERROR) << m_logtag << "in/out media type not valid: in "
                   << av_get_media_type_string(fgp.m_inMediaType) << ", out "
                   << av_get_media_type_string(fgp.m_outMediaType);
        return AVERROR(EINVAL);
    }

    m_inputs = avfilter_inout_alloc();
    m_outputs = avfilter_inout_alloc();
    m_filterGraph = avfilter_graph_alloc();
    if (!m_outputs || !m_inputs || !m_filterGraph) {
        ret = AVERROR(ENOMEM);
        return ret;
    }

    ret = prepareBufferSrc();
    if (ret < 0) {
        char buf[AV_ERROR_MAX_STRING_SIZE] = {0};
        av_strerror(ret, buf, AV_ERROR_MAX_STRING_SIZE);
        LOG(ERROR) << m_logtag << "failed prepare buffer src:" << buf;
        return ret;
    }

    ret = prepareBufferSink();
    if (ret < 0) {
        char buf[AV_ERROR_MAX_STRING_SIZE] = {0};
        av_strerror(ret, buf, AV_ERROR_MAX_STRING_SIZE);
        LOG(ERROR) << m_logtag << "failed prepare buffer sink:" << buf;
        return ret;
    }

    m_outputs->name = av_strdup("in");
    m_outputs->filter_ctx = m_srcCtx;
    m_outputs->pad_idx = 0;
    m_outputs->next = nullptr;

    m_inputs->name = av_strdup("out");
    m_inputs->filter_ctx = m_sinkCtx;
    m_inputs->pad_idx = 0;
    m_inputs->next = nullptr;

    ret = avfilter_graph_parse_ptr(m_filterGraph, m_fgp.m_filterDesc.c_str(), &m_inputs, &m_outputs, nullptr);
    if (ret < 0) {
        char buf[AV_ERROR_MAX_STRING_SIZE] = {0};
        av_strerror(ret, buf, AV_ERROR_MAX_STRING_SIZE);
        LOG(ERROR) << m_logtag << "failed graph_parse_ptr:" << buf;
        return ret;
    }

    ret = avfilter_graph_config(m_filterGraph, nullptr);
    if (ret < 0) {
        char buf[AV_ERROR_MAX_STRING_SIZE] = {0};
        av_strerror(ret, buf, AV_ERROR_MAX_STRING_SIZE);
        LOG(ERROR) << m_logtag << "failed config graph:" << buf;
        return ret;
    }

    //const char *dumpGraphStr = avfilter_graph_dump(m_filterGraph, nullptr);
    //LOG(INFO) << m_logtag << "filter graph\n" << dumpGraphStr;
    //if (dumpGraphStr)
    //    av_free((void *)dumpGraphStr);
    return ret;
}

int FilterGeneral::close() {
    if (m_filterGraph) {
        avfilter_graph_free(&m_filterGraph); // related filters will be set null
    }
    // release anyway
    avfilter_inout_free(&m_inputs);
    avfilter_inout_free(&m_outputs);
    return 0;
}

int FilterGeneral::prepareBufferSrc() {
    // m_bufsrcParams
    char srcArgs[512] = {0};
    if (m_fgp.m_inMediaType == AVMEDIA_TYPE_VIDEO) {
        snprintf(srcArgs, sizeof(srcArgs), "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
                 m_fgp.m_bufsrcParams->width, m_fgp.m_bufsrcParams->height, m_fgp.m_bufsrcParams->format,
                 m_fgp.m_bufsrcParams->time_base.num, m_fgp.m_bufsrcParams->time_base.den,
                 m_fgp.m_bufsrcParams->sample_aspect_ratio.num,
                 m_fgp.m_bufsrcParams->sample_aspect_ratio.den);
    } else {
        snprintf(srcArgs, sizeof(srcArgs),
                 "time_base=%d/%d:sample_fmt=%s:sample_rate=%d:channel_layout=%llu",
                 m_fgp.m_bufsrcParams->time_base.num, m_fgp.m_bufsrcParams->time_base.den,
                 av_get_sample_fmt_name((enum AVSampleFormat)m_fgp.m_bufsrcParams->format),
                 m_fgp.m_bufsrcParams->sample_rate, m_fgp.m_bufsrcParams->channel_layout);
    }

    LOG(INFO) << m_logtag << "filter in args: " << srcArgs;
    const char* bufsrcName = m_fgp.m_inMediaType == AVMEDIA_TYPE_VIDEO ? "buffer" : "abuffer";
    const AVFilter *bufsrc = avfilter_get_by_name(bufsrcName);
    CHECK(bufsrc != nullptr) << m_logtag << " cannot find buffersrc filter of name " << bufsrcName;
    int ret = avfilter_graph_create_filter(&m_srcCtx, bufsrc, "in", srcArgs, nullptr, m_filterGraph);
    if (ret < 0)
        return ret;

    av_buffersrc_parameters_set(m_srcCtx, m_fgp.m_bufsrcParams.get());
    return 0;
}

int FilterGeneral::prepareBufferSink() {
    const char* bufsinkName = m_fgp.m_outMediaType == AVMEDIA_TYPE_VIDEO ? "buffersink" : "abuffersink";
    const AVFilter *bufsink = avfilter_get_by_name(bufsinkName);
    CHECK(bufsink != nullptr) << m_logtag << " cannot find buffer sink filter of name " << bufsinkName;
    int ret = avfilter_graph_create_filter(&m_sinkCtx, bufsink, "out", nullptr, nullptr, m_filterGraph);
    if (ret < 0)
        return ret;

    if (!m_fgp.m_bufsinkParams && !m_fgp.m_abufsinkParams)
        return 0;

    if (m_fgp.m_outMediaType == AVMEDIA_TYPE_VIDEO && m_fgp.m_bufsinkParams) {
        ret = av_opt_set_int_list(m_sinkCtx, "pix_fmts", m_fgp.m_bufsinkParams->pixel_fmts,
                                  AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN);
        if (ret < 0)
            return ret;
    } else if (m_fgp.m_outMediaType == AVMEDIA_TYPE_AUDIO && m_fgp.m_abufsinkParams) {
        ret = av_opt_set_int_list(m_sinkCtx, "sample_fmts", m_fgp.m_abufsinkParams->sample_fmts, -1,
                                  AV_OPT_SEARCH_CHILDREN);
        if (ret < 0)
            return ret;
        ret = av_opt_set_int_list(m_sinkCtx, "channel_layouts",
                                  m_fgp.m_abufsinkParams->channel_layouts, -1, AV_OPT_SEARCH_CHILDREN);
        if (ret < 0)
            return ret;
        ret = av_opt_set_int_list(m_sinkCtx, "sample_rates",
                                  m_fgp.m_abufsinkParams->sample_rates, -1, AV_OPT_SEARCH_CHILDREN);
        if (ret < 0)
            return ret;
    }
    return 0;
}

////////////////////////////
// [process: send - receive]

int FilterGeneral::sendFilterFrame(AVFrame *frame, int srcFlags) {
    /* push input frame into the filtergraph. null frame input means EOF */
    int ret = av_buffersrc_add_frame_flags(m_srcCtx, frame, srcFlags);
    if (ret < 0)
        return ret;
    //if (srcFlags & AV_BUFFERSRC_FLAG_KEEP_REF)
    //    av_frame_unref(frame);
    return ret;
}

int FilterGeneral::
receiveFilterFrames(vector<shared_ptr<AVFrame>> & filterFrames, int sinkFlags) {
    int ret = 0;
    /* pull filtered frames from the filtergraph */
    do {
        shared_ptr<AVFrame> filterFrame(av_frame_alloc(), [](AVFrame *p){av_frame_free(&p);});
        CHECK(filterFrame != nullptr);
        ret = av_buffersink_get_frame(m_sinkCtx, filterFrame.get());
        if (ret == AVERROR(EAGAIN))
            return 0;
        else if (ret == AVERROR_EOF) {
            LOG(INFO) << m_logtag << "receive eof, flush done";
            return ret;
        }
        else if (ret < 0) // ret == AVERROR_EOF as flush done
            return ret;
        filterFrames.push_back(filterFrame);
     } while (true);
     return 0;
}

} // namespace ff_dynamic




/*
AVBufferSrcParameters *par = av_buffersrc_parameters_alloc();

    if (!par)
        return AVERROR(ENOMEM);
    memset(par, 0, sizeof(*par));
    par->format = AV_PIX_FMT_NONE;

    if (ist->dec_ctx->codec_type == AVMEDIA_TYPE_AUDIO) {
        av_log(NULL, AV_LOG_ERROR, "Cannot connect video filter to audio input\n");
        ret = AVERROR(EINVAL);
        goto fail;
    }

    if (!fr.num)
        fr = av_guess_frame_rate(input_files[ist->file_index]->ctx, ist->st, NULL);

    if (ist->dec_ctx->codec_type == AVMEDIA_TYPE_SUBTITLE) {
        ret = sub2video_prepare(ist, ifilter);
        if (ret < 0)
            goto fail;
    }

    sar = ifilter->sample_aspect_ratio;
    if(!sar.den)
        sar = (AVRational){0,1};
    av_bprint_init(&args, 0, AV_BPRINT_SIZE_AUTOMATIC);
    av_bprintf(&args,
             "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:"
             "pixel_aspect=%d/%d:sws_param=flags=%d",
             ifilter->width, ifilter->height, ifilter->format,
             tb.num, tb.den, sar.num, sar.den,
             SWS_BILINEAR + ((ist->dec_ctx->flags&AV_CODEC_FLAG_BITEXACT) ? SWS_BITEXACT:0));
    if (fr.num && fr.den)
        av_bprintf(&args, ":frame_rate=%d/%d", fr.num, fr.den);
    snprintf(name, sizeof(name), "graph %d input from stream %d:%d", fg->index,
             ist->file_index, ist->st->index);


    if ((ret = avfilter_graph_create_filter(&ifilter->filter, buffer_filt, name,
                                            args.str, NULL, fg->graph)) < 0)
        goto fail;
    par->hw_frames_ctx = ifilter->hw_frames_ctx;
    ret = av_buffersrc_parameters_set(ifilter->filter, par);
    if (ret < 0)
        goto fail;
    av_freep(&par);

    // PP - for hw_ctx
    if (hw_ref != NULL) {
               AVBufferSrcParameters *par = av_buffersrc_parameters_alloc();
               memset(par, 0, sizeof(*par));
             par->format = AV_PIX_FMT_NONE;
             par->hw_frames_ctx = hw_ref;
             av_buffersrc_parameters_set(buffersrc_ctx, par);
             av_freep(&par);
 */
