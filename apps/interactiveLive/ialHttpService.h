#pragma once

#include "ialService.h"
#include "httpCommon.h"
#include "httpServer.h"

namespace ial_service {
using namespace http_util;
using namespace ff_dynamic;
using namespace ::google::protobuf;

class IalHttpService : public IalService {
public:
    static IalHttpService & getOnlyInstance() {
        static IalHttpService s_instance;
        return s_instance;
    }
    virtual ~IalHttpService() = default;
    virtual int init(const string & configPath) {
        std::lock_guard<mutex> lock(m_runLock);
        int ret = IalService::init(configPath);
        if (ret < 0)
            return ret;
        /* Http part setup */
        m_httpServer.init(m_ialGlobalSetting.http_server_addr(),
                          (int16_t)m_ialGlobalSetting.http_server_port());
        registerHttpRequestHandlers();
        return 0;
    }
    int start() {
        IalService::startIal([this]() {m_httpServer.stop(); return 0;});
        LOG(INFO) << m_logtag << "Ial Http Server Start\n";
        m_httpServer.start();
        return 0;
    }

private:
    IalHttpService() = default;
    IalHttpService(const IalHttpService &) = default;
    IalHttpService & operator=(const IalHttpService &) = default;
    HttpServer m_httpServer;

private: /* Dynamic Request Process Handlers */
    int registerHttpRequestHandlers();
    int onRequest(shared_ptr<Request> & request, shared_ptr<Response> & reponse, pb::Message & pbmsg,
                  std::function<int()> process, bool bNeedRoomIdExist = true);
    int afterResponse();
    int requestToMessage(shared_ptr<Request> & request, pb::Message & pbmsg);

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
    /* on error */
    int onRequestError(shared_ptr<Request> & request, const error_code & e);

private: /* build streamlets */
    int buildInputStreamlet(const string & inputUrl,
                            const DavStreamletSetting::InputStreamletSetting & inputSetting);
    int asyncBuildInputStreamlet(const string & inputUrl,
                                 const DavStreamletSetting::InputStreamletSetting & inputSetting);
    int buildMixStreamlet(const string & mixStreamletId);
    int buildOutputStreamlet(const string & outputId,
                             const DavStreamletSetting::OutputStreamletSetting & outStreamletSetting,
                             const vector<string> & fullOutputUrls);

private: /* trivial ones */
    int sanityCheckCreateRoomMsg(const IalRequest::CreateRoom & createRoom);
    int failResponse(shared_ptr<Response> & response, const int errCode,
                     const string & errDetail, const bool bSync = true);
    vector<string> mkFullOutputUrl(const string & outputBaseUrl, const string & roomId, const string & outputId,
                                   const DavStreamletSetting::OutputStreamletSetting & outputStreamletSetting);
};

} // namespace ial_service
