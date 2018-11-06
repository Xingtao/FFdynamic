#include <unistd.h>
#include <string>
#include <atomic>
#include <exception>

#include <glog/logging.h>

#include "httpClient.h"
#include "globalSignalHandle.h"
#include "dynaDetectorService.h"

////////////////////////////////////////////////////////////////////////////////
using ::std::string;
using namespace dyna_detect_service;

////////////////////////////////////////////////////////////////////////////////
std::atomic<bool> g_bExit = ATOMIC_VAR_INIT(false);
std::atomic<int> g_interruptCount = ATOMIC_VAR_INIT(0);
static const string s_logtag = "[DetectorService] ";

int sendStopToDetectorService() {
    auto client = std::make_shared<http_util::HttpClient>("127.0.0.1", 8081);
    try { // Synchronous request
        client->request("POST", "/api1/stop");
    }
    catch(const std::exception & e) {
        LOG(WARNING) << s_logtag << "sendStopToDetectorService request exception: " << e.what();
    }
    return 0;
}

void sigIntHandle(int sig) {
    g_bExit = true;
    sendStopToDetectorService();
    LOG(INFO) << "sig " << sig << " captured. count " << g_interruptCount;
    if (++g_interruptCount >= 3) {
        LOG(INFO) << "force exit program";
        exit(1);
    }
}

////////////////////////////////////////////////////////////////////////////////
int main(int argc, char **argv) {
    google::InitGoogleLogging(s_logtag.c_str());
    if (argc != 2) {
        LOG(ERROR) << "Usage: detectorService configPath";
        return -1;
    }

    auto & sigHandle = global_sighandle::GlobalSignalHandle::getInstance();
    sigHandle.registe(SIGINT, sigIntHandle);

    string configPath(argv[1]);
    LOG(INFO) << s_logtag << "DetectorService with config file: " << configPath;

    /* start detector service */
    DynaDetectService & detectorService = DynaDetectService::getOnlyInstance();
    if (detectorService.init(configPath) < 0) {
        LOG(ERROR) << s_logtag << "Failed create DetectorService: " << detectorService.getErr();
        return -1;
    }
    detectorService.start();
    LOG(INFO) << s_logtag << "DetectorService Stopped. Exit program.";
    google::ShutdownGoogleLogging();
    return 0;
}
