#include <stdarg.h>
#include <unistd.h>
#include <algorithm>
#include "appService.h"

namespace app_common {

////////////////////////////////////////////////////////////////////////////////
static int s_ffmpegLogLevel = 0;
AppMsgCollector & AppService::s_appMsgCollector = AppService::getAppMsgCollector();

static const std::map<int, const char *> appServiceErrorCode2StrMap = {
    {APP_ERROR_PARSE_REQUEST,            "Fail parse http request"},
    {APP_ERROR_LOAD_CONFIG_FILE,         "Fail load configuration file"},
    {APP_ERROR_JSONSTR_TO_PBOBJ,         "Fail parse json string to pb object"},
    {APP_ERROR_PROCESS_REQUEST,          "Fail process incoming request"},
    {APP_ERROR_HTTP_SETTING,             "Invalied https settings"},
    {APP_ERROR_BUILD_STREAMLET,          "Fail build streamlet"}
};

string appServiceErr2Str(const int errCode) {
    auto it = std::find_if(appServiceErrorCode2StrMap.begin(), appServiceErrorCode2StrMap.end(),
                           [errCode](const std::pair<int, const char *> & item) {
                               return item.first == errCode;});
    if (it != appServiceErrorCode2StrMap.end())
        return string(it->second);
    return davMsg2str(errCode);
}

////////////////////////////////////////////////////////////////////////////////
// app msg reporter: dump to log, send to log server or whatever
int AppService::appMsgReport() {
    LOG(INFO) << m_logtag << "app msg reporter run";
    do{
        appMsgJsonReport(s_appMsgCollector.getMsg());
    } while(!m_bExit);
    LOG(INFO) << m_logtag << "app msg reporter finish";
    return 0;
}

int AppService::riverMonitor(std::function<int()> afterMonitorDone) {
    std::chrono::microseconds timeInterval(3000000); // 3000ms
    do {
        std::unique_lock<std::mutex> uniqueGuard(m_runLock);
        m_monitorCV.wait_for(uniqueGuard, timeInterval, [this]() {return (m_bMonitorCheck ? true : false);});
        m_bMonitorCheck = false;
        /* timeout check or event driven check stopped streamlet and then erase it */
        vector<DavStreamletTag> stopped;
        auto streamlets = m_river.getStreamlets();
        for (const auto & s : streamlets)
            if (s->isStopped())
                stopped.push_back(s->getTag());
        for (const auto & e : stopped)
            m_river.erase(e);
    } while (!m_bExit);

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

int AppService::doLogSetting() {
    /* setup glog first */
    const auto & glogdir = m_appGlobalSetting.glog_save_path();
    LOG(INFO) << m_logtag << "Glog Config with log dir " << glogdir;
    const string mkdirCmd = "mkdir -p " + glogdir;
    system(mkdirCmd.c_str());
    const string & glogLevel = m_appGlobalSetting.glog_save_level();

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
    s_ffmpegLogLevel = toFFmpegLogSeverity(m_appGlobalSetting.ffmpeg_log_level());
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
// [http]
int AppService::requestToMessage(shared_ptr<Request> & request, google::protobuf::Message & pbmsg) {
    int ret = 0;
    asio::streambuf::const_buffers_type cbt = request->m_request.data();
    const string jsonstr(asio::buffers_begin(cbt), asio::buffers_end(cbt));
    ret = PbTree::pbFromJsonString(pbmsg, jsonstr);
    if (ret < 0) {
        ERRORIT(APP_ERROR_PARSE_REQUEST, "Fail load to pb object: " + PbTree::errToString(ret));
        return APP_ERROR_PARSE_REQUEST;
    }
    LOG(INFO) << m_logtag << request->m_path << ",  request parse done: " << jsonstr;
    return 0;
}

int AppService::failResponse(shared_ptr<Response> & response, const int errCode,
                             const string & errDetail, const bool bSync) {
    AppGlobalSetting::CommonResponse failCR;
    string failCRStr;
    failCR.set_code(errCode);
    failCR.set_msg(errDetail);
    failCR.set_b_sync_resp(bSync);
    PbTree::pbToJsonString(failCR, failCRStr);
    response->write(failCRStr);
    return 0;
}

/////////////////////
// [report helpers]
int AppService::appMsgJsonReport(const shared_ptr<AVDictionary> & msg) {
    usleep(50000);
    return 0;
}

int AppService::appMsgProtobufReport(const shared_ptr<AVDictionary> & msg) {
    return 0;
}

//

/////////////////////////////////////////
// [Default Streamlets build]
int AppService::asyncBuildInputStreamlet(const string & inputUrl,
                                         const DavStreamletSetting::InputStreamletSetting & inputSetting,
                                         std::function<int(shared_ptr<DavStreamlet>)> onBuildSuccess) {
    auto inSetting = make_shared<DavStreamletSetting::InputStreamletSetting>();
    inSetting->CopyFrom(inputSetting); /* in case pass down inputSetting out of scope */
    std::thread([this, onBuildSuccess, inSetting, inputUrl]() {
            auto fut = std::async(std::launch::async, [this, inSetting, inputUrl]() {
                    return this->buildInputStreamlet(inputUrl, *inSetting);
                });
            fut.wait();
            if (fut.get() >= 0) {
                auto inputStreamlet = m_river.get(DavDefaultInputStreamletTag(inputUrl));
                onBuildSuccess(inputStreamlet);
            }
            return;
        }).detach();
    return 0;
}

int AppService::buildInputStreamlet(const string & inputUrl,
                                    const DavStreamletSetting::InputStreamletSetting & inputSetting) {
    DavStreamletOption so;
    so.set(DavOptionBufLimitNum(), std::to_string(m_appGlobalSetting.input_max_buf_num()));
    vector<DavWaveOption> waveOptions;
    PbStreamletSettingToDavOption::mkInputStreamletWaveOptions(inputUrl, inputSetting, waveOptions);
    DavDefaultInputStreamletBuilder builder;
    auto streamletInput = builder.build(waveOptions, DavDefaultInputStreamletTag(inputUrl), so);
    if (!streamletInput) {
        /* work around, for we may in another thread */
        AppMessager msg(APP_ERROR_BUILD_STREAMLET, ("fail create input streamlet with id " + inputUrl + ", " +
                                                    toStringViaOss(builder.m_buildInfo)));
        s_appMsgCollector.addMsg(msg);
        LOG(ERROR) << m_logtag << msg;

        // ERRORIT(APP_ERROR_BUILD_STREAMLET, "input streamlet build fail with id: " + inputUrl);
        return APP_ERROR_BUILD_STREAMLET;
    }
    m_river.add(streamletInput);
    LOG(INFO) << m_logtag << m_river.dumpRiver();
    return 0;
}

    int AppService::buildMixStreamlet(const string & mixStreamletName,
                                      const DavStreamletSetting::MixStreamletSetting & mixSetting) {
    DavStreamletOption so;
    so.set(DavOptionBufLimitNum(), std::to_string(m_appGlobalSetting.mix_max_buf_num()));
    vector<DavWaveOption> waveOptions;
    int ret = PbStreamletSettingToDavOption::
        mkMixStreamletWaveOptions(mixStreamletName, mixSetting, waveOptions);
    if (ret < 0) {
        ERRORIT(APP_ERROR_BUILD_STREAMLET, "mix setting invalid");
        return APP_ERROR_BUILD_STREAMLET;
    }
    DavMixStreamletBuilder builder;
    auto streamletMix = builder.build(waveOptions, DavMixStreamletTag(mixStreamletName), so);
    if (!streamletMix) {
        ERRORIT(APP_ERROR_BUILD_STREAMLET, ("mix streamlet build fail with id: " + mixStreamletName + ", " +
                                            toStringViaOss(builder.m_buildInfo)));
        return APP_ERROR_BUILD_STREAMLET;
    }
    m_river.add(streamletMix);
    return 0;
}

int AppService::buildOutputStreamlet(const string & outputId,
                                     const DavStreamletSetting::OutputStreamletSetting & outStreamletSetting,
                                     const vector<string> & fullOutputUrls) {
    DavStreamletOption so;
    so.set(DavOptionBufLimitNum(), std::to_string(m_appGlobalSetting.output_max_buf_num()));
    vector<DavWaveOption> waveOptions;
    PbStreamletSettingToDavOption::mkOutputStreamletWaveOptions(fullOutputUrls,
                                                                outStreamletSetting, waveOptions);
    DavDefaultOutputStreamletBuilder builder;
    auto streamletOutput = builder.build(waveOptions, DavDefaultOutputStreamletTag(outputId), so);
    if (!streamletOutput) {
        ERRORIT(APP_ERROR_BUILD_STREAMLET, ("build output streamlet fail; with id " +
                                            outputId + ", " + toStringViaOss(builder.m_buildInfo)));
        return APP_ERROR_BUILD_STREAMLET;
    }
    m_river.add(streamletOutput);
    return 0;
}

} // namespace app_common
