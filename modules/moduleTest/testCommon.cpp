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

    av_log_set_callback([] (void *ptr, int level, const char *fmt, va_list vl) {
        if (level > AV_LOG_WARNING)
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

} // namespace test_common
