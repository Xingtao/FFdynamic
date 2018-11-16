#pragma once
// system
#include <map>
#include <vector>
#include <memory>
#include <utility>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <condition_variable>

namespace ff_dynamic {
using ::std::map;
using ::std::pair;
using ::std::vector;
using ::std::shared_ptr;

/////////////////////////////////////////////////////////////////////////////////////////
enum class EDavExpect : int {
    eDavExpectNothing, /* can continue without input */
    eDavExpectAnyOne,  /* continue with any input */
    eDavExpectSpecificOne, /* only continue with specific one */
    eDavExpectExcludeList  /* continue with input not included in the list */
};

template<typename Address>
struct DavExpect {
    vector<EDavExpect> m_expectOrder = {EDavExpect::eDavExpectNothing};
    Address m_specificOne;
    vector<Address> m_excludeList;
};

template<typename Load, typename Address>
class DavTransmitor : public std::enable_shared_from_this<DavTransmitor<Load, Address>> {
public:
    using Transmitor = DavTransmitor<Load, Address>;
    explicit DavTransmitor(const string & logtag) : m_logtag(logtag) {}
    DavTransmitor() = default;
    virtual ~DavTransmitor() = default;

public: /* APIs */
    int addSender(const Address & senderAddr, const shared_ptr<Transmitor> & from) {
        std::lock_guard<std::mutex> guard(m_mutex);
        if (std::find(m_senderAddrs.begin(), m_senderAddrs.end(), senderAddr) != m_senderAddrs.end())
            return 0;
        m_senderAddrs.push_back(senderAddr);
        m_senderTransmitor.emplace(senderAddr, from);
        if (m_receiveCounts.count(senderAddr) == 0) /* stat input count number on this sender */
            m_receiveCounts.emplace(senderAddr, 0);
        return 0;
    }
    int addRecipient(const Address & addr, const shared_ptr<Transmitor> & r) {
        std::lock_guard<std::mutex> guard(m_mutex);
        bool bExist = false;
        for (auto & t : m_recipients) { /* from us to recipients */
            if (t.first == addr && t.second.get() == r.get()) {
                bExist = true;
                break;
            }
        }
        if (!bExist) {
            m_recipients.emplace(addr, r);
            m_sendCounts.emplace(r->getSelfAddress(), 0);
        }
        return 0;
    }
    int deleteSender(const Address & senderAddr) {
        std::lock_guard<std::mutex> guard(m_mutex);
        m_senderAddrs.erase(std::remove(m_senderAddrs.begin(), m_senderAddrs.end(), senderAddr),
                            m_senderAddrs.end());
        if (m_senderTransmitor.count(senderAddr) > 0)
            m_senderTransmitor.erase(senderAddr);

        /* also delete sender's load */
        m_loads.erase(std::remove_if(m_loads.begin(), m_loads.end(),
                                     [&senderAddr](const shared_ptr<Load> & load){
                                         return load->getAddress() == senderAddr;}),
                      m_loads.end());
        return 0;
    }

    int deleteRecipient(const shared_ptr<Transmitor> & r) {
        std::lock_guard<std::mutex> guard(m_mutex);
        auto it = m_recipients.begin();
        for (; it != m_recipients.end(); it++)
            if (it->second.get() == r.get())
                break;
        if (it != m_recipients.end())
            m_recipients.erase(it);
        return 0;
    }
    void deleteSenders() {
        std::lock_guard<std::mutex> guard(m_mutex);
        m_senderAddrs.clear();
        m_senderTransmitor.clear();
    }
    void deleteRecipients() {
        std::lock_guard<std::mutex> guard(m_mutex);
        m_recipients.clear();
    }
    void clear() {
        /* won't get upstream peers' output anymore */
        for (auto & t : m_senderTransmitor) {
            auto selfTransmitor(this->shared_from_this());
            LOG(INFO) << m_logtag << "--> up peer " << t.second->getLogtag() << " with ref " << t.second.use_count()
                      << " gonna delete " << m_logtag << " with ref count " << selfTransmitor.use_count();
            t.second->deleteRecipient(selfTransmitor);
        }
        /* lock here to avoid dead lock: in case upper peer do delivery (which will call this->welcome) */
        std::lock_guard<std::mutex> guard(m_mutex);
        m_loads.clear();
        m_senderAddrs.clear();
        m_senderTransmitor.clear();
        m_recipients.clear();
    }

public: /* trivial ones */
    inline const vector<Address> & getSenders() const noexcept {return m_senderAddrs;}
    inline bool isSenderStillValid(const Address & s) {
        std::lock_guard<std::mutex> lock(m_mutex);
        return std::find(m_senderAddrs.begin(), m_senderAddrs.end(), s) != m_senderAddrs.end();
    }
    inline size_t curLoadNum() const noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_loads.size();
    }
    inline void setSelfAddress(const Address & selfAddress) {m_selfAddress = selfAddress;}
    inline Address getSelfAddress() {return m_selfAddress;}
    inline const std::multimap<Address, shared_ptr<Transmitor>> & getRecipients() const noexcept {return m_recipients;}
    inline const map<Address, uint64_t> & getReceiveCounts() const noexcept {return m_receiveCounts;}
    inline const map<Address, uint64_t> & getSendCounts() const noexcept {return m_sendCounts;}
    inline string getSendCountStat() {return getCountStat(m_sendCounts);}
    inline string getReceiveCountStat() {return getCountStat(m_receiveCounts);}
    inline const string & getLogtag() const noexcept {return m_logtag;}

public: /* Transmitor load and unload its load */
    int welcome(const shared_ptr<Load> & in) {
        std::lock_guard<std::mutex> guard(m_mutex);
        m_loads.push_back(in);
        const auto & from = in->getAddress();
        m_receiveCounts.at(from) += 1;
        m_expectCV.notify_one();
        return 0;
    }
    int delivery(const shared_ptr<Load> & out) {
        std::lock_guard<std::mutex> guard(m_mutex);
        const Address & addr = out->getAddress();
        if (m_recipients.count(addr) > 0)
            for (auto & m : m_recipients)
                if (m.first == addr) {
                    m.second->welcome(out);
                    m_sendCounts.at(m.second->getSelfAddress()) += 1;
                }
        return 0;
    }
    int broadcast(const shared_ptr<Load> & out) {
        std::lock_guard<std::mutex> guard(m_mutex);
        for (auto & m : m_recipients)
            m.second->welcome(out);
        return 0;
    }
    int farwell(const shared_ptr<Load> & in) {
        std::lock_guard<std::mutex> guard(m_mutex);
        m_loads.erase(std::remove_if(m_loads.begin(), m_loads.end(),
                                     [&in](const shared_ptr<Load> & b) {
                                         return b.get() == in.get();}),
                      m_loads.end());
        return 0;
    }

public: /* expecting an load */
    shared_ptr<Load> retrive() {
        shared_ptr<Load> load;
        DavExpect<Address> expectNothing;
        expect(load, expectNothing, 0);
        if (load)
            farwell(load);
        return load;
    }
    bool expect(shared_ptr<Load> & expectLoad, const DavExpect<Address> & expectIn, int microSecond) {
        std::unique_lock<std::mutex> uniqueGuard(m_mutex);
        int stisfyIndex = -1;
        std::chrono::microseconds ms(microSecond);
        bool bExpect = m_expectCV.wait_for(uniqueGuard, ms,
                                           [this, &expectIn, &stisfyIndex]() {
                                               return this->isExpected(expectIn, stisfyIndex);
                                           });
        if (bExpect && stisfyIndex >= 0)
            expectLoad = m_loads[stisfyIndex];
        return bExpect;
    }

    bool isExpected(const DavExpect<Address> & expectIn, int & stisfyIndex) {
        bool bSatisified = false;
        for (const auto & expectOrder : expectIn.m_expectOrder) {
            switch (expectOrder){
            case EDavExpect::eDavExpectNothing:
                if (m_loads.size() > 0)
                    stisfyIndex = 0;
                bSatisified = true;
                break;
            case EDavExpect::eDavExpectAnyOne:
                bSatisified = m_loads.size() > 0;
                stisfyIndex = 0;
                break;
            case EDavExpect::eDavExpectSpecificOne:
                for (int k=0; k < (int)m_loads.size(); k++) {
                    if (m_loads[k]->getAddress() == expectIn.m_specificOne) {
                        bSatisified = true;
                        break;
                    }
                }
                break;
            case EDavExpect::eDavExpectExcludeList:
                bSatisified = processExpectExcludeList(expectIn.m_excludeList, stisfyIndex);
                break;
            default:
                break;
            }
        }
        return bSatisified;
    }

    bool processExpectExcludeList(const vector<Address> & excludeList, int & stisfyIndex) {
        for (int k=0; k < (int)m_loads.size(); k++) {
            const auto & load = m_loads[k];
            auto it = std::find_if(excludeList.begin(), excludeList.end(),
                                   [&load] (const Address & addr) {
                                       return addr == load->getAddress();
                              });
            if (it == excludeList.end()) {
                stisfyIndex = k;
                return true;
            }
        }
        return false;
    }

private:
    string getCountStat(const map<Address, uint64_t> & counts) {
        std::ostringstream oss;
        oss << "{";
        for (auto & m : counts)
            oss << "<" << m.first << " = " << m.second << "> ";
        oss << "}";
        return oss.str();
    }

private:
    string m_logtag;
    Address m_selfAddress;
    std::mutex m_mutex;
    std::condition_variable m_expectCV;
    vector<Address> m_senderAddrs;
    std::multimap<Address, shared_ptr<Transmitor>> m_senderTransmitor;
    map<Address, uint64_t> m_receiveCounts;
    std::multimap<Address, shared_ptr<Transmitor>> m_recipients;
    map<Address, uint64_t> m_sendCounts;
    vector<shared_ptr<Load>> m_loads;
};

/* Note:
   Load and Address has tight relation, buf if we use
       'template<template<typename> typename Load, typename Address>>',
   it will add extra complexity, and not easy to use.
   So, we left the burdon to the user, please make sure 'Load' has a 'Address & getAddress()' method.

   This class can also be used as a standalone helper when you porject needs simple publish-subscribe
   or TODO...
*/

} // Namespace
