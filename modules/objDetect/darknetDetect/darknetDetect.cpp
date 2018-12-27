#include <math.h>
#include <iostream>
#include <fstream>
#include "frameMat.h"
#include "davImplTravel.h"
#include "darknetDetect.h"

namespace ff_dynamic {
///////////////////////////////////
// [Register - DarknetDetect] There is no data output, only peer events fro DarknetDetect
static DavImplRegister s_darknetDetectReg(DavWaveClassObjDetect(), vector<string>({"darknetDetect"}), {},
                                        [](const DavWaveOption & options) -> unique_ptr<DavImpl> {
                                            unique_ptr<DarknetDetect> p(new DarknetDetect(options));
                                            return p;
                                        });

const DavRegisterProperties & DarknetDetect::getRegisterProperties() const noexcept {
    return s_darknetDetectReg.m_properties;
}

/////////////////
// static helpers
static void set_pixel(image m, int x, int y, int c, float val)
{
    if (x < 0 || y < 0 || c < 0 || x >= m.w || y >= m.h || c >= m.c) return;
    assert(x < m.w && y < m.h && c < m.c);
    m.data[c*m.h*m.w + y*m.w + x] = val;
}

static image yuv420pToDarknetRgb(AVFrame *frame) {
    image im;
    im.c = 3;
    im.w = frame->width;
    im.h = frame->height;
    im.data = (float *)calloc(im.w * im.h * im.c, sizeof(float));
    int i, j;
    float r, g, b;
    float y, u, v;
    for (j = 0; j < frame->height; ++j) {
        for (i = 0; i < frame->width; ++i) {
            y = frame->data[0][j * frame->height + i];
            u = frame->data[1][j * frame->height / 2 + i / 2];
            v = frame->data[2][j * frame->height / 2 + i / 2];
            r = y + 1.13983*v;
            g = y + -.39465*u + -.58060*v;
            b = y + 2.03211*u;
            set_pixel(im, i, j, 0, r);
            set_pixel(im, i, j, 1, g);
            set_pixel(im, i, j, 2, b);
        }
    }
    return im;
}

////////////////////////////////////
//  [event process]
int DarknetDetect::processChangeConfThreshold(const DynaEventChangeConfThreshold & e) {
    m_dps.m_confThreshold = e.m_confThreshold;
    return 0;
}

////////////////////////////////////
//  [construct - destruct - process]

int DarknetDetect::onConstruct() {
    LOG(INFO) << m_logtag << "start creating DarknetDetect " << m_options.dump();

    std::function<int (const DynaEventChangeConfThreshold &)> f =
        [this] (const DynaEventChangeConfThreshold & e) {return processChangeConfThreshold(e);};
    m_implEvent.registerEvent(f);

    m_dps.m_detectOrClassify = m_options.get("detect_or_classify");
    if (m_dps.m_detectOrClassify != "detect") {
        ERRORIT(DAV_ERROR_IMPL_ON_CONSTRUCT,
                "darknet integration only do detection for now " + m_dps.m_detectOrClassify);
        return DAV_ERROR_IMPL_ON_CONSTRUCT;
    }
    m_dps.m_detectorFrameworkTag = m_options.get("detector_framework_tag");
    m_dps.m_modelPath = m_options.get("model_path");
    m_dps.m_configPath = m_options.get("config_path");
    m_dps.m_classnamePath = m_options.get("classname_path");
    m_options.getInt("backend_id", m_dps.m_backendId);
    m_options.getInt("target_id", m_dps.m_targetId);
    m_options.getDouble("scale_factor", m_dps.m_scaleFactor);
    m_options.getBool("swap_rb", m_dps.m_bSwapRb);
    m_options.getInt("width", m_dps.m_width);
    m_options.getInt("height", m_dps.m_height);
    m_options.getDouble("conf_threshold", m_dps.m_confThreshold);
    m_options.getInt("detect_interval", m_dps.m_detectInterval);
    vector<double> means;
    m_options.getDoubleArray("means", means);
    if (means.size() == 3) {
        m_means = cv::Scalar{means[0], means[1], means[2]};
    }

    m_net = load_network((char *)m_dps.m_configPath.c_str(), (char *)m_dps.m_modelPath.c_str(), 0);
    if (!m_net) {
        string detail = "Fail load darknet detect model. model path " + m_dps.m_modelPath +
            ", confit path " +  m_dps.m_configPath;
        ERRORIT(DAV_ERROR_IMPL_ON_CONSTRUCT, detail);
        return DAV_ERROR_IMPL_ON_CONSTRUCT;
    }

    /* network settings before predict */
    if (m_dps.m_targetId >= 0) { /* nvidia gpu index */
        // TODO: this is awkward, darknet use global gpu index. Should put it inside 'network' structure
        // cuda_set_device(m_dps.m_targetId);
    }

    if ((m_net->w != m_dps.m_width) || (m_net->h != m_dps.m_height)) {
        LOG(WARNING) << m_logtag << "config WxH not the same with net's WxH. will use net's. "
                     << m_dps.m_width << "x" << m_dps.m_height << ", " << m_net->w << "X" << m_net->h;
        m_dps.m_width = m_net->w;
        m_dps.m_height = m_net->h;
    }
    /* set batch */
    set_batch_network(m_net, 1);

    if (!m_dps.m_classnamePath.empty()) {
        std::ifstream ifs(m_dps.m_classnamePath.c_str());
        if (!ifs.is_open()) {
            ERRORIT(DAV_ERROR_IMPL_ON_CONSTRUCT, "fail to open class name file " + m_dps.m_classnamePath);
            return DAV_ERROR_IMPL_ON_CONSTRUCT;
        }
        std::string line;
        while (std::getline(ifs, line)) {
            m_classNames.emplace_back(line);
        }
    }
    LOG(INFO) << m_logtag << "DarknetDetect create done: " << m_dps;
    return 0;
}

int DarknetDetect::onDestruct() {
    if (m_net) {
        free_network(m_net);
        m_net = nullptr;
    }
    LOG(INFO) << m_logtag << "DarknetDetect Destruct";
    return 0;
}

////////////////////////////////////
//  [dynamic initialization]

int DarknetDetect::onDynamicallyInitializeViaTravelStatic(DavProcCtx & ctx) {
    auto in = m_inputTravelStatic.at(ctx.m_froms[0]);
    if (!in->m_codecpar && (in->m_pixfmt == AV_PIX_FMT_NONE)) {
        ERRORIT(DAV_ERROR_TRAVEL_STATIC_INVALID_CODECPAR,
                m_logtag + "dehaze cannot get valid codecpar or videopar");
        return DAV_ERROR_TRAVEL_STATIC_INVALID_CODECPAR;
    }
    /* set output infos */
    m_timestampMgr.clear();
    m_outputTravelStatic.clear();
    m_timestampMgr.insert(std::make_pair(ctx.m_froms[0], DavImplTimestamp(in->m_timebase, in->m_timebase)));
    /* no data stream output, so no output travel static info */
    m_bDynamicallyInitialized = true;
    return 0;
}

////////////////////////////
int DarknetDetect::onProcess(DavProcCtx & ctx) {
    ctx.m_expect.m_expectOrder = {EDavExpect::eDavExpectAnyOne};
    if (!ctx.m_inBuf)
        return 0;

    /* detect with interval (frame skip) */
    m_inputCount++;
    const bool bSkipDetect = (m_inputCount % m_dps.m_detectInterval) == 0 ? false : true;
    if (bSkipDetect)
        return 0;

    int ret = 0;
    /* ref frame is a frame ref to original frame (data shared),
       but timestamp is convert to current impl's timebase */
    auto inFrame = ctx.m_inRefFrame;
    if (!inFrame) {
        LOG(INFO) << m_logtag << "darknet detector reciving flush frame";
        ctx.m_bInputFlush = true;
        /* no flush needed, so just return EOF */
        return AVERROR_EOF;
    }

    /* load inFrame and do predict. inFrame in format of yuv420p */
    const int imageWidth = inFrame->width;
    const int imageHeight = inFrame->height;
    image inImage = yuv420pToDarknetRgb(inFrame);
    if ((imageWidth != m_net->w) || (imageHeight != m_net->h)) {
        image resized = resize_image(inImage, m_net->w, m_net->h);
        free_image(inImage);
        inImage = resized;
    }

    /* do predict */
    float *X = inImage.data;
    clock_t startTime = clock();
    srand(lround(startTime));
    network_predict(m_net, X);
    const double inferTime = sec(clock() - startTime);
    LOG(INFO) << "Predicted time " << inferTime  << " seconds";

    /* prepare output events*/
    layer l = m_net->layers[m_net->n-1];
    int nboxes = 0;
    detection *dets = get_network_boxes(m_net, 1, 1, m_dps.m_confThreshold, 0, nullptr, 0, &nboxes);
    const int num = l.side * l.side * l.n;

    auto detectEvent = make_shared<ObjDetectEvent>();
    for (int i = 0; i < num; ++i) {
        for (int j = 0; j < (int)m_classNames.size(); ++j) {
            ObjDetectEvent::DetectResult result;
            if (dets[i].prob[j] > m_dps.m_confThreshold) {
                result.m_confidence = dets[i].prob[j];
                result.m_className = m_classNames[j];
                const int left  = (dets[i].bbox.x - dets[i].bbox.w / 2.0) * imageWidth;
                const int right = (dets[i].bbox.x + dets[i].bbox.w / 2.0) * imageWidth;
                const int top   = (dets[i].bbox.y - dets[i].bbox.h / 2.0) * imageHeight;
                const int bot   = (dets[i].bbox.y + dets[i].bbox.h / 2.0) * imageHeight;
                result.m_rect.x = left < 0 ? 0 : left;
                result.m_rect.y = top < 0 ? 0 : top;
                result.m_rect.w = (right >= imageWidth ? imageWidth : right) - result.m_rect.x;
                result.m_rect.h = result.m_rect.y - (bot >= imageHeight ? imageHeight : bot);
                detectEvent->m_results.emplace_back(result);
            }
        }
    }

    free_detections(dets, nboxes);
    free_image(inImage);

    detectEvent->m_inferTime = inferTime;
    detectEvent->m_detectOrClassify = m_dps.m_detectOrClassify;
    detectEvent->m_detectorFrameworkTag = m_dps.m_detectorFrameworkTag;
    detectEvent->m_framePts = inFrame->pts;
    ctx.m_pubEvents.emplace_back(detectEvent);
    /* No travel static needed for detectors, just events */
    return 0;
}

} // namespace ff_dynamic
