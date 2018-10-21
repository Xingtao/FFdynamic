#include <stdarg.h>
#include <unistd.h>
#include <algorithm>
#include "ialService.h"

namespace ial_service {

////////////////////////////////////////////////////////////////////////////////
IalMsgCollector & IalService::s_ialMsgCollector = IalService::getIalMsgCollector();

////////////////////////////////////////////////////////////////////////////////
static const std::map<int, const char *> ialServiceErrorCode2StrMap = {
    {IAL_ERROR_PARSE_REQUEST,            "Failld parse http request"},
    {IAL_ERROR_LOAD_CONFIG_FILE,         "Fail load configuration file"},
    {IAL_ERROR_JSONSTR_TO_PBOBJ,         "Fail parse json string to pb object"},
    {IAL_ERROR_PROCESS_REQUEST,          "Fail process incoming request"},
    {IAL_ERROR_EXCEED_MAX_JOINS,         "Inputs exceed max participants"},
    {IAL_ERROR_OUTPUT_ID_NOT_FOUND,      "Fail find output setting id"},
    {IAL_ERROR_OUTPUT_URL_NOT_FOUND,     "Fail get output full url"},
    {IAL_ERROR_BUILD_STREAMLET,          "Fail build streamlet"},
    {IAL_ERROR_CREATE_ROOM,              "Create room fail"},
    {IAL_ERROR_PARTICIPANT_JOIN,         "Participant join fail"},
    {IAL_ERROR_PARTICIPANT_LEFT,         "Participant left fail"},
    {IAL_ERROR_OUTSTREAM_ADD_CLOSE     , "Output stream add/close fail"},
    {IAL_ERROR_STREAM_INFO_QUERY,        "Input/Output stream info query fail"},
    {IAL_ERROR_PARTICIPANT_MUTE_UNMUTE,  "Participant mute/unMute fail"},
    {IAL_ERROR_VIDEO_FILTER_QUERY,       "Video filter query fail"},
    {IAL_ERROR_VIDEO_FILTER_CHANGE,      "Video filter change fail"},
    {IAL_ERROR_MIX_LAYOUT_CHANGE,        "Video mix layout change fail"},
    {IAL_ERROR_MUTE_UNMUTE,              "Audio mix mute/unmute fail"},
    {IAL_ERROR_MIX_SET_BACKGROUD,        "Video mix set back groud fail"}
};

string ialServiceErr2Str(const int errCode) {
    auto it = std::find_if(ialServiceErrorCode2StrMap.begin(), ialServiceErrorCode2StrMap.end(),
                           [errCode](const std::pair<int, const char *> & item) {
                               return item.first == errCode;});
    if (it != ialServiceErrorCode2StrMap.end())
        return string(it->second);
    return davMsg2str(errCode);
}

std::ostream & operator<<(std::ostream & os, const IalMessager & msg) {
    os << "{code:" << msg.m_msgCode << ", detail: " << msg.m_msgDetail << "}";
    return os;
}

static int s_ffmpegLogLevel = 0;

////////////////////////////////////////////////////////////////////////////////
int IalService::init(const string & configPath) {
    string configContent;
    int ret = IalUtil::readFileContent(configPath, configContent);
    if (ret < 0 || configContent.empty()) {
        ERRORIT(IAL_ERROR_LOAD_CONFIG_FILE, "please check config path " + configPath);
        return IAL_ERROR_LOAD_CONFIG_FILE;
    }
    ret = PbTree::pbFromJsonString(m_ialConfig, configContent, false);
    if (ret < 0) {
        INFOIT(IAL_ERROR_JSONSTR_TO_PBOBJ,
               "Load config file to IalTaskConfig (no ingore fields fail): " + PbTree::errToString(ret));
        ret = PbTree::pbFromJsonString(m_ialConfig, configContent, true); /* ignore unknown */
        if (ret >= 0) {
            LOG(INFO) << m_logtag << "there are unknown/missing fields in config file; "
                      << "it is ok if intended. ingore and continue";
        } else {
            LOG(ERROR) << m_logtag << configContent;
            return IAL_ERROR_JSONSTR_TO_PBOBJ;
        }
    }
    m_ialGlobalSetting.CopyFrom(m_ialConfig.global_setting());
    m_inputSetting.CopyFrom(m_ialConfig.input_setting());
    m_mixSetting.CopyFrom(m_ialConfig.mix_setting());
    for (auto & o : m_ialConfig.output_settings())
        m_outputSettings.emplace(o.first, o.second);

    /* log setting */
    doLogSetting();

    /* prepare a common cr */
    m_successCR.set_code(0);
    m_successCR.set_msg("ok");
    m_successCR.set_b_sync_resp(true);
    PbTree::pbToJsonString(m_successCR, m_successCRJsonStr);
    m_successCR.set_b_sync_resp(false);
    PbTree::pbToJsonString(m_successCR, m_successCRJsonStrAsync);

    /* log parsed ial config */
    string globalSetting;
    PbTree::pbToJsonString(m_ialGlobalSetting, globalSetting);
    string inputSetting;
    PbTree::pbToJsonString(m_inputSetting, inputSetting);
    string mixSetting;
    PbTree::pbToJsonString(m_mixSetting, mixSetting);
    LOG(INFO) << m_logtag << "IalConfig - \n";
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

////////////////////////////////////////////////////////////////////////////////
// ial msg reporter: dump to log, send to log server or whatever
int IalService::ialMsgReport() {
    LOG(INFO) << m_logtag << "ial msg reporter run";
    do{
        ialMsgJsonReport(s_ialMsgCollector.getMsg());
    } while(!m_bIalExit);
    LOG(INFO) << m_logtag << "ial msg reporter finish";
    return 0;
}

int IalService::ialMonitor(std::function<int()> afterMonitorDone) {
    std::chrono::microseconds timeout(3000000); // 3000ms
    do {
        std::unique_lock<std::mutex> uniqueGuard(m_runLock);
        m_monitorCV.wait_for(uniqueGuard, timeout, [this]() {return (m_bMonitorCheck ? true : false);});
        m_bMonitorCheck = false;
        /* timeout check or event driven check stopped streamlet and then erase it */
        vector<DavStreamletTag> stopped;
        auto streamlets = m_river.getStreamlets();
        for (const auto & s : streamlets)
            if (s->isStopped())
                stopped.push_back(s->getTag());
        for (const auto & e : stopped)
            m_river.erase(e);
    } while (!m_bIalExit);

    m_river.stop();
    m_river.clear();

    /* at last */
    afterMonitorDone();
    return 0;
}

static google::LogSeverity toGlogSeverity(string logLevel) {
    static const map<string, int> glogSeverityMap = {
        {"info", google::INFO},
        {"warning", google::WARNING},
        {"error", google::ERROR},
        {"fatal", google::FATAL}
    };
    std::transform(logLevel.begin(), logLevel.end(), logLevel.begin(),
                   [](char c) {return std::tolower(c);});
    if (!glogSeverityMap.count(logLevel)) {
        return google::INFO;
    }
    return glogSeverityMap.at(logLevel);
}

static int toFFmpegLogSeverity(string logLevel) {
    static const map<string, int> ffmpegLogSeverityMap = {
        {"trace", AV_LOG_TRACE}, // 56
        {"debug", AV_LOG_DEBUG}, // 48
        {"verbose", AV_LOG_VERBOSE}, // 40
        {"info", AV_LOG_INFO}, // 32
        {"warning", AV_LOG_WARNING}, // 24
        {"error", AV_LOG_ERROR}, // 16
        {"fatal", AV_LOG_FATAL}, // 8
        {"error", AV_LOG_PANIC}, // 0
        {"quiet", AV_LOG_QUIET}  // -8
    };

    std::transform(logLevel.begin(), logLevel.end(), logLevel.begin(),
                   [](char c) {return std::tolower(c);});
    if (!ffmpegLogSeverityMap.count(logLevel)) {
        return AV_LOG_ERROR;
    }
    return ffmpegLogSeverityMap.at(logLevel);
}

int IalService::doLogSetting() {
    /* setup glog first */
    const auto & glogdir = m_ialGlobalSetting.glog_save_path();
    LOG(INFO) << m_logtag << "Glog Config with log dir " << glogdir;
    const string mkdirCmd = "mkdir -p " + glogdir;
    system(mkdirCmd.c_str());
    const string & glogLevel = m_ialGlobalSetting.glog_save_level();

    google::InstallFailureSignalHandler();
    /* google::InstallFailureWriter(&sighandles); */
    google::LogSeverity level = toGlogSeverity(glogLevel);
    /* add log prefix with severity */
    if (level < google::ERROR)
       google::SetLogDestination(level, (glogdir + "/" + glogLevel + "_").c_str());
    /* output error alone anyway */
    google::SetLogDestination(google::ERROR, (glogdir + "/error_").c_str());
    // google::SetLogFilenameExtension(".ffd.log");

    /* */
    FLAGS_stderrthreshold = 0;
    FLAGS_alsologtostderr=1;
    FLAGS_colorlogtostderr = true; /* colorful std output */
    FLAGS_logbufsecs = 1;
    FLAGS_max_log_size = 200;
    FLAGS_stop_logging_if_full_disk = true;

    /* setup ffmpeg log */
    s_ffmpegLogLevel = toFFmpegLogSeverity(m_ialGlobalSetting.ffmpeg_log_level());
    av_log_set_callback([] (void *ptr, int level, const char *fmt, va_list vl) {
        if (level > s_ffmpegLogLevel)
            return ;
        char message[8192];
        const char *ffmpegModule = nullptr;
        if (ptr) {
            AVClass *avc = *(AVClass**) ptr;
            if (avc->item_name)
                ffmpegModule = avc->item_name(ptr);
        }
        vsnprintf(message, sizeof(message), fmt, vl);
        LOG(WARNING) << "[FFMPEG][" << (ffmpegModule ? ffmpegModule : "") << "]" << message;
    });

    return 0;
}


////////////////////////////////////////////////////////////////////////////////
// trival helpers
int IalService::ialMsgJsonReport(const shared_ptr<AVDictionary> & msg) {
    usleep(50000);
    return 0;
}

int IalService::ialMsgProtobufReport(const shared_ptr<AVDictionary> & msg) {
    return 0;
}

} // namespace ial_service
