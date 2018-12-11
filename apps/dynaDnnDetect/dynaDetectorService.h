#pragma once

#include "httpCommon.h"
#include "httpServer.h"
#include "davStreamlet.h"
#include "objDetectStreamlet.h"
#include "pbToDynaDetectOption.h"
#include "dynaDnnDetect.pb.h"
#include "appService.h"

namespace dyna_detect_service {
using namespace ff_dynamic;
using namespace http_util;
using namespace app_common;
using namespace ::google::protobuf;

///////////////////////////////////////////////////////////////////////////
/* extend Error Description in DavMessager & AppMessager for DynaDetect service */
#define DYNA_DETECT_ERROR_TASK_INIT                FFERRTAG('F', 'D', 'T', 'I')
#define DYNA_DETECT_ERROR_NO_SUCH_DETECTOR         FFERRTAG('E', 'N', 'S', 'D')
#define DYNA_DETECT_ERROR_DETECTOR_ALREADY_RUNNING FFERRTAG('E', 'D', 'A', 'R')
#define DYNA_DETECT_ERROR_DETECTOR_ALREADY_STOPPED FFERRTAG('E', 'D', 'A', 'S')
#define DYNA_DETECT_ERROR_DETECTOR_ADD             FFERRTAG('E', 'D', 'A', 'D')
#define DYNA_DETECT_ERROR_DETECTOR_DELETE          FFERRTAG('E', 'D', 'D', 'L')

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
        int ret = AppService::init(configPath, m_config);
        if (ret < 0)
            return ret;
        m_dynaDetectGlobalSetting.CopyFrom(m_config.dyna_detect_global_setting());
        m_inputSetting.CopyFrom(m_config.input_setting());
        m_outputSetting.CopyFrom(m_config.output_setting());
        for (auto & s : m_config.dnn_detector_settings()) {
            m_dnnDetectorSettings.emplace(s.first, s.second);
            m_detectors.emplace(s.first, s.second.enable());
        }

         /* log parsed app config */
        string globalSetting;
        PbTree::pbToJsonString(m_appGlobalSetting, globalSetting);
        string inputSetting;
        PbTree::pbToJsonString(m_inputSetting, inputSetting);
        string outputSetting;
        PbTree::pbToJsonString(m_outputSetting, outputSetting);
        LOG(INFO) << m_logtag << "DynaDetectConfig - \n";
        LOG(INFO) << globalSetting << "\n";
        LOG(INFO) << inputSetting << "\n";
        LOG(INFO) << outputSetting << "\n";
        for (const auto & o : m_dnnDetectorSettings) {
            string dnnDetectorSettings;
            PbTree::pbToJsonString(o.second, outputSetting);
            LOG(INFO) << o.first << " -> " << outputSetting << "\n";
        }
        return createTask();
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
    DynaDnnDetect::DynaDnnDetectTaskConfig m_config; /* original configuration, load from config file */
    DynaDnnDetect::DynaDetectGlobalSetting m_dynaDetectGlobalSetting;
    map<string, DynaDnnDetect::DnnDetectSetting> m_dnnDetectorSettings;
     /* break down from dynaDetect config for convinience: add/update/delete operates on following fields */
    DavStreamletSetting::InputStreamletSetting m_inputSetting;
    DavStreamletSetting::OutputStreamletSetting m_outputSetting;
    map<string, bool> m_detectors; /* this is loaded from configure file */
    const string m_dnnDetectStreamletName {"dnnDetectStreamletName"};

private: /* Dynamic Request Process Handlers */
    int createTask();
    int buildDynaDetectStreamlet();
    virtual int registerHttpRequestHandlers();
    /* add/delete detector */
    int onAddOneDetector(shared_ptr<Response> &, const string &);
    int onDeleteOneDetector(shared_ptr<Response> &, const string &);
    /* dynaDetect stop */
    int onDynaDetectStop(shared_ptr<Response> &, shared_ptr<Request> &);
};

} // namespace dyna_detect_service
