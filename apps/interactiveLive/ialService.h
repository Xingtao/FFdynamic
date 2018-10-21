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
#include "ialConfig.pb.h"
#include "ialRequest.pb.h"
#include "pbtree.h"
#include "ialUtil.h"

namespace ial_service {
using ::std::string;
using ::std::vector;
using ::std::mutex;
using ::std::shared_ptr;
using namespace pb_tree;
using namespace ff_dynamic;
using IalMsgCollector = DavMsgCollector;

//////////////////////////////////////////////////////////////////////////////////
/* extend Error Description in DavMessager for ial service */
#define IAL_ERROR_PARSE_REQUEST            FFERRTAG('F', 'P', 'R', 'Q')
#define IAL_ERROR_LOAD_CONFIG_FILE         FFERRTAG('E', 'L', 'C', 'F')
#define IAL_ERROR_JSONSTR_TO_PBOBJ         FFERRTAG('E', 'J', 'T', 'P')
#define IAL_ERROR_PROCESS_REQUEST          FFERRTAG('E', 'P', 'R', 'Q')
#define IAL_ERROR_EXCEED_MAX_JOINS         FFERRTAG('E', 'E', 'X', 'M')
#define IAL_ERROR_OUTPUT_ID_NOT_FOUND      FFERRTAG('O', 'I', 'N', 'F')
#define IAL_ERROR_OUTPUT_URL_NOT_FOUND     FFERRTAG('O', 'U', 'N', 'F')
#define IAL_ERROR_BUILD_STREAMLET          FFERRTAG('F', 'B', 'S', 'L')

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

struct IalMessager : public DavMessager {
    IalMessager() = default;
    IalMessager(const int msgCode, const string & detail) {setInfo(msgCode, detail);}
    inline virtual void setInfo(const int msgCode, const string & msgDetail = "") noexcept {
        m_msgCode = msgCode;
        m_msgDetail = ialServiceErr2Str(msgCode) + ", " + msgDetail;
    }
};

extern std::ostream & operator<<(std::ostream & os, const IalMessager & msg);

//////////////////
class IalService {
public:
    IalService() = default;
    virtual ~IalService() {
        std::lock_guard<mutex> lock(m_runLock);
        m_river.clear();
    };

public:
    virtual bool isStopped() {
        return m_river.isStopped();
    }
    inline DavMsgError & getErr() {return m_ialInfo;}

protected:
    virtual int init(const string & configPath);
    int startIal(std::function<int()> afterMonitorDone) {
        m_msgCollectorFuture = std::async(std::launch::async, [this]() {return ialMsgReport();});
        m_monitorFuture = std::async(std::launch::async,
                                     [this, afterMonitorDone]() {return ialMonitor(afterMonitorDone);});
        return 0;
    };
    int setIalExit() {
        m_bIalExit = true;
        return 0;
    }
    int ialMonitor(std::function<int()> afterMonitorDone);

protected:
    std::atomic<bool> m_bIalExit = ATOMIC_VAR_INIT(false);
    std::mutex m_runLock;
    std::condition_variable m_monitorCV;
    std::future<int> m_monitorFuture;
    std::atomic<bool> m_bMonitorCheck = ATOMIC_VAR_INIT(false);
    string m_logtag {"[IalService] "};
    IalMessager m_ialInfo; /* process log/events */
    /* the following three fields need clear after river task finish */
    DavRiver m_river;
    string m_roomId;
    string m_roomOutputBaseUrl;

protected:
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
    IalRequest::CommonResponse m_successCR;
    string m_successCRJsonStr;
    string m_successCRJsonStrAsync;
    const string m_mixStreamletName {"avMixStreamlet"};

protected: /* log and msg report */
    static IalMsgCollector & s_ialMsgCollector;
    static IalMsgCollector & getIalMsgCollector() {return DavMsgCollector::getInstance();}
    std::future<int> m_msgCollectorFuture;
    int ialMsgReport();
    int ialMsgJsonReport(const shared_ptr<AVDictionary> & msg);
    int ialMsgProtobufReport(const shared_ptr<AVDictionary> & msg);

    int doLogSetting();
    #undef ERRORIT
    #undef INFOIT
    #define ERRORIT(code, detail) {m_ialInfo.setInfo(code, detail);\
            s_ialMsgCollector.addMsg(m_ialInfo);LOG(ERROR) << m_logtag << m_ialInfo;}
    #define INFOIT(code, detail) {m_ialInfo.setInfo(code, detail); \
            s_ialMsgCollector.addMsg(m_ialInfo);LOG(INFO) << m_logtag << m_ialInfo;}
};

} // namespace
