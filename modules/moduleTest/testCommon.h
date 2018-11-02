#pragma once

#include <unistd.h>
#include <string>
#include <vector>
#include <memory>
#include <atomic>

#include <glog/logging.h>
#include "ffmpegHeaders.h"
#include "globalSignalHandle.h"
#include "davWave.h"
#include "davMessager.h"
#include "davStreamlet.h"

namespace test_common {
using ::std::string;
using namespace ff_dynamic;
using namespace global_sighandle;

extern std::atomic<bool> g_bExit;
extern int testInit(const string & logtag);

template<typename T>
int testRun(T & t) {
    LOG(INFO) << "-- Start river process";
    int ret = 0;
    auto & collector = DavMsgCollector::getInstance();
    do {
        if (t.isStopped()) {
            break;
        } else {
            ret = t.getErr();
            if (ret < 0 && ret != AVERROR_EOF && ret != AVERROR(EAGAIN)) {
                DavMessager err(ret, "find a process error, will stop");
                LOG(ERROR) << "process failed " << err;
                g_bExit = true;
                break;
            }
        }

        auto msg = collector.getMsg();
        if (0 && msg) { /* report collected msg to whereever needed */
            char *buf = nullptr;
            av_dict_get_string(msg.get(), &buf, ':', ',');
            LOG(INFO) << "[test common] " << buf;
            av_freep(&buf);
        }
        usleep(100000);
    } while (!g_bExit);

    return 0;
}

} // namespace test_common
