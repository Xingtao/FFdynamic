#include "testCommon.h"

using namespace ff_dynamic;

namespace test_common {

std::atomic<bool> g_bExit = ATOMIC_VAR_INIT(false);
std::atomic<int> g_bInterruptCount = ATOMIC_VAR_INIT(0);

void sigIntHandle(int sig) {
    g_bExit = true;
    LOG(INFO) << "sig " << sig << " captured. count" << g_bInterruptCount;
    if (++g_bInterruptCount >= 3) {
        LOG(INFO) << "exit program";
        exit(1);
    }
}

int testInit(const string & logtag) {
    google::InitGoogleLogging(logtag.c_str());
    google::InstallFailureSignalHandler();
    FLAGS_stderrthreshold = 0;
    FLAGS_logtostderr = 1;
    auto & sigHandle = GlobalSignalHandle::getInstance();
    sigHandle.registe(SIGINT, sigIntHandle);
    return 0;
}

} // namespace test_common
