#include <time.h>
#include <map>
#include <algorithm>

#include "glog/logging.h"
#include "davMessager.h"

namespace ff_dynamic {

static const std::map<int, const char *> davMessagerCode2StrMap = {
    {0,                                "Success"},
    // if cannot knwo init implementation errors, fill those ones
    {DAV_ERROR_IMPL_ON_CONSTRUCT,   "Imp - failed on impl construct"},
    {DAV_ERROR_IMPL_DYNAMIC_INIT,   "Imp - failed on dynamic initialization"},
    {DAV_ERROR_IMPL_ON_DESTRUCT,    "Imp - failed on impl destruct"},
    {DAV_ERROR_IMPL_PROCESS,        "Impl - do process fail"},
    {DAV_ERROR_IMPL_POST_PROCESS,   "Impl - do post-process fail"},

    // base proc errors
    {DAV_ERROR_BASE_EMPTY_IMPL,         "Dav base has empty implementation"},
    {DAV_ERROR_BASE_CREATE_PROC_THREAD, "Dav base failed creating process thread"},
    {DAV_ERROR_BASE_POST_PROCESS,       "Dav base do post-process fail"},
    {DAV_ERROR_BASE_EMPTY_OPTION,       "Dav base required options not present"},

    // Implementation factory errors
    {DAV_ERROR_FACTORY_NO_IMPL_TYPE,       "Impl factory - no impl type found in options"},
    {DAV_ERROR_FACTORY_NO_CLASS_TYPE,      "Impl factory - no class type found in options"},
    {DAV_ERROR_FACTORY_INVALID_IMPL_TYPE,  "Impl factory - invalid impl type found in options"},
    {DAV_ERROR_FACTORY_INVALID_CLASS_TYPE, "Impl factory - invalid class type found in options"},
    {DAV_ERROR_FACTORY_CREATE_IMPL,        "Impl factory - create implementation failed"},

    // Dict error codes
    {DAV_ERROR_DICT_NO_SUCH_KEY,           "Dict - no such key"},
    {DAV_ERROR_DICT_KEY_EXIST,             "Dict - key exists"},
    {DAV_ERROR_DICT_VAL_INVALID,           "Dict - value invalid"},
    {DAV_ERROR_DICT_VAL_OUT_RANGE,         "Dict - value out of range"},
    {DAV_ERROR_DICT_TYPE_MISMATCH,         "Dict - type mismatch"},
    {DAV_ERROR_DICT_VAL_UNKNOWN,           "Dict - value unknown"},

    {DAV_ERROR_DICT_MISS_INPUTURL,         "Dict - missing input url"},
    {DAV_ERROR_DICT_MISS_OUTPUTURL,        "Dict - missing output url"},
    {DAV_ERROR_DICT_MISS_CODECPAR,         "Dict - missing codecpar"},

    // FFdynamic event message process error
    {DAV_ERROR_EVENT_MESSAGE_INVALID,      "Event - message invalid"},
    {DAV_ERROR_EVENT_PROCESS_NOT_SUPPORT,  "Event - not support for processing"},
    {DAV_ERROR_EVENT_LAYOUT_UPDATE,        "Event - video mix layout update fail"},
    {DAV_ERROR_EVENT_MUTE_UNMUTE,          "Event - audio mute/unmute fail"},
    {DAV_ERROR_EVENT_BACKGROUD_UPDATE,     "Event - video mix set backgroud picture fail"},

    // implementation specific errors
    {DAV_ERROR_IMPL_CODEC_NOT_FOUND,           "Impl - codec not found"},
    {DAV_ERROR_IMPL_DATA_NOPTS,                "Impl - data without valid timestamp"},
    {DAV_ERROR_IMPL_UNEXPECT_EMPTY_INBUF,      "Impl - unexpected empty input buf"},
    {DAV_ERROR_TRAVEL_STATIC_INVALID_CODECPAR, "Impl - TravelStatic - invalid codecpar"},
    {DAV_ERROR_TRAVEL_STATIC_INVALID_VIDEOPAR, "Impl - TravelStatic - invalid videopar"},
    {DAV_ERROR_TRAVEL_STATIC_INVALID_AUDIOPAR, "Impl - TravelStatic - invalid audiopar"},
    {DAV_ERROR_IMPL_CLEAR_CACHE_BUFFER,        "Impl - clear input cache buffer"},
    {DAV_ERROR_IMPL_DTS_NOT_MONOTONIC,         "Impl - pkt dts not monotonic"},
    {DAV_ERROR_IMPL_PKT_NO_DTS,                "Impl - pkt has no valid dts"},
    {DAV_ERROR_IMPL_CREATE_AUDIO_RESAMPLE,     "Impl - fail creating audio resample"},

    ////////////////////////////////////////////////////////////////////////////
    // info entries
    {DAV_INFO_BASE_CONSTRUCT_DONE,       "DavProc - construct done"},
    {DAV_INFO_BASE_DESTRUCT_DONE,        "DavProc - destruct done"},
    {DAV_INFO_RUN_PROCESS_THREAD,        "DavProc - run process thread"},
    {DAV_INFO_END_PROCESS_THREAD,        "DavProc - end process thread"},
    {DAV_INFO_BASE_DELETE_ONE_RECEIVER,  "DavProc - delete one of its input (receiver)"},
    {DAV_INFO_BASE_END_PROCESS,          "DavProc - end process after self flush done"},

    {DAV_INFO_IMPL_INSTANCE_CREATE_DONE, "DavImpl instance create done"},
    {DAV_INFO_BASE_DESTRUCT_DONE,        "DavImpl instance destruct done"},
    {DAV_INFO_DYNAMIC_CONNECT,           "DavWave connect"},
    {DAV_INFO_DYNAMIC_DISCONNECT,        "DavWave dis-connect"},
    {DAV_INFO_DYNAMIC_SUBSCRIBE,         "DavWave subscribe peer event"},
    {DAV_INFO_DYNAMIC_UNSUBSCRIBE,       "DavWave un-subscribe peer event"},
    {DAV_INFO_ADD_ONE_MIX_STREAM,        "DavImpl - Add one stream to mixer done"},
    {DAV_INFO_ONE_FINISHED_MIX_STREAM,   "DavImpl - One stream end when mix streams"},
    // warning entries
    {DAV_WARNING_IMPL_DROP_DATA,         "Impl - drop data"},
    {DAV_WARNING_IMPL_CACHE_TOO_MANY,    "Impl - cache too many data"},
    {DAV_WARNING_IMPL_UNUSED_OPTS,       "Impl - unused options"}
};

string davMsg2str(const int msgNum) {
    auto it = std::find_if(davMessagerCode2StrMap.begin(), davMessagerCode2StrMap.end(),
                           [msgNum](const std::pair<int, const char *> & item) {
                               return item.first == msgNum;});
    if (it != davMessagerCode2StrMap.end())
        return string(it->second);
    /* all > 0 codes are defind by us and should be found. */
    if (msgNum > 0)
        return string("unknown");
    /* else, ffmpeg defined error code */
    char buf[AV_ERROR_MAX_STRING_SIZE] = {0};
    av_strerror(msgNum, buf, AV_ERROR_MAX_STRING_SIZE);
    return string(buf);
}

std::ostream & operator<<(std::ostream & os, const DavMessager & msg) { // output as json
    os << "{code:" << msg.m_msgCode << ", detail: " << msg.m_msgDetail << "}";
    return os;
}

////
int DavMsgCollector::addMsg(const DavMessager & e){
    if (m_msgQueue.size() > m_maxMsgNum) {
        std::lock_guard<std::mutex> lock(m_lock);
        auto & msg = m_msgQueue.front();
        LOG(WARNING) << "[MsgCollector] " << "Discard un-processed msg: " << msg;
        m_msgQueue.pop();
    }
    time_t curTime = time((time_t *)nullptr);
    static const char * const msgCode = "msgCode";
    static const char * const msgType = "msgType";
    static const char * const msgTime = "time";
    static const char * const detail = "detail";
    static const char * const msgCount = "msgCount";
    const char *msgTypeValue = getMsgType(e.m_msgCode);
    AVDictionary *dict = nullptr;
    av_dict_set(&dict, msgCode, std::to_string(e.m_msgCode).c_str(), 0);
    av_dict_set(&dict, msgType, msgTypeValue, 0);
    av_dict_set(&dict, detail, e.m_msgDetail.c_str(), 0);
    av_dict_set_int(&dict, msgTime, (int64_t)curTime, 0);
    std::lock_guard<std::mutex> lock(m_lock);
    av_dict_set_int(&dict, msgCount, ++m_msgCount, 0);
    if (m_msgQueue.size() >= m_maxMsgNum)
        m_msgQueue.pop();
    m_msgQueue.push(std::shared_ptr<AVDictionary>(dict, [](AVDictionary *d){av_dict_free(&d);}));
    return 0;
}

std::shared_ptr<AVDictionary> DavMsgCollector::getMsg(){
    std::lock_guard<std::mutex> lock(m_lock);
    if (m_msgQueue.size() == 0)
        return {};
    auto e = m_msgQueue.front();
    m_msgQueue.pop();
    return e;
}

} // namespace
