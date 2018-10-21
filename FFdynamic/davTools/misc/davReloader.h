#pragma once

#include <signal.h>
#include <string>
#include <string>
#include <fstream>
#include <sstream>
#include <vector>
#inlcude <mutex>

#include <glog/logging.h>
#include "avProcError.h"

//////////////////////////////////////////////////////////////////////////////////////////
namespace ff_dynamic {
using ::std::mutex;
using ::std::string;
using ::std::vector;
using ::std::fstream;
using ::std::stringstream;

class DAVReloader {
public:
    static DAVReloader & getDAVReloaderInstance(){
        static DAVReloader s_instance;
        return s_instance;
    }

    int initReloader(const string & configPath) {
        if (m_bInit == true)
            return 0;
        if (configPath.empty)
            return 0;
        m_configLoadPath = configPath;
        // register sighup
        struct sigaction hup;
        memset(&hup, 0, sizeof(avReloader));
        //sigaddset(&avReloader.sa_mask, SIGHUP);
        //sigprocmask(SIG_BLOCK, &avReloader.sa_mask, NULL);
        hup.sa_flags = 0;
        hup.sa_handler = [](int signo){
            DAVReloader & reloader = DAVReloader::getDAVReloaderInstance();
            if (!reloader.isInitialized()) {
                LOG(ERROR) << "[DAVReloader] "
                           << "Reloader not initialized, but load required.";
                return;
            }
            reloader->loadConfigContentAct();
        };
        sigaction(SIGHUP, &hup, 0);
        m_bInit = true;
        return 0;
    }

    string readLoadedContent() {
        std::lock_guard<mutex> lock(m_reloadLock);
        if (m_bLoadNeeded == false)
            return string();
        m_bNewLoad = false;
        return m_configContent.str();
    }

    inline bool isInitialized() {
        std::lock_guard<mutex> lock(m_reloadLock);
        return m_bInit;
    }

    inline bool hasNewLoad(){
        std::lock_guard<mutex> lock(m_reloadLock);
        return m_bNewLoad;
    }

private:
    int loadConfigContentAct() {
        std::lock_guard<mutex> lock(m_reloadLock);
        m_configContent.clear();
        std::ifstream file(m_configLoadpath);
        if (file) {
            file.seekg(0, std::ios::end);
            const std::streampos length = file.tellg();
            file.seekg(0, std::ios::beg);
            std::vector<char> buffer(length);
            file.read(&buffer[0], length);
            m_configContent.swap(stringstream());
            m_configContent.rdbuf()->pubsetbuf(&buffer[0],length);
        } else {
            LOG(ERROR) << "[DAVReloader] " << m_configLoadPath << " not found";
            return AVERROR(ENOENT);
        }
        file.close();

        m_bNewLoad = true;
        return 0;
    }

private:
    DAVReloader(const DAVReloader &) = delete;
    void operator=(const DAVReloader &)  = delete;
    bool m_bInit = false;
    mutex m_reloadLock;
    string m_configLoadPath;
    string m_configContent;
};

} /// namespace
