#include "dynaDetectorService.h"

namespace dyna_detect_service {

////////////////////////////////////////////////////////////////////////////////
static const std::map<int, const char *> dynaDetectServiceErrorCode2StrMap = {
    {DYNA_DETECT_ERROR_TASK_INIT,                "failed create task from config file"},
    {DYNA_DETECT_ERROR_DETECTOR_ALREADY_RUNNING, "detector already enabled"},
    {DYNA_DETECT_ERROR_DETECTOR_ALREADY_STOPPED, "detector already stopped"},
    {DYNA_DETECT_ERROR_DETECTOR_ADD,             "add detector failed"},
    {DYNA_DETECT_ERROR_DETECTOR_DELETE,          "delete detector failed"},
    {DYNA_DETECT_ERROR_NO_SUCH_DETECTOR,         "no such dnn detector"}
};

string dynaDetectServiceErr2Str(const int errCode) {
    auto it = std::find_if(dynaDetectServiceErrorCode2StrMap.begin(), dynaDetectServiceErrorCode2StrMap.end(),
                           [errCode](const std::pair<int, const char *> & item) {
                               return item.first == errCode;});
    if (it != dynaDetectServiceErrorCode2StrMap.end())
        return string(it->second);
    return appServiceErr2Str(errCode);
}

///////////////////////////////////
// [Http Request Return Error Code]
static constexpr int API_ERRCODE_INVALID_MSG = 1;
static constexpr int API_ERRCODE_DETECTOR_ALREADY_RUNNING = 2;
static constexpr int API_ERRCODE_DETECTOR_ALREADY_STOPPED = 3;
static constexpr int API_ERRCODE_NO_SUCH_DETECTOR = 4;
static constexpr int API_ERRCODE_ADD_DETECTOR = 4;

////////////////////////////////////////////////
int DynaDetectService::createTask() {
    int ret = buildOutputStreamlet("output_dyna_detect", m_outputSetting,
                                   {m_dynaDetectGlobalSetting.output_url()});
    if (ret < 0) {
        m_river.clear();
        ERRORIT(DYNA_DETECT_ERROR_TASK_INIT,  "fail create output streamlet " + m_appInfo.m_msgDetail);
        return DYNA_DETECT_ERROR_TASK_INIT;
    }
    LOG(INFO) << m_logtag << "craete output streamlet done";

    /* 2. build dnn detect streamlet */
    ret = buildDynaDetectStreamlet();
    if (ret < 0) {
        m_river.clear();
        ERRORIT(DYNA_DETECT_ERROR_TASK_INIT,  "fail create dyna detect streamlet " + m_appInfo.m_msgDetail);
        return DYNA_DETECT_ERROR_TASK_INIT;
    }
    LOG(INFO) << m_logtag << "craete dnn streamlet done";
    auto detectStreamlet = m_river.get(ObjDetectStreamletTag(m_dnnDetectStreamletName));
    auto outputStreamlets = m_river.getStreamletsByCategory(DavDefaultOutputStreamletTag());
    for (auto & o : outputStreamlets)
        detectStreamlet >= o;

    /* 3. async build input streamlet with callback that connect to mix streamlet */
    auto onBuildSuccess = [this] (shared_ptr<DavStreamlet> inputStreamlet) {
        if (!inputStreamlet)
            return AVERROR(EINVAL);
        auto detectStreamlet = m_river.get(ObjDetectStreamletTag(m_dnnDetectStreamletName));
        if (!detectStreamlet)
            return AVERROR(EINVAL);
        inputStreamlet >= detectStreamlet; /* only connect video */
        return inputStreamlet->start(); /*start just after connect */
    };
    asyncBuildInputStreamlet(m_dynaDetectGlobalSetting.input_url(), m_inputSetting, onBuildSuccess);

    /* 4. river start */
    m_river.start();
    LOG(INFO) << m_logtag << "dynamic dnn detect task started";
    return 0;
}

/////////////////////////////////////////
// [Event Process]
int DynaDetectService::onAddOneDetector(shared_ptr<Response> & response, const string & detectorName) {
    std::lock_guard<mutex> lock(m_runLock);
    if (!m_detectors.count(detectorName)) {
        string detail = detectorName + " is not in configure file";
        ERRORIT(DYNA_DETECT_ERROR_NO_SUCH_DETECTOR, detail);
        return failResponse(response, API_ERRCODE_NO_SUCH_DETECTOR, detail);
    }
    if (m_detectors.at(detectorName)) {
        string detail = detectorName + " is alrady enabled";
        ERRORIT(DYNA_DETECT_ERROR_DETECTOR_ALREADY_RUNNING, detail);
        return failResponse(response, API_ERRCODE_DETECTOR_ALREADY_RUNNING, detail);
    }
    m_detectors.at(detectorName) = true;
    auto detectStreamlet = m_river.get(ObjDetectStreamletTag(m_dnnDetectStreamletName));
    CHECK(detectStreamlet != nullptr);
    DavWaveOption o;
    PbDnnDetectSettingToDavOption::toDnnDetectOption(m_dnnDetectorSettings.at(detectorName), o, detectorName);
    ObjDetectStreamletBuilder builder;
    int ret = builder.addDetector(detectStreamlet, o);
    if (ret < 0) {
        string detail = detectorName + " adding failed";
        ERRORIT(DYNA_DETECT_ERROR_DETECTOR_ADD, detail);
        return failResponse(response, API_ERRCODE_ADD_DETECTOR, detail);
    }
    response->write(m_successCRJsonStr);
    return 0;
}

int DynaDetectService::onDeleteOneDetector(shared_ptr<Response> & response, const string & detectorName) {
    std::lock_guard<mutex> lock(m_runLock);
    if (!m_detectors.count(detectorName)) {
        string detail = detectorName + " is not in configure file";
        ERRORIT(DYNA_DETECT_ERROR_NO_SUCH_DETECTOR, detail);
        return failResponse(response, API_ERRCODE_NO_SUCH_DETECTOR, detail);
    }
    if (!m_detectors.at(detectorName)) {
        string detail = detectorName + " is alrady stopped";
        ERRORIT(DYNA_DETECT_ERROR_DETECTOR_ALREADY_STOPPED, detail);
        return failResponse(response, API_ERRCODE_DETECTOR_ALREADY_STOPPED, detail);
    }
    m_detectors.at(detectorName) = false;
    auto detectStreamlet = m_river.get(ObjDetectStreamletTag(m_dnnDetectStreamletName));
    auto wave = detectStreamlet->getWave(DavWaveClassObjDetect(detectorName));
    CHECK(detectStreamlet != nullptr && wave != nullptr);
    detectStreamlet->deleteOneWave(wave);
    LOG(INFO) << m_logtag << "delete one detector " << detectorName;
    // find and delete that detector
    response->write(m_successCRJsonStr);
    return 0;
}

//////////////////
/* dynaDetect stop */
int DynaDetectService::onDynaDetectStop(shared_ptr<Response> & response, shared_ptr<Request> & request) {
    LOG(INFO) << m_logtag << "receive dynaDetect stop request";
    response->write(m_successCRJsonStr);
    m_river.stop();
    m_river.clear();
    DynaDetectService::setExit();
    return 0;
}

////////////////////////////////////////////////////////////////////////////////
// [Register Http Dynamic On Handler]

int DynaDetectService::registerHttpRequestHandlers() {
    m_httpServer.m_resources["^/api1/detector/[a-zA-Z0-9_]+"]["POST"] =
        [this] (shared_ptr<Response> & response, shared_ptr<Request> & request) {
        const string & queryPath = request->m_path;
        std::size_t found = queryPath.find_last_of("/");
        const string detectorName(queryPath.substr(found+1));
        LOG(INFO) << m_logtag << "will add " << queryPath << " " << detectorName;
        return onAddOneDetector(response, detectorName);
    };
    m_httpServer.m_resources["^/api1/detector/[a-zA-Z0-9_]+"]["DELETE"] =
        [this] (shared_ptr<Response> & response, shared_ptr<Request> & request) {
        const string & queryPath = request->m_path;
        std::size_t found = queryPath.find_last_of("/");
        const string detectorName(queryPath.substr(found+1));
        LOG(INFO) << m_logtag << "will delete " << queryPath << " " << detectorName;
        return onDeleteOneDetector(response, detectorName);
    };

    /* stop */
    m_httpServer.m_resources["^/api1/stop$"]["POST"] =
        [this](shared_ptr<Response> & response, shared_ptr<Request> & request) {
        return onDynaDetectStop(response, request);
    };

    /* onerror */
    m_httpServer.m_onError = [this](shared_ptr <Request> & request, const error_code & e) {
        return onRequestError(request, e);
    };

    LOG(INFO) << m_logtag << "register all request handle done";
    return 0;
}

//////////////
// [Build]
int DynaDetectService::buildDynaDetectStreamlet() {
    DavStreamletOption so;
    so.set(DavOptionBufLimitNum(), std::to_string(m_dynaDetectGlobalSetting.max_buf_num_of_detect_streamlet()));
    vector<DavWaveOption> waveOptions;
    for (auto & s : m_dnnDetectorSettings) {
        if (m_detectors.at(s.first)) {
            DavWaveOption o;
            PbDnnDetectSettingToDavOption::toDnnDetectOption(s.second, o, s.first);
            waveOptions.emplace_back(o);
        }
    }
    DavWaveOption dataRelay((DavWaveClassDataRelay()));
    waveOptions.emplace_back(dataRelay);
    DavWaveOption postDraw((DavWaveClassCvPostDraw()));
    waveOptions.emplace_back(postDraw);

    ObjDetectStreamletBuilder builder;
    auto streamletDetect = builder.build(waveOptions, ObjDetectStreamletTag(m_dnnDetectStreamletName), so);
    if (!streamletDetect) {
        ERRORIT(APP_ERROR_BUILD_STREAMLET, ("build streamletDetect fail " + toStringViaOss(builder.m_buildInfo)));
        return APP_ERROR_BUILD_STREAMLET;
    }
    m_river.add(streamletDetect);
    return 0;
}

} // namespace dyna_detect_service
