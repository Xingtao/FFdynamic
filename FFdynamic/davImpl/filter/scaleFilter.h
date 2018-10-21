#pragma once

#include <string>
#include <vector>
#include <memory>
#include <mutex>

#include "davUtil.h"
#include "filterGeneral.h"

namespace ff_dynamic {
using ::std::string;
using ::std::vector;
using ::std::shared_ptr;

struct ScaleFilterParams {
    enum AVPixelFormat m_inFormat = AV_PIX_FMT_NONE;
    int m_inWidth = 0;
    int m_inHeight = 0;
    AVRational m_inTimebase = {0, 1};
    AVRational m_inFramerate = {0, 1};
    AVRational m_inSar = {0, 1};

    int m_outWidth = 0;
    int m_outHeight = 0;
    AVRational m_outTimebase = {0, 1};
    AVRational m_outFramerate = {0, 1};

    AVBufferRef *m_hwFramesCtx = nullptr;
    bool m_bFpsScale = false; /* whether do fps conversion */
    string m_logtag;
};

extern std::ostream & operator<<(std::ostream & os, const ScaleFilterParams & p);
extern bool operator==(const ScaleFilterParams & l, const ScaleFilterParams & r);

/* it is for video mix and video encode only, not a general purpose class */
/* 1) spatial and temporal scale 2) keep the original pixel format 3) not thread safe */
class ScaleFilter {
public:
    ScaleFilter() = default;
    virtual ~ScaleFilter() = default;
    int initScaleFilter(const ScaleFilterParams & fbp);
    int close();
    int sendFrame(AVFrame *inFrame);
    int receiveFrames(vector<shared_ptr<AVFrame>> & scaleFrames);
    const ScaleFilterParams & getScaleFilterParams() const {return m_sfp;}
    const vector<shared_ptr<AVFrame>> & queryReadyFrames() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_scaledFrames;
    }

private:
    string m_logtag = "[ScaleFilter] ";
    ScaleFilterParams m_sfp;
    FilterGeneral m_filterGeneral;
    vector<shared_ptr<AVFrame>> m_scaledFrames;
    bool m_bFlushDone = false;
    std::mutex m_mutex;
};

    // 1. av_hwframe_ctx_alloc, ref previous device ref to allocate new hw frame context
    //AVBufferRef *av_hwframe_ctx_alloc(AVBufferRef *device_ref_in)
    //{
    //AVHWDeviceContext *device_ctx = (AVHWDeviceContext*)device_ref_in->data;
    //const HWContextType  *hw_type = device_ctx->internal->hw_type;
    //AVHWFramesContext *ctx;
    //AVBufferRef *buf, *device_ref = NULL;
    //
    //ctx = av_mallocz(sizeof(*ctx));
    //if (!ctx)
    //    return NULL;
    //

    // 2. under this new hw frame ctx, allocate new  hw frame
    // av_hwframe_get_buffer from hwcontext.h to allocate new hw frame;

    // 3. for copy part
    // device -> to device copy, how to do this using api? or we write one

    // av_hwframe_transfer_data could be transfer data from src -> dst, with rectangle, but padding content
    // not specified.

    // "stride" are specified via frame->linesize[]

} // namespace ff_dynamic
