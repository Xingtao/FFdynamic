#pragma once

#include <signal.h>
#include <map>
#include <mutex>
#include <utility>
#include <functional>
#include <algorithm>

namespace global_sighandle {
using ::std::map;
using ::std::function;

/* A easy to use global signal handle class aims for simple signal handle scenario. */
class GlobalSignalHandle {
public:
    static GlobalSignalHandle & getInstance() {
        static GlobalSignalHandle s_gsh;
        return s_gsh;
    }

    int registe(int s, sig_t f) {
        std::lock_guard<std::mutex> lock(m_lock);
        auto it = std::find_if(m_handlers.begin(), m_handlers.end(),
                               [s](const std::pair<int, sig_t> & e) {return e.first == s;});
        if (it != m_handlers.end())
            return -1; // already exist, remove first
        signal(s, f);
        m_handlers.insert(std::pair<int, sig_t>(s, f));
        return 0;
    }

    int remove(int s) {
        std::lock_guard<std::mutex> lock(m_lock);
        auto it = std::find_if(m_handlers.begin(), m_handlers.end(),
                               [s](const std::pair<int, sig_t> & e) {return e.first == s;});
        if (it == m_handlers.end())
            return -1; // not exist
        signal(s, SIG_DFL);
        m_handlers.erase(it);
        return 0;
    }

private:
    GlobalSignalHandle () = default;
    GlobalSignalHandle(const GlobalSignalHandle &) = delete;
    GlobalSignalHandle & operator=(const GlobalSignalHandle &) = delete;
    std::map<int, sig_t> m_handlers;
    std::mutex m_lock;
};

} // namespace signal_handle
