#pragma once

#include "appService.h"
#include "httpCommon.h"
#include "httpServer.h"
#include "ialConfig.pb.h"
#include "ialRequest.pb.h"

namespace ial_service {
using namespace http_util;
using namespace app_common;
using namespace ff_dynamic;
using namespace ::google::protobuf;

//////////////////////////////////////////////////////////////////////////////////
/* extend Error Description in DavMessager & AppMessager for Ial service */
#define IAL_ERROR_EXCEED_MAX_JOINS         FFERRTAG('E', 'E', 'X', 'M')
#define IAL_ERROR_OUTPUT_ID_NOT_FOUND      FFERRTAG('O', 'I', 'N', 'F')
#define IAL_ERROR_OUTPUT_URL_NOT_FOUND     FFERRTAG('O', 'U', 'N', 'F')
#define IAL_ERROR_CREATE_ROOM              FFERRTAG('C', 'R', 'O', 'M')
#define IAL_ERROR_PARTICIPANT_JOIN         FFERRTAG('P', 'C', 'J', 'I')
#define IAL_ERROR_PARTICIPANT_LEFT         FFERRTAG('P', 'C', 'L', 'T')
#define IAL_ERROR_OUTSTREAM_ADD_CLOSE      FFERRTAG('O', 'S', 'A', 'C')
#define IAL_ERROR_STREAM_INFO_QUERY        FFERRTAG('S', 'I', 'Q', 'F')
#define IAL_ERROR_PARTICIPANT_MUTE_UNMUTE  FFERRTAG('P', 'M', 'U', 'F')
#define IAL_ERROR_VIDEO_FILTER_QUERY       FFERRTAG('V', 'F', 'Q', 'F')
#define IAL_ERROR_VIDEO_FILTER_CHANGE      FFERRTAG('V', 'F', 'C', 'F')
#define IAL_ERROR_MIX_LAYOUT_CHANGE        FFERRTAG('F', 'M', 'L', 'C')
#define IAL_ERROR_MUTE_UNMUTE              FFERRTAG('F', 'M', 'U', 'U')
#define IAL_ERROR_MIX_SET_BACKGROUD        FFERRTAG('S', 'M', 'B', 'G')

extern string ialServiceErr2Str(const int errNum);

struct IalMessager : public AppMessager {
    IalMessager() = default;
    IalMessager(const int msgCode, const string & detail) {setInfo(msgCode, detail);}
    inline virtual string msg2str(const int reportNum) {
        return ialServiceErr2Str(reportNum);
    }
};

////////////////
// [Ial Service]
class IalService : public AppService {
public:
    static IalService & getOnlyInstance() {
        static IalService s_instance;
        return s_instance;
    }
    virtual ~IalService() = default;
    int init(const string & configPath) {
        std::lock_guard<mutex> lock(m_runLock);
        int ret = AppService::init(configPath, m_ialConfig);
        if (ret < 0)
            return ret;
        m_ialGlobalSetting.CopyFrom(m_ialConfig.ial_global_setting());
        m_inputSetting.CopyFrom(m_ialConfig.input_setting());
        m_mixSetting.CopyFrom(m_ialConfig.mix_setting());
        for (auto & o : m_ialConfig.output_settings())
            m_outputSettings.emplace(o.first, o.second);
         /* log parsed app config */
        string ialGlobalSetting;
        PbTree::pbToJsonString(m_ialGlobalSetting, ialGlobalSetting);
        string inputSetting;
        PbTree::pbToJsonString(m_inputSetting, inputSetting);
        string mixSetting;
        PbTree::pbToJsonString(m_mixSetting, mixSetting);
        LOG(INFO) << m_logtag << "AppConfig - \n";
        LOG(INFO) << ialGlobalSetting << "\n";
        LOG(INFO) << inputSetting << "\n";
        LOG(INFO) << mixSetting << "\n";
        for (const auto & o : m_outputSettings) {
            string outputSetting;
            PbTree::pbToJsonString(o.second, outputSetting);
            LOG(INFO) << o.first << " -> " << outputSetting << "\n";
        }
       return 0;
    }

    virtual int start() {
        registerHttpRequestHandlers();
        AppService::start();
        return 0;
    }

private:
    IalService() = default;
    IalService(const IalService &) = default;
    IalService & operator=(const IalService &) = default;

private:
    /* Ial */
    IalConfig::IalTaskConfig m_ialConfig; /* original configuration, load from config file */
     /* break down from ial config for convinience: add/update/delete operates on following fields */
    DavStreamletSetting::InputStreamletSetting m_inputSetting;
    DavStreamletSetting::MixStreamletSetting m_mixSetting;
    /* an unique tag associated with one set of output settting */
    /* TODO: restriction here, one output setting id could only have one output streamlet;
             this is caused by lacking output full url when do the room creation and
             we use 'output setting id' as streamlet's tag */
    map<string, DavStreamletSetting::OutputStreamletSetting> m_outputSettings;
    IalConfig::IalGlobalSetting m_ialGlobalSetting;
    const string m_mixStreamletName {"avMixStreamlet"};
    string m_roomId;
    string m_roomOutputBaseUrl;

private: /* Dynamic Request Process Handlers */
    int onRequest(shared_ptr<Request> & request, shared_ptr<Response> & response, pb::Message & pbmsg,
                  std::function<int()> requestProcess, bool bNeedRoomIdExist = true);
    virtual int registerHttpRequestHandlers();
    /* setting */
    int onUpdateInputSetting(shared_ptr<Response> &, const IalRequest::UpdateInputSetting &);
    int onAddOutputSetting(shared_ptr<Response> &, const IalRequest::AddOutputSetting & );
    int onUpdateMixSetting(shared_ptr<Response> &, const IalRequest::UpdateMixSetting &);
    /* dynamic event */
    int onCreateRoom(shared_ptr<Response> &, const IalRequest::CreateRoom &);
    int onAddNewInputStream(shared_ptr<Response> &, const IalRequest::AddNewInputStream &);
    int onAddNewOutput(shared_ptr<Response> &, const IalRequest::AddNewOutput &);
    int onCloseOneInputStream(shared_ptr<Response> &, const IalRequest::CloseOneInputStream &);
    int onCloseOneOutput(shared_ptr<Response> &, const IalRequest::CloseOneOutput &);
    /* mix */
    int onMuteUnmute(shared_ptr<Response> &, const IalRequest::AudioMixMuteUnMute &);
    int onMixLayoutChange(shared_ptr<Response> &, const IalRequest::VideoMixChangeLayout &);
    int onMixBackgroudUpdate(shared_ptr<Response> &, const IalRequest::VideoMixUpdateBackgroud &);
    /* queries */
    int onGetOneInputStreamInfo(shared_ptr<Response> &, const IalRequest::GetOneInputStreamInfo &);
    int onGetOneOutputStreamInfo(shared_ptr<Response> &, const IalRequest::GetOneOutputStreamInfo &);
    /* ial stop */
    int onIalStop(shared_ptr<Response> &, shared_ptr<Request> &);

private: /* trivial ones */
    int sanityCheckCreateRoomMsg(const IalRequest::CreateRoom & createRoom);
    vector<string> mkFullOutputUrl(const string & outputBaseUrl, const string & roomId, const string & outputId,
                                   const DavStreamletSetting::OutputStreamletSetting & outputStreamletSetting);
};

} // namespace ial_service
