#pragma once

#include <memory>
#include <string>
#include <utility>
#include <functional>
#include <unordered_map>
#include <typeinfo>
#include <typeindex>

#include <glog/logging.h>
#include "davPeerEvent.h"

namespace ff_dynamic {
using ::std::string;
using ::std::unordered_map;
using ::std::function;
using ::std::shared_ptr;

/* Runtime Event Process: dynamic config implementation */
class DavImplEventProcess {
public:
    DavImplEventProcess () = default;

    template <typename T>
    int processExternal(T & event) {
        auto it = m_implEventMap.find(std::type_index(typeid(event)));
        if (it == m_implEventMap.end())
            return DAV_ERROR_EVENT_PROCESS_NOT_SUPPORT;
        auto & eventProcess = *(reinterpret_cast<function<int (const T &)> *>(it->second->m_fp));
        return eventProcess(event);
    }

    int processPeer(const DavPeerEvent & event) {
        const auto & e = event.getSelf();
        auto it = m_implEventMap.find(std::type_index(typeid(event)));
        if (it == m_implEventMap.end())
            return DAV_ERROR_EVENT_PROCESS_NOT_SUPPORT;
        auto & eventProcess = *(reinterpret_cast<function<int (decltype(e))> *>(it->second->m_fp));
        return eventProcess(event);
    }
    /* called in each implementation for interested events */
    int processTravelDynamic(const DavTravelDynamic & event) {
        const auto & e = event.getSelf();
        auto it = m_implEventMap.find(std::type_index(typeid(event)));
        if (it == m_implEventMap.end())
            return DAV_ERROR_EVENT_PROCESS_NOT_SUPPORT;
        auto & eventProcess = *(reinterpret_cast<function<int (decltype(e))> *>(it->second->m_fp));
        return eventProcess(event);
    }

    template <typename T>
    int registerEvent(function<int (const T &)> & f) {
        std::type_index typeIndex = std::type_index(typeid(T));
        auto g = new function<int (const T &)>(f);
        auto p = reinterpret_cast<void *>(g);
        EventDeleter<T> d(p);
        auto deleter = [d] (EventFunctionPointer *t) {
            using FP = decltype(d.m_fp);
            FP fp = reinterpret_cast<FP>(t->m_fp);
            delete fp;
        };
        shared_ptr<EventFunctionPointer> fp(new EventFunctionPointer(p), deleter);
        m_implEventMap.insert(std::make_pair(typeIndex, fp));
        return 0;
    }

    void deleteEvent(const std::type_index typeIndex) {
        auto it = m_implEventMap.find(typeIndex);
        if (it != m_implEventMap.end()) {
            m_implEventMap.erase(it);
        }
        return;
    }

private: /* two wrappers to make deleter easier */
    template<typename T>
    struct EventDeleter {
        EventDeleter(void *p) {
            m_fp = reinterpret_cast<function<int (const T &)> *>(p);
        }
        std::function<int (const T &)> *m_fp = nullptr;
    };

    struct EventFunctionPointer {
        EventFunctionPointer(void *p) {m_fp = p;}
        void *m_fp = nullptr;
    };

private:
    using DavImplEventProcessMap = unordered_map<std::type_index, shared_ptr<EventFunctionPointer>>;
    DavImplEventProcessMap m_implEventMap;
};

/* TODO: may make with variable Args later
//template<typename T>
//struct DavImplEventProcessTraits;
//
//template<typename Ret, typename ...Args>
//struct DavImplEventProcessTraits<std::function<Rer(Args...)>> {
//    using Ret = result_type;
//    const size_t m_nargs = sizeof...(Args);
//    template <size_t I>
//    struct arg {
//        using ArgTypes = typename std::tuple_element<I, std::tuple<Args...>>::type type;
//    };
//};
*/

} // namespace ff_dynamic
