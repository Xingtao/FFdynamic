#include "davStreamlet.h"
#include "davStreamletBuilder.h"

#include "pbToDavOptionEvent.h"
#include "ialHttpService.h"
#include "davDynamicEvent.h"
#include "ialRequest.pb.h"

namespace ial_service {

////////////////////////////////////////////////
static constexpr int API_ERRCODE_INVALID_MSG = 1;
static constexpr int API_ERRCODE_ROOM_EXISTS = 2;
static constexpr int API_ERRCODE_INVALID_INPUT_SETTING = 3;
static constexpr int API_ERRCODE_INVALID_OUTPUT_SETTING = 4;
static constexpr int API_ERRCODE_INVALID_MIX_SETTING = 5;
static constexpr int API_ERRCODE_NO_SUCH_OUTPUT_SETTING = 6;
static constexpr int API_ERRCODE_ADD_EXIST_INPUT = 7;
static constexpr int API_ERRCODE_ADD_EXIST_OUTPUT = 8;
static constexpr int API_ERRCODE_CLOSE_NONE_EXIST_INPUT = 9;
static constexpr int API_ERRCODE_CLOSE_NONE_EXIST_OUTPUT = 10;
static constexpr int API_ERRCODE_EXCEED_MAX_PARTICIPANTS = 11;
static constexpr int API_ERRCODE_CREATE_ROOM_FIRST = 12;
static constexpr int API_ERRCODE_MIX_LAYOUT_CHANGE = 12;
static constexpr int API_ERRCODE_MUTE_UNMUTE = 13;
static constexpr int API_ERRCODE_MIX_UPDATE_BACKGROUD = 14;
////////////////////////////////////////////////
// [dynamic events]

int IalHttpService::onCreateRoom(shared_ptr<Response> & response, const IalRequest::CreateRoom & createRoom) {
      int ret = sanityCheckCreateRoomMsg(createRoom);
    if (ret != 0)
        return failResponse(response, ret, "create room msg content invalid: " + m_ialInfo.m_msgDetail);

    /* 1. build streamlet options */
    for (const auto & oneOutputInfo : createRoom.output_stream_infos()) {
        // the full output uri will be: base_url + room_id + "_" + output_setting_id + ["." + mux_fmt];
        const auto & oid = oneOutputInfo.output_setting_id();
        const auto & outSettingUsed =
            oneOutputInfo.has_specific_setting() ? oneOutputInfo.specific_setting() : m_outputSettings.at(oid);
        vector<string> outputFullUrls;
        if (oneOutputInfo.output_urls().size() == 0) {
            outputFullUrls = mkFullOutputUrl(createRoom.room_output_base_url(),
                                             createRoom.room_id(), oid, outSettingUsed);
        } else  {
            for (const auto & u : oneOutputInfo.output_urls())
                outputFullUrls.emplace_back(u);
        }
        ret = buildOutputStreamlet(oid, outSettingUsed, outputFullUrls);
        if (ret < 0) {
            m_river.clear();
            ERRORIT(IAL_ERROR_CREATE_ROOM,
                    "create room with id: " + createRoom.room_id() + ";" + m_ialInfo.m_msgDetail);
            return failResponse(response, API_ERRCODE_INVALID_OUTPUT_SETTING,
                            "Fail create room " + m_ialInfo.m_msgDetail);
        }
    }

    /* 2. build streamlet: sync build mix & output streamlet, aync build input streamlet */
    ret = buildMixStreamlet(m_mixStreamletName);
    if (ret < 0) {
        m_river.clear();
        ERRORIT(IAL_ERROR_CREATE_ROOM,
                "create room with id: " + createRoom.room_id() + ";" + m_ialInfo.m_msgDetail);
        return failResponse(response, API_ERRCODE_INVALID_MIX_SETTING,
                            "Fail create room " + m_ialInfo.m_msgDetail);
    }
    LOG(INFO) << m_logtag << "craete mix streamlet done";

    /* 3. connect mix & outputs */
    auto mixStreamlet = m_river.get(DavMixStreamletTag(m_mixStreamletName));
    auto outputStreamlets = m_river.getStreamletsByCategory(DavDefaultOutputStreamletTag());
    for (auto & o : outputStreamlets)
        mixStreamlet >> o;

    LOG(INFO) << m_logtag << "connect mix streamlet done. start async create input streamlet";

    /* 4. async build input streamlet with callback that connect to mix streamlet */
    for (const auto & inputUrl : createRoom.input_urls())
        asyncBuildInputStreamlet(inputUrl, m_inputSetting);

    /* 5. river start */
    m_river.start();

    /* finally */
    m_roomId = createRoom.room_id();
    m_roomOutputBaseUrl = createRoom.room_output_base_url();
    response->write(m_successCRJsonStrAsync);
    LOG(INFO) << m_logtag << "craete new room with roomId: " << m_roomId << ", output base "
              << m_roomOutputBaseUrl;
    return 0;
}

int IalHttpService::asyncBuildInputStreamlet(const string & inputUrl,
                                             const DavStreamletSetting::InputStreamletSetting & inputSetting) {
    auto inSetting = make_shared<DavStreamletSetting::InputStreamletSetting>();
    inSetting->CopyFrom(inputSetting); /* in case pass down inputSetting out of scope */
    std::thread([this, inSetting, inputUrl]() {
            auto fut = std::async(std::launch::async, [this, inSetting, inputUrl]() {
                    return this->buildInputStreamlet(inputUrl, *inSetting);
                });
            fut.wait();
            if (fut.get() >= 0) {
                auto inputStreamlet = m_river.get(DavDefaultInputStreamletTag(inputUrl));
                auto mixStreamlet = m_river.get(DavMixStreamletTag(m_mixStreamletName));
                CHECK(inputStreamlet != nullptr && mixStreamlet != nullptr);
                inputStreamlet >> mixStreamlet;
                inputStreamlet->start(); /*start just after connect */
            }
            return;
        }).detach();
    return 0;
}

////////////////////////////////////////////////////////////////////////////////
//// [Input Streamlet Add/Close/Get Ino]
int IalHttpService::onAddNewInputStream(shared_ptr<Response> & response,
                                        const IalRequest::AddNewInputStream & joinNew) {
    const string & inputUrl = joinNew.input_url();
    /* proto2: check has_field; proto3: for scalars, like a c strucutre; for message like a pointer */
    if  (inputUrl.empty()) {
        const string detail = "add new input stream doesn't contain valid input url";
        ERRORIT(IAL_ERROR_PARTICIPANT_JOIN, detail);
        return failResponse(response, API_ERRCODE_INVALID_MSG, detail);
    }
    auto exist = m_river.count(DavDefaultInputStreamletTag(inputUrl));
    if (exist) {
        const string detail = "input stream already exist. close it first: " + inputUrl;
        ERRORIT(IAL_ERROR_OUTSTREAM_ADD_CLOSE, detail);
        return failResponse(response, API_ERRCODE_ADD_EXIST_OUTPUT, detail);
    }
    /* check max participants */
    vector<shared_ptr<DavStreamlet>> inputStreamlets =
        m_river.getStreamletsByCategory(DavDefaultInputStreamletTag());
    /* TODO: async building one is not in count */
    if ((int)inputStreamlets.size() + 1 >= m_ialGlobalSetting.max_participants()) {
        const string detail = "exceed max join participants: " + inputUrl;
        ERRORIT(IAL_ERROR_EXCEED_MAX_JOINS, detail);
        return failResponse(response, API_ERRCODE_EXCEED_MAX_PARTICIPANTS, detail);
    }

    const auto & inSetting = joinNew.has_specific_setting() ? joinNew.specific_setting() : m_inputSetting;
    asyncBuildInputStreamlet(joinNew.input_url(), inSetting);
    response->write(m_successCRJsonStrAsync);
    return 0;
}

int IalHttpService::onCloseOneInputStream(shared_ptr<Response> & response,
                                          const IalRequest::CloseOneInputStream & closeInput) {
    const string & inputUrl = closeInput.input_url();
    if  (inputUrl.empty()) {
        const string detail = "CloseInputStream doesn't contain valid input url " + inputUrl;
        ERRORIT(IAL_ERROR_PARTICIPANT_LEFT, detail);
        return failResponse(response, API_ERRCODE_CLOSE_NONE_EXIST_INPUT, detail);
    }
    const auto closeInputTag = DavDefaultInputStreamletTag(inputUrl);
    if (m_river.count(closeInputTag) == 0) {
        const string detail = "CloseInputStream: cannot find input streamlet with input url " + inputUrl;
        ERRORIT(IAL_ERROR_PARTICIPANT_LEFT, detail);
        return failResponse(response, API_ERRCODE_CLOSE_NONE_EXIST_INPUT, detail);
    }

    auto inputStreamlet = m_river.get(closeInputTag);
    /* stop will make a flush and quit wave's thread; also disconnect with its peers;
       this may take a while, but should less than 50ms */
    inputStreamlet->stop();
    m_river.erase(closeInputTag);
    response->write(m_successCRJsonStr);
    return 0;
}

////////////////////////////////////////////////////////////////////////////////
//// [Output Streamlet Add/Close/Get Ino]

int IalHttpService::onAddNewOutput(shared_ptr<Response> & response,
                                   const IalRequest::AddNewOutput & oneOutputInfo) {
    const auto & oid = oneOutputInfo.output_setting_id();
    if (m_river.count(DavDefaultOutputStreamletTag(oid))) {
        /* TODO: not good restriction: output setting id could only be used by one output */
        const string detail = "output setting id already used by an output streamlet: " + oid;
        ERRORIT(IAL_ERROR_OUTSTREAM_ADD_CLOSE, detail);
        return failResponse(response, API_ERRCODE_INVALID_OUTPUT_SETTING, detail);
    }
    const bool bHasSpecificSetting = oneOutputInfo.has_specific_setting();
    const bool bUseDefaultSetting = static_cast<bool>(m_outputSettings.count(oid));
    if (!bHasSpecificSetting && !bUseDefaultSetting) {
        const string detail = (oid + " associated output setting not "
                               "found and no specific setting, add it first");
        ERRORIT(IAL_ERROR_OUTPUT_ID_NOT_FOUND, detail);
        return failResponse(response, API_ERRCODE_INVALID_OUTPUT_SETTING, detail);
    }
    if (oneOutputInfo.output_urls().size() == 0 && m_roomOutputBaseUrl.empty()) {
        const string detail = "no default output base url and no pass down full output url";
            ERRORIT(IAL_ERROR_OUTPUT_URL_NOT_FOUND, detail);
        return failResponse(response, API_ERRCODE_INVALID_OUTPUT_SETTING, detail);
    }

    const auto & outSettingUsed = bHasSpecificSetting ?
        oneOutputInfo.specific_setting() : m_outputSettings.at(oid);
    vector<string> outputFullUrls;
    if (oneOutputInfo.output_urls().size() == 0)
        outputFullUrls = mkFullOutputUrl(m_roomOutputBaseUrl, m_roomId, oid, outSettingUsed);
    else {
        for (const auto & u : oneOutputInfo.output_urls())
            outputFullUrls.emplace_back(u);
    }
    int ret = buildOutputStreamlet(oid, outSettingUsed, outputFullUrls);
    if (ret < 0)
        return failResponse(response, API_ERRCODE_INVALID_OUTPUT_SETTING,
                            "Fail add new output straem " + m_ialInfo.m_msgDetail);

    auto newOutputStreamlet = m_river.get(DavDefaultOutputStreamletTag(oid));
    auto mixStreamlet = m_river.get(DavMixStreamletTag(m_mixStreamletName));
    CHECK(newOutputStreamlet != nullptr && mixStreamlet != nullptr);
    newOutputStreamlet->start();
    mixStreamlet >> newOutputStreamlet;
    response->write(m_successCRJsonStr);
    return 0;
}

int IalHttpService::onCloseOneOutput(shared_ptr<Response> & response,
                                     const IalRequest::CloseOneOutput & closeOutput) {
    const DavDefaultOutputStreamletTag spcificOutputTag(closeOutput.output_setting_id());;
    if (!m_river.count(spcificOutputTag)) {
        const string detail = "CloseOutputStream doesn't contain valid output setting id " +
            closeOutput.output_setting_id();
        ERRORIT(IAL_ERROR_PARTICIPANT_LEFT, detail);
        return failResponse(response, API_ERRCODE_CLOSE_NONE_EXIST_OUTPUT, detail);
    }
    auto outputStreamlet = m_river.get(spcificOutputTag);
    outputStreamlet->stop();
    m_river.erase(spcificOutputTag);
    response->write(m_successCRJsonStr);
    return 0;
}

/* mix */
int IalHttpService::onMuteUnmute(shared_ptr<Response> & response,
                                 const IalRequest::AudioMixMuteUnMute & muteUnmute) {
    if (!muteUnmute.mute_input_urls_size() && !muteUnmute.unmute_input_urls_size())
        return failResponse(response, API_ERRCODE_INVALID_MSG, "no mute/unmute setting in message");

    auto mixStreamlets = m_river.getStreamletsByCategory((DavMixStreamletTag()));
    if (mixStreamlets.size() != 1) {
        const string detail = "mute/unmute fail, no mix streamlet found";
        ERRORIT(IAL_ERROR_MUTE_UNMUTE, detail);
        return failResponse(response, API_ERRCODE_MUTE_UNMUTE, detail);
    }
    auto & mixStreamlet = mixStreamlets[0];
    auto audioMixers = mixStreamlet->getWavesByCategory((DavWaveClassAudioMix()));
    if (audioMixers.size() != 1) {
        const string detail = "mute/unmute fail, no audio mix wave found";
        ERRORIT(IAL_ERROR_MUTE_UNMUTE, detail);
        return failResponse(response, API_ERRCODE_MUTE_UNMUTE, detail);
    }
    auto & audioMixer = audioMixers[0];

    /* get mute/unmute list */
    DavDynaEventAudioMixMuteUnmute muteUnmuteEvent;
    for (const auto & m : muteUnmute.mute_input_urls()) {
        auto streamlet = m_river.get(DavDefaultInputStreamletTag(m));
        if (!streamlet) {
            LOG(WARNING) << m_logtag << "no input streamlet with " + m;
            continue;
        }
        muteUnmuteEvent.m_muteGroupIds.emplace_back(streamlet->getGroupId());
    }
    for (const auto & m : muteUnmute.unmute_input_urls()) {
        auto streamlet = m_river.get(DavDefaultInputStreamletTag(m));
        if (!streamlet) {
            LOG(WARNING) << m_logtag << "no input streamlet with " + m;
            continue;
        }
        muteUnmuteEvent.m_unmuteGroupIds.emplace_back(streamlet->getGroupId());
    }
    int ret = audioMixer->processDynamicEvent(muteUnmuteEvent);
    if (ret < 0) {
        const string detail = "mute/unmute audio mix process fail";
        ERRORIT(IAL_ERROR_MUTE_UNMUTE, detail);
        return failResponse(response, API_ERRCODE_MUTE_UNMUTE, detail);
    }
    response->write(m_successCRJsonStrAsync);
    return 0;
}

int IalHttpService::onMixBackgroudUpdate(shared_ptr<Response> & response,
                                         const IalRequest::VideoMixUpdateBackgroud & backgroudChange) {
    auto mixStreamlets = m_river.getStreamletsByCategory((DavMixStreamletTag()));
    if (mixStreamlets.size() != 1) {
        const string detail = "layout change fail, no video mix streamlet found";
        ERRORIT(IAL_ERROR_MIX_SET_BACKGROUD, detail);
        return failResponse(response, API_ERRCODE_MIX_UPDATE_BACKGROUD, detail);
    }
    auto & mixStreamlet = mixStreamlets[0];
    auto videoMixers = mixStreamlet->getWavesByCategory((DavWaveClassVideoMix()));
    if (videoMixers.size() != 1) {
        const string detail = "layout change fail, no video mix wave found";
        ERRORIT(IAL_ERROR_MIX_SET_BACKGROUD, detail);
        return failResponse(response, API_ERRCODE_MIX_UPDATE_BACKGROUD, detail);
    }
    auto & videoMixer = videoMixers[0];
    DavDynaEventVideoMixSetNewBackgroud dynaEvent {backgroudChange.backgroud_image_url()};
    int ret = videoMixer->processDynamicEvent(dynaEvent);
    if (ret < 0) {
        const string detail = "video mix set new backgroud picture fail" + backgroudChange.backgroud_image_url();
        ERRORIT(IAL_ERROR_MIX_SET_BACKGROUD, detail);
        return failResponse(response, API_ERRCODE_MIX_UPDATE_BACKGROUD, detail);
    }
    response->write(m_successCRJsonStrAsync);
    return 0;
}

int IalHttpService::onMixLayoutChange(shared_ptr<Response> & response,
                                      const IalRequest::VideoMixChangeLayout & layoutChange) {
    if (!layoutChange.has_new_layout()) {
        return failResponse(response, API_ERRCODE_INVALID_MSG, "no new layout info found");
    }
    auto mixStreamlets = m_river.getStreamletsByCategory((DavMixStreamletTag()));
    if (mixStreamlets.size() != 1) {
        const string detail = "layout change fail, no video mix streamlet found";
        ERRORIT(IAL_ERROR_MIX_LAYOUT_CHANGE, detail);
        return failResponse(response, API_ERRCODE_MIX_LAYOUT_CHANGE, detail);
    }
    auto & mixStreamlet = mixStreamlets[0];
    auto videoMixers = mixStreamlet->getWavesByCategory((DavWaveClassVideoMix()));
    if (videoMixers.size() != 1) {
        const string detail = "layout change fail, no video mix wave found";
        ERRORIT(IAL_ERROR_MIX_LAYOUT_CHANGE, detail);
        return failResponse(response, API_ERRCODE_MIX_LAYOUT_CHANGE, detail);
    }
    auto & videoMixer = videoMixers[0];
    const DavWaveSetting::VideoMixLayoutUpdate & layoutInfo = layoutChange.new_layout();
    DavDynaEventVideoMixLayoutUpdate e;
    PbEventToDavDynamicEvent::toLayoutUpdateEvent(layoutInfo, e);
    int ret = videoMixer->processDynamicEvent(e);
    if (ret < 0) {
        const string detail = "layout change video mix process fail";
        ERRORIT(IAL_ERROR_MIX_LAYOUT_CHANGE, detail);
        return failResponse(response, API_ERRCODE_MIX_LAYOUT_CHANGE, detail);
    }
    response->write(m_successCRJsonStrAsync);
    return 0;
}

/* queries */
int IalHttpService::onGetOneInputStreamInfo(shared_ptr<Response> & response,
                                            const IalRequest::GetOneInputStreamInfo & getInputInfo) {
    return 0;
}

int IalHttpService::onGetOneOutputStreamInfo(shared_ptr<Response> & response,
                                             const IalRequest::GetOneOutputStreamInfo & getOutputInfo) {
    return 0;
}

/* TODO: get mute/unmute info; filter info; layout info */

///////////////////
// [update setting]
int IalHttpService::onUpdateInputSetting(shared_ptr<Response> & response,
                                         const IalRequest::UpdateInputSetting & inputSetting) {
    m_inputSetting.CopyFrom(inputSetting);
    LOG(INFO) << m_logtag << "Update input setting " << PbTree::pbToJsonString(m_inputSetting);
    response->write(m_successCRJsonStr);
    return 0;
}

int IalHttpService::onAddOutputSetting(shared_ptr<Response> & response,
                                       const IalRequest::AddOutputSetting & addOutputSetting) {
    auto outputSettingId = addOutputSetting.output_setting_id();
    auto outputSetting = addOutputSetting.output_setting();
    if (m_outputSettings.count(outputSettingId) == 0) {
        m_outputSettings.emplace(outputSettingId, outputSetting);
    } else {
        auto outset = m_outputSettings.at(outputSettingId);
        outset.CopyFrom(outputSetting);
    }
    LOG(INFO) << m_logtag << "Update output setting, setting id " << outputSettingId
              << PbTree::pbToJsonString(outputSetting);
    response->write(m_successCRJsonStr);
    return 0;
}

int IalHttpService::onUpdateMixSetting(shared_ptr<Response> & response,
                                       const IalRequest::UpdateMixSetting & mixSetting) {
    m_mixSetting.CopyFrom(mixSetting);
    LOG(INFO) << m_logtag << "Update mix setting " << PbTree::pbToJsonString(m_mixSetting);
    response->write(m_successCRJsonStr);
    return 0;
}

//////////////////
// [other handler]

/* ial stop */
int IalHttpService::onIalStop(shared_ptr<Response> & response, shared_ptr<Request> & request) {
    LOG(INFO) << m_logtag << "receive ial stop request";
    response->write(m_successCRJsonStr);
    m_roomId.clear();
    m_roomOutputBaseUrl.clear();
    m_river.stop();
    m_river.clear();
    IalService::setIalExit();
    return 0;
}

/* on error */
int IalHttpService::onRequestError(shared_ptr<Request> & request, const error_code & ec) {
    asio::streambuf::const_buffers_type cbt = request->m_request.data();
    string requeststr(asio::buffers_begin(cbt), asio::buffers_end(cbt));
    ERRORIT(IAL_ERROR_PROCESS_REQUEST, ec.message() + ", " + requeststr);
    return 0;
}

/////////////////////////////////////////
// [Streamlet/Wave options build helpers]

int IalHttpService::buildInputStreamlet(const string & inputUrl,
                                        const DavStreamletSetting::InputStreamletSetting & inputSetting) {
    DavStreamletOption so;
    so.set(DavOptionBufLimitNum(), std::to_string(m_ialGlobalSetting.input_max_buf_num()));
    vector<DavWaveOption> waveOptions;
    PbStreamletSettingToDavOption::mkInputStreamletWaveOptions(inputUrl, inputSetting, waveOptions);
    DavDefaultInputStreamletBuilder builder;
    auto streamletInput = builder.build(waveOptions, DavDefaultInputStreamletTag(inputUrl), so);
    if (!streamletInput) {
        /* work around, for we may in another thread */
        IalMessager msg(IAL_ERROR_BUILD_STREAMLET, ("fail create input streamlet with id " + inputUrl + ", " +
                                                    toStringViaOss(builder.m_buildInfo)));
        s_ialMsgCollector.addMsg(msg);
        LOG(ERROR) << m_logtag << msg;

        // ERRORIT(IAL_ERROR_BUILD_STREAMLET, "input streamlet build fail with id: " + inputUrl);
        return IAL_ERROR_BUILD_STREAMLET;
    }
    m_river.add(streamletInput);
    LOG(INFO) << m_logtag << m_river.dumpRiver();
    return 0;
}

int IalHttpService::buildMixStreamlet(const string & mixStreamletName) {
    DavStreamletOption so;
    so.set(DavOptionBufLimitNum(), std::to_string(m_ialGlobalSetting.mix_max_buf_num()));
    vector<DavWaveOption> waveOptions;
    int ret = PbStreamletSettingToDavOption::
        mkMixStreamletWaveOptions(mixStreamletName, m_mixSetting, waveOptions);
    if (ret < 0) {
        ERRORIT(IAL_ERROR_BUILD_STREAMLET, "mix setting invalid");
        return IAL_ERROR_BUILD_STREAMLET;
    }
    DavMixStreamletBuilder builder;
    auto streamletMix = builder.build(waveOptions, DavMixStreamletTag(mixStreamletName), so);
    if (!streamletMix) {
        ERRORIT(IAL_ERROR_BUILD_STREAMLET, ("mix streamlet build fail with id: " + mixStreamletName + ", " +
                                            toStringViaOss(builder.m_buildInfo)));
        return IAL_ERROR_BUILD_STREAMLET;
    }
    m_river.add(streamletMix);
    return 0;
}

int IalHttpService::buildOutputStreamlet(const string & outputId,
                                         const DavStreamletSetting::OutputStreamletSetting & outStreamletSetting,
                                         const vector<string> & fullOutputUrls) {
    DavStreamletOption so;
    so.set(DavOptionBufLimitNum(), std::to_string(m_ialGlobalSetting.output_max_buf_num()));
    vector<DavWaveOption> waveOptions;
    PbStreamletSettingToDavOption::mkOutputStreamletWaveOptions(fullOutputUrls,
                                                                outStreamletSetting, waveOptions);
    DavDefaultOutputStreamletBuilder builder;
    auto streamletOutput = builder.build(waveOptions, DavDefaultOutputStreamletTag(outputId), so);
    if (!streamletOutput) {
        ERRORIT(IAL_ERROR_BUILD_STREAMLET, ("build output streamlet fail; with id " +
                                            outputId + ", " + toStringViaOss(builder.m_buildInfo)));
        return IAL_ERROR_BUILD_STREAMLET;
    }
    m_river.add(streamletOutput);
    return 0;
}

////////////////////////////////////////////////////////////////////////////////
// [Register Http Dynamic On Handler]
int IalHttpService::afterResponse() {
    /* notify this event */
    m_bMonitorCheck = true;
    m_monitorCV.notify_one();
    return 0;
}

int IalHttpService::onRequest(shared_ptr<Request> & request, shared_ptr<Response> & response, pb::Message & pbmsg,
                              std::function<int()> requestProcess, bool bNeedRoomIdExist) {
    std::lock_guard<mutex> lock(m_runLock);
    if (bNeedRoomIdExist && m_roomId.empty()) {
        ERRORIT(IAL_ERROR_PROCESS_REQUEST, "There is no room exist, create it first");
        return failResponse(response, API_ERRCODE_CREATE_ROOM_FIRST, "room not exists, create it first");
    } else if (!bNeedRoomIdExist && !m_roomId.empty()) {
        ERRORIT(IAL_ERROR_PROCESS_REQUEST, "Room already exists");
        return failResponse(response, API_ERRCODE_ROOM_EXISTS, "room already created");
    }
    int ret = requestToMessage(request, pbmsg);
    if (ret < 0)
        return failResponse(response, API_ERRCODE_INVALID_MSG, m_ialInfo.m_msgDetail);

    /* also do response in this call */
    ret = requestProcess();
    if (ret < 0)
        return ret;
    return afterResponse();
}

int IalHttpService::registerHttpRequestHandlers() {
    /* update / add setting part */
    m_httpServer.m_resources["^/api1/ial/update_input_setting$"]["POST"] =
        [this] (shared_ptr<Response> & response, shared_ptr<Request> & request) {
        IalRequest::UpdateInputSetting inputSetting;
        return onRequest(request, response, inputSetting,
                         [this, &response, &inputSetting] () {
                             return onUpdateInputSetting(response, inputSetting);});
    };

    m_httpServer.m_resources["^/api1/ial/add_output_setting$"]["POST"] =
        [this] (shared_ptr<Response> & response, shared_ptr<Request> & request) {
        IalRequest::AddOutputSetting addOutputSetting;
        return onRequest(request, response, addOutputSetting,
                         [this, &response, &addOutputSetting] () {
                             return onAddOutputSetting(response, addOutputSetting);});
    };

    m_httpServer.m_resources["^/api1/ial/update_mix_setting$"]["POST"] =
        [this] (shared_ptr<Response> & response, shared_ptr<Request> & request) {
        IalRequest::UpdateMixSetting mixSetting;
        return onRequest(request, response, mixSetting,
                         [this, &response, &mixSetting] () {
                             return onUpdateMixSetting(response, mixSetting);});
    };

    /* ial task dynamic change part */
    m_httpServer.m_resources["^/api1/ial/create_room$"]["POST"] =
        [this] (shared_ptr<Response> & response, shared_ptr<Request> & request) {
        IalRequest::CreateRoom createRoom;
        return onRequest(request, response, createRoom,
                         [this, &response, &createRoom] () {
                             return onCreateRoom(response, createRoom);},
                         false);
    };

    m_httpServer.m_resources["^/api1/ial/add_new_input_stream$"]["POST"] =
        [this] (shared_ptr<Response> & response, shared_ptr<Request> & request) {
        IalRequest::AddNewInputStream joinNew;
        return onRequest(request, response, joinNew,
                         [this, &response, &joinNew] () {
                             return onAddNewInputStream(response, joinNew);});
    };

    m_httpServer.m_resources["^/api1/ial/add_new_output"]["POST"] =
        [this] (shared_ptr<Response> & response, shared_ptr<Request> & request) {
        IalRequest::AddNewOutput oneOutputInfo;
        return onRequest(request, response, oneOutputInfo,
                         [this, &response, &oneOutputInfo] () {
                             return onAddNewOutput(response, oneOutputInfo);});
    };

    m_httpServer.m_resources["^/api1/ial/close_one_input_stream$"]["POST"] =
        [this] (shared_ptr<Response> & response, shared_ptr<Request> & request) {
        IalRequest::CloseOneInputStream closeInput;
        return onRequest(request, response, closeInput,
                         [this, &response, &closeInput] () {
                             return onCloseOneInputStream(response, closeInput);});
    };

    m_httpServer.m_resources["^/api1/ial/close_one_output$"]["POST"] =
        [this] (shared_ptr<Response> & response, shared_ptr<Request> & request) {
        IalRequest::CloseOneOutput closeOutput;
        return onRequest(request, response, closeOutput,
                         [this, &response, &closeOutput] () {
                             return onCloseOneOutput(response, closeOutput);});
    };

    /* audio mix */
    m_httpServer.m_resources["^/api1/ial/mute_unmute_stream$"]["POST"] =
        [this] (shared_ptr<Response> & response, shared_ptr<Request> & request) {
        IalRequest::AudioMixMuteUnMute muteUnmute;
        return onRequest(request, response, muteUnmute,
                         [this, &response, &muteUnmute] () {
                             return onMuteUnmute(response, muteUnmute);});
    };

    /* video mix */
    m_httpServer.m_resources["^/api1/ial/mix_layout_change$"]["POST"] =
        [this] (shared_ptr<Response> & response, shared_ptr<Request> & request) {
        IalRequest::VideoMixChangeLayout layoutChange;
        return onRequest(request, response, layoutChange,
                         [this, &response, &layoutChange] () {
                             return onMixLayoutChange(response, layoutChange);});
    };

    m_httpServer.m_resources["^/api1/ial/mix_backgroud_update$"]["POST"] =
        [this] (shared_ptr<Response> & response, shared_ptr<Request> & request) {
        IalRequest::VideoMixUpdateBackgroud updateBg;
        return onRequest(request, response, updateBg,
                         [this, &response, &updateBg] () {
                             return onMixBackgroudUpdate(response, updateBg);});
    };

    /* Query infos */
    m_httpServer.m_resources["^/api1/ial/input_stream_info$"]["POST"] =
        [this] (shared_ptr<Response> & response, shared_ptr<Request> & request) {
        IalRequest::GetOneInputStreamInfo getInputInfo;
        return onRequest(request, response, getInputInfo,
                         [this, &response, &getInputInfo] () {
                             return onGetOneInputStreamInfo(response, getInputInfo);});
    };

    m_httpServer.m_resources["^/api1/ial/output_stream_info$"]["POST"] =
        [this] (shared_ptr<Response> & response, shared_ptr<Request> & request) {
        IalRequest::GetOneOutputStreamInfo getOutputInfo;
        return onRequest(request, response, getOutputInfo,
                         [this, &response, &getOutputInfo] () {
                             return onGetOneOutputStreamInfo(response, getOutputInfo);});
    };

    /* stop */
    m_httpServer.m_resources["^/api1/ial/stop$"]["POST"] =
        [this](shared_ptr<Response> & response, shared_ptr<Request> & request) {
        return onIalStop(response, request);
    };

    /* onerror */
    m_httpServer.m_onError = [this](shared_ptr <Request> & request, const error_code & e) {
        return onRequestError(request, e);
    };

    LOG(INFO) << m_logtag << "register all request handle done";
    return 0;
}

////////////////////
// [trivial helpers]
int IalHttpService::requestToMessage(shared_ptr<Request> & request, pb::Message & pbmsg) {
    int ret = 0;
    asio::streambuf::const_buffers_type cbt = request->m_request.data();
    const string jsonstr(asio::buffers_begin(cbt), asio::buffers_end(cbt));
    ret = PbTree::pbFromJsonString(pbmsg, jsonstr);
    if (ret < 0) {
        ERRORIT(IAL_ERROR_PARSE_REQUEST, "Fail load to pb object: " + PbTree::errToString(ret));
        return IAL_ERROR_PARSE_REQUEST;
    }
    LOG(INFO) << m_logtag << request->m_path << ",  request parse done: " << jsonstr;
    return 0;
}

int IalHttpService::failResponse(shared_ptr<Response> & response, const int errCode,
                             const string & errDetail, const bool bSync) {
    IalRequest::CommonResponse failCR;
    string failCRStr;
    failCR.set_code(errCode);
    failCR.set_msg(errDetail);
    failCR.set_b_sync_resp(bSync);
    PbTree::pbToJsonString(failCR, failCRStr);
    response->write(failCRStr);
    return 0;
}

int IalHttpService::sanityCheckCreateRoomMsg(const IalRequest::CreateRoom & createRoom) {
    if (createRoom.room_id().empty()) {
        ERRORIT(IAL_ERROR_CREATE_ROOM, "incoming msg no valid room_id");
        return API_ERRCODE_INVALID_MSG;
    }
    if (createRoom.input_urls().size() > m_ialGlobalSetting.max_participants() ) {
        ERRORIT(IAL_ERROR_EXCEED_MAX_JOINS, "CreateRoom containes too many input urls");
        return API_ERRCODE_INVALID_MSG;
    }
    /* output check */
    bool hasDefaultOutputBaseUrl = true;
    if (createRoom.room_output_base_url().empty()) {
        hasDefaultOutputBaseUrl = false;
    }
    const auto & outInfos = createRoom.output_stream_infos();
    for (const auto & oneOutputInfo : outInfos) {
        const auto & oid = oneOutputInfo.output_setting_id();
        if (m_outputSettings.count(oid) == 0 && !oneOutputInfo.has_specific_setting()) {
            ERRORIT(IAL_ERROR_OUTPUT_ID_NOT_FOUND,
                    oid + " output setting not found and no specific setting, add it first");
            return API_ERRCODE_INVALID_MSG;
        }
        if (!hasDefaultOutputBaseUrl && (oneOutputInfo.output_urls().size() == 0)) {
            ERRORIT(IAL_ERROR_OUTPUT_URL_NOT_FOUND,
                    "no default output base url and no pass down full output url");
            return API_ERRCODE_INVALID_MSG;
        }
        const auto & outSettingUsed =
            oneOutputInfo.has_specific_setting() ? oneOutputInfo.specific_setting() : m_outputSettings.at(oid);
        if (!hasDefaultOutputBaseUrl &&
            (oneOutputInfo.output_urls().size() < outSettingUsed.mux_outputs().size())) {
            ERRORIT(IAL_ERROR_OUTPUT_URL_NOT_FOUND,
                    "no default output base url and pass down full output url size less than mux outputs size");
            return API_ERRCODE_INVALID_MSG;
        }
    }
    return 0;
}

vector<string> IalHttpService::mkFullOutputUrl(const string & outputBaseUrl,
                     const string & roomId, const string & outputId,
                     const DavStreamletSetting::OutputStreamletSetting & outputStreamletSetting) {
    vector<string> outputUrls;
    for (size_t k=0; k < outputStreamletSetting.mux_outputs().size(); k++) {
        // TODO: may add an extension m.mux_fmt();
        const string oneOutUrl = outputBaseUrl + "/" + roomId + "_" + outputId + "_" + std::to_string(k);
        LOG(INFO) << m_logtag << "--> one out " << oneOutUrl;
        outputUrls.emplace_back(oneOutUrl);
    }
    return outputUrls;
}

} // namespace ial_service
