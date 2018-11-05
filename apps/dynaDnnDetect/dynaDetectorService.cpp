#include "davStreamlet.h"
#include "davStreamletBuilder.h"
#include "cvStreamlet.h"
#include "dynaDetectorService.h"

namespace dyna_detect_service {

////////////////////////////////////////////////////////////////////////////////
static const std::map<int, const char *> dynaDetectServiceErrorCode2StrMap = {
    {DYNA_DETECT_ERROR_TASK_INIT, " failed create task from config file"},
    {DYNA_DETECT_ERROR_DETECTOR_ALREADY_RUNNING, " detector already enabled"},
    {DYNA_DETECT_ERROR_DETECTOR_ALREADY_STOPPED, " detector already stopped"},
    {DYNA_DETECT_ERROR_NO_SUCH_DETECTOR, "no such dnn detector"}
};

string dynaDetectServiceErr2Str(const int errCode) {
    auto it = std::find_if(dynaDetectServiceErrorCode2StrMap.begin(), dynaDetectServiceErrorCode2StrMap.end(),
                           [errCode](const std::pair<int, const char *> & item) {
                               return item.first == errCode;});
    if (it != dynaDetectServiceErrorCode2StrMap.end())
        return string(it->second);
    return appServiceErr2Str(errCode);
}

////////////////////////////////////////////////
// [Http Request Return Error Code]
static constexpr int API_ERRCODE_INVALID_MSG = 1;
static constexpr int API_ERRCODE_DETECTOR_ALREADY_RUNNING = 2;
static constexpr int API_ERRCODE_DETECTOR_ALREADY_STOPPED = 3;
static constexpr int API_ERRCODE_NO_SUCH_DETECTOR = 4;

////////////////////////////////////////////////
int DynaDetectService::createTask() {
    int ret = buildOutputStreamlet("output_dyna_detect", m_outputSetting, m_dynaDetectGlobalSetting.output_url());
    if (ret < 0) {
        m_river.clear();
        ERRORIT(DYNA_DETECT_ERROR_TASK_INIT,  "fail create output streamlet " + m_appInfo.m_msgDetail);
        return failResponse(response, API_ERRCODE_INVALID_MSG, m_appInfo.m_msgDetail);
    }
    LOG(INFO) << m_logtag << "craete output streamlet done";

    /* 2. build dnn detect streamlet */
    ret = buildDynaDetectStreamlet();
    if (ret < 0) {
        m_river.clear();
        ERRORIT(DYNA_DETECT_ERROR_TASK_INIT,  "fail create dyna detect streamlet " + m_appInfo.m_msgDetail);
        return failResponse(response, API_ERRCODE_INVALID_MSG, m_appInfo.m_msgDetail);
    }
    LOG(INFO) << m_logtag << "craete dnn streamlet done";

    /* 3. async build input streamlet with callback that connect to mix streamlet */
    auto onBuildSuccess = [this] (shared_ptr<DavStreamlet> inputStreamlet) {
        if (!inputStreamlet)
            return AVERROR(EINVAL);
        auto detectStreamlet = m_river.get(CvDnnDetectStreamletTag(m_dnnDetectStreamletName));
        if (!detectStreamlet)
            return AVERROR(EINVAL);
        inputStreamlet >= detectStreamlet; /* only connect video */
        return inputStreamlet->start(); /*start just after connect */
    };

    /* 4. river start */
    m_river.start();
    response->write(m_successCRJsonStrAsync);
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

    // create new detector and start it.
    return 0;
}

int DynaDetectService::onDeleteOneDetector(shared_ptr<Response> & response, const string & detectorName) {
    std::lock_guard<mutex> lock(m_runLock);
    if (!m_detectors.count(detectorName)) {
        string detail = detectorName + " is not in configure file";
        ERRORIT(DYNA_DETECT_ERROR_NO_SUCH_DETECTOR, detail);
        return failResponse(response, API_ERRCODE_NO_SUCH_DETECTOR, detail);
    }
    if (m_detectors.at(detectorName)) {
        string detail = detectorName + " is alrady enabled";
        ERRORIT(DYNA_DETECT_ERROR_DETECTOR_ALREADY_STOPPED, detail);
        return failResponse(response, API_ERRCODE_DETECTOR_ALREADY_STOPPED, detail);
    }
    m_detectors.at(detectorName) = false;
    // find and delete that detector
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
    m_httpServer.m_resources["^/api1/detectors/[a-zA-Z0-9_]+$"]["POST"] =
        [this] (shared_ptr<Response> & response, shared_ptr<Request> & request) {
        const string & queryPath = request->m_path;
        std::size_t found = queryPath.find_last_of("/");
        const string detectorName(queryPath.substr(found+1));
        return onAddOneDetector(response, detectorName);
    };
    m_httpServer.m_resources["^/api1/detectors/[a-zA-Z0-9_]+$"]["DELETE"] =
        [this] (shared_ptr<Response> & response, shared_ptr<Request> & request) {
        const string & queryPath = request->m_path;
        std::size_t found = queryPath.find_last_of("/");
        const string detectorName(queryPath.substr(found+1));
        return onDeleteDetector(response, detectorName);
    };

    /* stop */
    m_httpServer.m_resources["^/api1/dynaDetect/stop$"]["POST"] =
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
        DavWaveOption o;
        PbDnnDetectSettingToDavOption::toDnnDetectOption(s.second, o);
        waveOptions.emplace_back(o);
    }
    CvDnnDetectStreamletBuilder builder;
    auto streamletDetect = builder.build(waveOptions, CvDnnDetectStreamletTag(m_dnnDetectStreamletName), so);
    if (!streamletDetect) {
        ERRORIT(APP_ERROR_BUILD_STREAMLET, ("build streamletDetect fail " + toStringViaOss(builder.m_buildInfo)));
        return APP_ERROR_BUILD_STREAMLET;
    }
    m_river.add(streamletDetect);
    return 0;
}

} // namespace dyna_detect_service
