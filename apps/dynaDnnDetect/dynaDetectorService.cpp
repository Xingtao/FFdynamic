#include "davStreamlet.h"
#include "davStreamletBuilder.h"
#include "cvStreamlet.h"
#include "dynaDetectorService.h"

namespace dyna_detect_service {

////////////////////////////////////////////////////////////////////////////////
static const std::map<int, const char *> dynaDetectServiceErrorCode2StrMap = {
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
static constexpr int API_ERRCODE_DETECTOR_EXISTS = 2;
static constexpr int API_ERRCODE_NO_SUCH_DETECTOR = 3;

////////////////////////////////////////////////
// [dynamic events]

///////////////////
// [update setting]
int DynaDetectService::onUpdateInputSetting(shared_ptr<Response> & response,
                                         const DynaDetectRequest::UpdateInputSetting & inputSetting) {
    m_inputSetting.CopyFrom(inputSetting);
    LOG(INFO) << m_logtag << "Update input setting " << PbTree::pbToJsonString(m_inputSetting);
    response->write(m_successCRJsonStr);
    return 0;
}

//////////////////
// [other handler]

/* dynaDetect stop */
int DynaDetectService::onDynaDetectStop(shared_ptr<Response> & response, shared_ptr<Request> & request) {
    LOG(INFO) << m_logtag << "receive dynaDetect stop request";
    response->write(m_successCRJsonStr);
    m_roomId.clear();
    m_roomOutputBaseUrl.clear();
    m_river.stop();
    m_river.clear();
    DynaDetectService::setExit();
    return 0;
}

/////////////////////////////////////////
// [Streamlet/Wave options build helpers]

////////////////////////////////////////////////////////////////////////////////
// [Register Http Dynamic On Handler]
int DynaDetectService::onRequest(shared_ptr<Request> & request, shared_ptr<Response> & response, pb::Message & pbmsg,
                          std::function<int()> requestProcess, bool bNeedRoomIdExist) {
    std::lock_guard<mutex> lock(m_runLock);
    if (bNeedRoomIdExist && m_roomId.empty()) {
        ERRORIT(APP_ERROR_PROCESS_REQUEST, "There is no room exist, create it first");
        return failResponse(response, API_ERRCODE_CREATE_ROOM_FIRST, "room not exists, create it first");
    } else if (!bNeedRoomIdExist && !m_roomId.empty()) {
        ERRORIT(APP_ERROR_PROCESS_REQUEST, "Room already exists");
        return failResponse(response, API_ERRCODE_ROOM_EXISTS, "room already created");
    }
    int ret = requestToMessage(request, pbmsg);
    if (ret < 0)
        return failResponse(response, API_ERRCODE_INVALID_MSG, m_appInfo.m_msgDetail);

    /* also do response in this call */
    ret = requestProcess();
    if (ret < 0)
        return ret;
    return afterHttpResponse();
}

int DynaDetectService::registerHttpRequestHandlers() {
    m_httpServer.m_resources["^/api1/dynaDetect/add_output_setting$"]["POST"] =
        [this] (shared_ptr<Response> & response, shared_ptr<Request> & request) {
        DynaDetectRequest::AddOutputSetting addOutputSetting;
        return onRequest(request, response, addOutputSetting,
                         [this, &response, &addOutputSetting] () {
                             return onAddOutputSetting(response, addOutputSetting);});
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

} // namespace dyna_detect_service
