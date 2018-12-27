#include <iostream>
#include <fstream>

#include "frameMat.h"
#include "davImplTravel.h"
#include "cvDnnDetect.h"

namespace ff_dynamic {
/////////////////////////////////
// [Register - auto, cvDnnDetect] There is no data output but only peer events
static DavImplRegister s_cvDnnDetectReg(DavWaveClassObjDetect(), vector<string>({"auto", "cvDnnDetect"}), {},
                                        [](const DavWaveOption & options) -> unique_ptr<DavImpl> {
                                            unique_ptr<CvDnnDetect> p(new CvDnnDetect(options));
                                            return p;
                                        });

const DavRegisterProperties & CvDnnDetect::getRegisterProperties() const noexcept {
    return s_cvDnnDetectReg.m_properties;
}

////////////////////////////////////
//  [event process]
int CvDnnDetect::processChangeConfThreshold(const DynaEventChangeConfThreshold & e) {
    m_dps.m_confThreshold = e.m_confThreshold;
    return 0;
}

////////////////////////////////////
//  [construct - destruct - process]
int CvDnnDetect::onConstruct() {
    LOG(INFO) << m_logtag << "start creating CvDnnDetect " << m_options.dump();
    std::function<int (const DynaEventChangeConfThreshold &)> f =
        [this] (const DynaEventChangeConfThreshold & e) {return processChangeConfThreshold(e);};
    m_implEvent.registerEvent(f);

    m_dps.m_detectOrClassify = m_options.get("detect_or_classify");
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

    try {
        m_net = cv::dnn::readNet(m_dps.m_modelPath, m_dps.m_configPath, m_dps.m_detectorFrameworkTag);
    } catch (const std::exception & e) {
        string detail = "Fail create cv dnn net model. model path " + m_dps.m_modelPath +
            ", confit path " +  m_dps.m_configPath;
        ERRORIT(DAV_ERROR_IMPL_ON_CONSTRUCT, detail);
        return DAV_ERROR_IMPL_ON_CONSTRUCT;
    }
    m_net.setPreferableBackend(m_dps.m_backendId);
    m_net.setPreferableTarget(m_dps.m_targetId);

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
    LOG(INFO) << m_logtag << "CvDnnDetect create done: " << m_dps;
    return 0;
}

int CvDnnDetect::onDestruct() {
    LOG(INFO) << m_logtag << "CvDnnDetect Destruct";
    return 0;
}

////////////////////////////////////
//  [dynamic initialization]

int CvDnnDetect::onDynamicallyInitializeViaTravelStatic(DavProcCtx & ctx) {
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
    /* output no data stream, so no output travel static info */
    m_bDynamicallyInitialized = true;
    return 0;
}

////////////////////////////

int CvDnnDetect::onProcess(DavProcCtx & ctx) {
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
        LOG(INFO) << m_logtag << "cv dnn detector reciving flush frame";
        ctx.m_bInputFlush = true;
        /* no flush needed, so just return EOF */
        return AVERROR_EOF;
    }

    // convert this frame to opencv Mat
    cv::Mat yuvMat;
    FrameMat::frameToMatYuv420(inFrame, yuvMat);
    cv::Mat image, blob;
    if (m_dps.m_bSwapRb)
        cv::cvtColor(yuvMat, image, CV_YUV2RGB_I420);
    else
        cv::cvtColor(yuvMat, image, CV_YUV2BGR_I420);
    cv::Size inpSize(m_dps.m_width > 0 ? m_dps.m_width : image.cols,
                     m_dps.m_height > 0 ? m_dps.m_height : image.rows);
    cv::dnn::blobFromImage(image, blob, m_dps.m_scaleFactor, inpSize, m_means, false, false);
    m_net.setInput(blob);
    if (m_net.getLayer(0)->outputNameToIndex("im_info") != -1) { // Faster-RCNN or R-FCN
        resize(image, image, inpSize);
        cv::Mat imInfo = (cv::Mat_<float>(1, 3) << inpSize.height, inpSize.width, 1.6f);
        m_net.setInput(imInfo, "im_info");
    }

    /* prepare output events*/
    auto detectEvent = make_shared<ObjDetectEvent>();
    if (m_dps.m_detectOrClassify == "classify") {
        cv::Mat prob = m_net.forward();
        cv::Point classIdPoint;
        double confidence;
        cv::minMaxLoc(prob.reshape(1, 1), 0, &confidence, 0, &classIdPoint);
        int classId = classIdPoint.x;
        ObjDetectEvent::DetectResult result;
        result.m_confidence = confidence;
        result.m_className = m_classNames[classId];
        detectEvent->m_results.emplace_back(result);
        LOG(INFO) << m_logtag << "classification with confidence "
                  <<  confidence << ", classId " << classId << ", name " << m_classNames[classId];
    } else if (m_dps.m_detectOrClassify == "detect") {
        std::vector<cv::Mat> outs;
        m_net.forward(outs, getOutputsNames());
        postprocess(image, outs, detectEvent);
    } else {
        LOG(ERROR) << m_logtag << "Fail: not detect or classify " + m_dps.m_detectOrClassify;
        return AVERROR(EINVAL);
    }
    const double freq = cv::getTickFrequency() / 1000;
    vector<double> layersTimes;
    detectEvent->m_inferTime = m_net.getPerfProfile(layersTimes) / freq;
    detectEvent->m_detectOrClassify = m_dps.m_detectOrClassify;
    detectEvent->m_detectorFrameworkTag = m_dps.m_detectorFrameworkTag;
    detectEvent->m_framePts = inFrame->pts;
    ctx.m_pubEvents.emplace_back(detectEvent);
    /* No travel static needed for detectors, just events */
    return 0;
}

////////////
// [helpers]
int CvDnnDetect::postprocess(const cv::Mat & image, const vector<cv::Mat> & outs,
                             shared_ptr<ObjDetectEvent> & detectEvent) {
    /* result assign */
    vector<int> outLayers = m_net.getUnconnectedOutLayers();
    string outLayerType = m_net.getLayer(outLayers[0])->type;
    if (m_net.getLayer(0)->outputNameToIndex("im_info") != -1) { // Faster-RCNN or R-FCN
        // Network produces output blob with a shape 1x1xNx7 where N is a number of detections.
        // each detection is: [batchId, classId, confidence, left, top, right, bottom]
        CHECK(outs.size() == 1) << "Faster-Rcnn or R-FCN output should have size 1";
        const float* data = (float*)outs[0].data;
        for (size_t i=0; i < outs[0].total(); i+=7) {
            ObjDetectEvent::DetectResult result;
            result.m_confidence = data[i + 2];
            if (result.m_confidence > m_dps.m_confThreshold) {
                result.m_rect.x = (int)data[i + 3];
                result.m_rect.y = (int)data[i + 4];
                const int right = (int)data[i + 5];
                const int bottom = (int)data[i + 6];
                result.m_rect.w = right - result.m_rect.x + 1;
                result.m_rect.h = bottom - result.m_rect.y + 1;
                const int classId = (int)(data[i + 1]) - 1; // classId 0 is background
                if (classId < (int)m_classNames.size())
                    result.m_className = m_classNames[classId];
                detectEvent->m_results.emplace_back(result);
            }
        }
    } else if (outLayerType == "DetectionOutput") {
        // Network produces output blob with a shape 1x1xNx7 where N is a number of detections。
        // each detection: [batchId, classId, confidence, left, top, right, bottom]
        CHECK(outs.size() == 1) << "DetectionOutput should have size one";
        const float* data = (float*)outs[0].data;
        for (size_t i=0; i < outs[0].total(); i+=7) {
            ObjDetectEvent::DetectResult result;
            result.m_confidence = data[i + 2];
            if (result.m_confidence > m_dps.m_confThreshold) {
                result.m_rect.x = (int)(data[i + 3] * image.cols);
                result.m_rect.y = (int)(data[i + 4] * image.rows);
                const int right = (int)(data[i + 5] * image.cols);
                const int bottom = (int)(data[i + 6] * image.rows);
                result.m_rect.w = right - result.m_rect.x + 1;
                result.m_rect.h = bottom - result.m_rect.y + 1;
                const int classId = (int)(data[i + 1]) - 1; // classId 0 is background
                if (classId < (int)m_classNames.size())
                    result.m_className = m_classNames[classId];
                detectEvent->m_results.emplace_back(result);
            }
        }
    } else if (outLayerType == "Region") {
        for (size_t i = 0; i < outs.size(); ++i) {
            // Network produces output blob with a shape NxC where N is a number of detected objects
            // and C is a number of classes + 4; 4 numbers are [center_x, center_y, width, height]
            ObjDetectEvent::DetectResult result;
            float* data = (float*)outs[i].data;
            for (int j = 0; j < outs[i].rows; ++j, data += outs[i].cols) {
                cv::Mat scores = outs[i].row(j).colRange(5, outs[i].cols);
                cv::Point classIdPoint;
                minMaxLoc(scores, 0, &result.m_confidence, 0, &classIdPoint);
                if (result.m_confidence > m_dps.m_confThreshold) {
                    int centerX = (int)(data[0] * image.cols);
                    int centerY = (int)(data[1] * image.rows);
                    result.m_rect.w = (int)(data[2] * image.cols);
                    result.m_rect.h = (int)(data[3] * image.rows);
                    result.m_rect.x = centerX - result.m_rect.w / 2;
                    result.m_rect.y = centerY - result.m_rect.h / 2;
                    if (classIdPoint.x < (int)m_classNames.size())
                        result.m_className = m_classNames[classIdPoint.x];
                    detectEvent->m_results.emplace_back(result);
                }
            }
        }
    } else {
        LOG(ERROR) << m_logtag << "Unknown output layer type: " << outLayerType;
        return -1;
    }
    return 0;
}

const vector<cv::String> & CvDnnDetect::getOutputsNames() {
    if (m_outBlobNames.empty()) {
        vector<int> outLayers = m_net.getUnconnectedOutLayers();
        vector<cv::String> layersNames = m_net.getLayerNames();
        m_outBlobNames.resize(outLayers.size());
        for (size_t i = 0; i < outLayers.size(); ++i)
            m_outBlobNames[i] = layersNames[outLayers[i] - 1];
    }
    return m_outBlobNames;
}

} // namespace ff_dynamic
