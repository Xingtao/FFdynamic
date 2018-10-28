#include <unistd.h>
#include <string>
#include <atomic>
#include <exception>

#include <glog/logging.h>

#include "globalSignalHandle.h"
#include "httpClient.h"
#include "ialService.h"

////////////////////////////////////////////////////////////////////////////////
#ifndef COMPILE_TIME
#define COMPILE_TIME ("unknown")
#endif

#ifndef COMPILE_VERSION
#define COMPILE_VERSION ("unknown")
#endif

////////////////////////////////////////////////////////////////////////////////
using ::std::string;
using namespace ial_service;

////////////////////////////////////////////////////////////////////////////////
std::atomic<bool> g_bExit = ATOMIC_VAR_INIT(false);
std::atomic<int> g_interruptCount = ATOMIC_VAR_INIT(0);
static const string s_logtag = "[IalService] ";


////////////////////////////////////////////////////////////////////////////////
// static helpers
static void logVersion()
{
    LOG(INFO) << s_logtag << "NEW SESSION OF LOG";
    LOG(INFO) << s_logtag << "Compile Time " << COMPILE_TIME << ", Version " << COMPILE_VERSION;
    return;
}

int sendStopToIalService() {
    auto client = std::make_shared<http_util::HttpClient>("127.0.0.1", 8080);
    try { // Synchronous request
        client->request("POST", "/api1/ial/stop");
    }
    catch(const std::exception & e) {
        LOG(WARNING) << s_logtag << "sendStopToIalService request exception: " << e.what();
    }
    return 0;
}

void sigIntHandle(int sig) {
    g_bExit = true;
    sendStopToIalService();
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
        LOG(ERROR) << "Usage: ialService configPath";
        return -1;
    }
    auto & sigHandle = global_sighandle::GlobalSignalHandle::getInstance();
    sigHandle.registe(SIGINT, sigIntHandle);

    string configPath(argv[1]);
    LOG(INFO) << s_logtag << "IalService with config file: " << configPath;

    /* start ial service */
    IalService & ialService = IalService::getOnlyInstance();
    if (ialService.init(configPath) < 0) {
        LOG(ERROR) << s_logtag << "Failed create IalService: " << ialService.getErr();
        return -1;
    }

    logVersion();
    ialService.start();
    LOG(INFO) << s_logtag << "IalService Stopped. Exit program.";
    google::ShutdownGoogleLogging();
    return 0;
}
