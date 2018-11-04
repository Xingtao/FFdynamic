#pragma once

#include "appService.h"
#include "httpCommon.h"
#include "httpServer.h"
#include "pbToDynaDetectOption.h"
#include "dynaDnnDetect.pb.h"

namespace dyna_detect_service {
using namespace http_util;
using namespace app_common;
using namespace ff_dynamic;
using namespace ::google::protobuf;

///////////////////////////////////////////////////////////////////////////
/* extend Error Description in DavMessager & AppMessager for DynaDetect service */
#define DYNA_DETECT_ERROR_NO_SUCH_DETECTOR  FFERRTAG('E', 'N', 'S', 'D')

extern string dynaDetectServiceErr2Str(const int errNum);

struct DynaDetectMessager : public AppMessager {
    DynaDetectMessager() = default;
    DynaDetectMessager(const int msgCode, const string & detail) {setInfo(msgCode, detail);}
    inline virtual string msg2str(const int reportNum) {
        return dynaDetectServiceErr2Str(reportNum);
    }
};

////////////////
// [DynaDetect Service]
class DynaDetectService : public AppService {
public:
    static DynaDetectService & getOnlyInstance() {
        static DynaDetectService s_instance;
        return s_instance;
    }
    virtual ~DynaDetectService() = default;
    int init(const string & configPath) {
        std::lock_guard<mutex> lock(m_runLock);
        int ret = AppService::init(configPath, m_dynaDetectConfig);
        if (ret < 0)
            return ret;
        m_dynaDetectGlobalSetting.CopyFrom(m_dynaDetectConfig.dynaDetect_global_setting());
         /* log parsed app config */
        string globalSetting;
        PbTree::pbToJsonString(m_appGlobalSetting, globalSetting);
        string inputSetting;
        PbTree::pbToJsonString(m_inputSetting, inputSetting);
        string mixSetting;
        PbTree::pbToJsonString(m_mixSetting, mixSetting);
        LOG(INFO) << m_logtag << "AppConfig - \n";
        LOG(INFO) << globalSetting << "\n";
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
    DynaDetectService() = default;
    DynaDetectService(const DynaDetectService &) = default;
    DynaDetectService & operator=(const DynaDetectService &) = default;

private:
    /* DynaDetect */
    DynaDetectConfig::DynaDetectTaskConfig m_dynaDetectConfig; /* original configuration, load from config file */
     /* break down from dynaDetect config for convinience: add/update/delete operates on following fields */
    DavStreamletSetting::InputStreamletSetting m_inputSetting;
    DynaDetectConfig::DynaDetectGlobalSetting m_dynaDetectGlobalSetting;

private: /* Dynamic Request Process Handlers */
    int onRequest(shared_ptr<Request> & request, shared_ptr<Response> & response, pb::Message & pbmsg,
                  std::function<int()> requestProcess, bool bNeedRoomIdExist = true);
    virtual int registerHttpRequestHandlers();
    /* setting */
    int onUpdateInputSetting(shared_ptr<Response> &, const DynaDetectRequest::UpdateInputSetting &);
    /* dynaDetect stop */
    int onDynaDetectStop(shared_ptr<Response> &, shared_ptr<Request> &);

private: /* build streamlets */
    int buildInputStreamlet(const string & inputUrl,
                            const DavStreamletSetting::InputStreamletSetting & inputSetting);
    int asyncBuildInputStreamlet(const string & inputUrl,
                                 const DavStreamletSetting::InputStreamletSetting & inputSetting);
    int buildMixStreamlet(const string & mixStreamletId);
    int buildOutputStreamlet(const string & outputId,
                             const DavStreamletSetting::OutputStreamletSetting & outStreamletSetting,
                             const vector<string> & fullOutputUrls);
};

} // namespace dyna_detect_service
