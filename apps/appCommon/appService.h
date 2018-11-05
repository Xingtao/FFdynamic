#include <vector>
#include <memory>
#include <string>
#include <mutex>
#include <chrono>
#include <condition_variable>
#include <thread>
#include <algorithm>
#include <future>

#include <glog/logging.h>

#include "davMessager.h"
#include "davStreamlet.h"
#include "pbtree.h"
#include "httpServer.h"
#include "appUtil.h"
#include "appGlobalSetting.pb.h"
#include "davStreamlet.h"
#include "davStreamletBuilder.h"
#include "pbToDavOptionEvent.h"

namespace app_common {
using ::std::string;
using ::std::vector;
using ::std::mutex;
using ::std::shared_ptr;
using namespace pb_tree;
using namespace http_util;
using namespace ff_dynamic;
using AppMsgCollector = DavMsgCollector;

//////////////////////////////////////////////////////////////////////////////////
/* extend Error Description in DavMessager for app service */
#define APP_ERROR_LOAD_CONFIG_FILE  FFERRTAG('E', 'L', 'C', 'F')
#define APP_ERROR_JSONSTR_TO_PBOBJ  FFERRTAG('E', 'J', 'T', 'P')
#define APP_ERROR_PARSE_REQUEST     FFERRTAG('F', 'P', 'R', 'Q')
#define APP_ERROR_PROCESS_REQUEST   FFERRTAG('E', 'P', 'R', 'Q')
#define APP_ERROR_HTTP_SETTING      FFERRTAG('E', 'H', 'T', 'S')
#define APP_ERROR_BUILD_STREAMLET   FFERRTAG('E', 'B', 'S', 'L')

extern string appServiceErr2Str(const int errNum);
struct AppMessager : public DavMessager {
    AppMessager() = default;
    AppMessager(const int msgCode, const string & detail) {setInfo(msgCode, detail);}
    inline virtual string msg2str(const int reportNum) {
        return appServiceErr2Str(reportNum);
    }
};

//////////////////
class AppService {
public:
    AppService() = default;
    virtual ~AppService() {
        std::lock_guard<mutex> lock(m_runLock);
        m_river.clear();
    };

public:
    inline virtual bool isStopped() {return m_river.isStopped();}
    inline DavMsgError & getErr() {return m_appInfo;}

protected: /* log and msg report related */
    static AppMsgCollector & s_appMsgCollector;
    static AppMsgCollector & getAppMsgCollector() {return DavMsgCollector::getInstance();}
    std::future<int> m_msgCollectorFuture;
    int appMsgReport();
    int appMsgJsonReport(const shared_ptr<AVDictionary> & msg);
    int appMsgProtobufReport(const shared_ptr<AVDictionary> & msg);
    int doLogSetting();
    #undef ERRORIT
    #undef INFOIT
    #define ERRORIT(code, detail) {m_appInfo.setInfo(code, detail);\
            s_appMsgCollector.addMsg(m_appInfo);LOG(ERROR) << m_logtag << m_appInfo;}
    #define INFOIT(code, detail) {m_appInfo.setInfo(code, detail); \
            s_appMsgCollector.addMsg(m_appInfo);LOG(INFO) << m_logtag << m_appInfo;}

protected:
    inline int setExit() {m_bExit = true; return 0;}
    virtual int start() {
        m_msgCollectorFuture = std::async(std::launch::async, [this]() {return appMsgReport();});
        auto afterMonitorDone = [this](){m_httpServer.stop(); return 0;};
        m_monitorFuture = std::async(std::launch::async,
                                     [this, afterMonitorDone] () {return riverMonitor(afterMonitorDone);});
        if (!m_httpIp.empty()) {
            m_httpServer.init(m_httpIp, m_httpPort);
            LOG(INFO) << m_logtag << "Http Server Start";
            m_httpServer.start(); /* thread block here*/
        } else {
            return APP_ERROR_HTTP_SETTING;
        }
        return 0;
    };

    template <typename T>
    int init(const string & configPath, T & configPbObj) {
        string configContent;
        int ret = AppUtil::readFileContent(configPath, configContent);
        if (ret < 0 || configContent.empty()) {
            ERRORIT(APP_ERROR_LOAD_CONFIG_FILE, "please check config path " + configPath);
            return APP_ERROR_LOAD_CONFIG_FILE;
        }
        ret = PbTree::pbFromJsonString(configPbObj, configContent, false);
        if (ret < 0) {
            INFOIT(APP_ERROR_JSONSTR_TO_PBOBJ,
                   "Load config file to AppTaskConfig (no ingore fields fail): " + PbTree::errToString(ret));
            ret = PbTree::pbFromJsonString(configPbObj, configContent, true); /* ignore unknown */
            if (ret >= 0) {
                LOG(INFO) << m_logtag << "there are unknown/missing fields in config file; "
                          << "it is ok if intended. ingore and continue";
            } else {
                LOG(ERROR) << m_logtag << configContent;
                return APP_ERROR_JSONSTR_TO_PBOBJ;
            }
        }
        m_appGlobalSetting.CopyFrom(configPbObj.app_global_setting());
        string appGlobalSetting;
        PbTree::pbToJsonString(m_appGlobalSetting, appGlobalSetting);
        LOG(INFO) << "AppGlobalSetting " << appGlobalSetting;
        m_httpIp = m_appGlobalSetting.http_server_addr();
        m_httpPort = (int16_t)m_appGlobalSetting.http_server_port();
        /* log setting */
        doLogSetting();
        /* prepare a common cr */
        m_successCR.set_code(0);
        m_successCR.set_msg("ok");
        m_successCR.set_b_sync_resp(true);
        PbTree::pbToJsonString(m_successCR, m_successCRJsonStr);
        m_successCR.set_b_sync_resp(false);
        PbTree::pbToJsonString(m_successCR, m_successCRJsonStrAsync);
        return 0;
    }
    int riverMonitor(std::function<int()> afterMonitorDone);

protected: /* http interface */
    HttpServer m_httpServer;
    virtual int registerHttpRequestHandlers() {return 0;};
    virtual int requestToMessage(shared_ptr<Request> & request, google::protobuf::Message & pbmsg);
    virtual int failResponse(shared_ptr<Response> & response, const int errCode,
                             const string & errDetail, const bool bSync = true);
    virtual int afterHttpResponse() {m_bMonitorCheck = true; m_monitorCV.notify_one(); return 0;}
    /* default on error */
    virtual int onRequestError(shared_ptr<Request> & request, const error_code & ec) {
        asio::streambuf::const_buffers_type cbt = request->m_request.data();
        string requeststr(asio::buffers_begin(cbt), asio::buffers_end(cbt));
        ERRORIT(APP_ERROR_PROCESS_REQUEST, ec.message() + ", " + requeststr);
        return 0;
    }
    string m_httpIp;
    int16_t m_httpPort = -1;
    AppGlobalSetting::CommonResponse m_successCR;
    string m_successCRJsonStr;
    string m_successCRJsonStrAsync;

protected: /* provides few default streamlet builders */
    virtual int buildInputStreamlet(const string & inputUrl,
                                    const DavStreamletSetting::InputStreamletSetting & inputSetting);
    virtual int asyncBuildInputStreamlet(const string & inputUrl,
                                         const DavStreamletSetting::InputStreamletSetting & inputSetting,
                                         std::function<int(shared_ptr<DavStreamlet>)> onBuildSuccess);
    virtual int buildMixStreamlet(const string & mixStreamletId,
                                  const DavStreamletSetting::MixStreamletSetting & mixSetting);
    virtual int buildOutputStreamlet(const string & outputId,
                                     const DavStreamletSetting::OutputStreamletSetting & outStreamletSetting,
                                     const vector<string> & fullOutputUrls);
protected:
    AppGlobalSetting::GlobalSetting m_appGlobalSetting;
    DavRiver m_river; /* a set of all implementations */
    std::atomic<bool> m_bExit = ATOMIC_VAR_INIT(false);
    std::mutex m_runLock;
    std::condition_variable m_monitorCV;
    std::future<int> m_monitorFuture;
    std::atomic<bool> m_bMonitorCheck = ATOMIC_VAR_INIT(false);
    string m_logtag {"[AppService] "};
    AppMessager m_appInfo; /* process log/events */
};

} // namespace
