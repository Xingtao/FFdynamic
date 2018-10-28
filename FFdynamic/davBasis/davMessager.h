#pragma once

#include <iostream>
#include <string>
#include <queue>
#include <exception>
#include <memory>
#include <mutex>

#include "davUtil.h"
#include "ffmpegHeaders.h"

namespace ff_dynamic {
using ::std::string;
using ::std::queue;

//////////////////////////////////////////////////////////////////////////////////////////
/* FFmpeg use Posix Error Code along with its own error definitions via FFERRTAG macro.
   Here we define FFdynamic's error code (extend FFmpeg's error code, also use FFERRTAG)
   Please make sure no name collapsion with FFmpeg's ERRTAG.
*/

// FFdynamic dynamic initialize & close & process error
#define DAV_ERROR_IMPL_ON_CONSTRUCT       FFERRTAG('E', 'I', 'O', 'C')
#define DAV_ERROR_IMPL_DYNAMIC_INIT       FFERRTAG('E', 'I', 'D', 'I')
#define DAV_ERROR_IMPL_ON_DESTRUCT        FFERRTAG('E', 'E', 'O', 'D')
#define DAV_ERROR_IMPL_PROCESS            FFERRTAG('I', 'P', 'R', 'F')
#define DAV_ERROR_IMPL_POST_PROCESS       FFERRTAG('I', 'P', 'P', 'F')

// DavProc base error code
#define DAV_ERROR_BASE_EMPTY_IMPL           FFERRTAG('B', 'E', 'I', 'M')
#define DAV_ERROR_BASE_CREATE_PROC_THREAD   FFERRTAG('B', 'C', 'P', 'T')
#define DAV_ERROR_BASE_POST_PROCESS         FFERRTAG('B', 'P', 'P', 'F')
#define DAV_ERROR_BASE_EMPTY_OPTION         FFERRTAG('B', 'E', 'I', 'O')

// Impl factory error code
#define DAV_ERROR_FACTORY_NO_IMPL_TYPE       FFERRTAG('F', 'N', 'I', 'T')
#define DAV_ERROR_FACTORY_NO_CLASS_TYPE      FFERRTAG('F', 'N', 'C', 'T')
#define DAV_ERROR_FACTORY_INVALID_IMPL_TYPE  FFERRTAG('F', 'I', 'I', 'T')
#define DAV_ERROR_FACTORY_INVALID_CLASS_TYPE FFERRTAG('F', 'I', 'C', 'T')
#define DAV_ERROR_FACTORY_CREATE_IMPL        FFERRTAG('F', 'C', 'I', 'F')

// Dict error codes
#define DAV_ERROR_DICT_NO_SUCH_KEY           FFERRTAG('D', 'N', 'S', 'K')
#define DAV_ERROR_DICT_KEY_EXIST             FFERRTAG('D', 'N', 'S', 'K')
#define DAV_ERROR_DICT_VAL_INVALID           FFERRTAG('D', 'V', 'I', 'V')
#define DAV_ERROR_DICT_VAL_OUT_RANGE         FFERRTAG('D', 'V', 'O', 'R')
#define DAV_ERROR_DICT_VAL_UNKNOWN           FFERRTAG('D', 'V', 'U', 'K')
#define DAV_ERROR_DICT_TYPE_MISMATCH        FFERRTAG('D', 'V', 'T', 'M')
#define DAV_ERROR_DICT_MISS_INPUTURL         FFERRTAG('D', 'M', 'I', 'U')
#define DAV_ERROR_DICT_MISS_OUTPUTURL        FFERRTAG('D', 'M', 'O', 'U')
#define DAV_ERROR_DICT_MISS_CODECPAR         FFERRTAG('D', 'M', 'C', 'P')

// FFdynamic events process error
#define DAV_ERROR_EVENT_MESSAGE_INVALID     FFERRTAG('E', 'M', 'I', 'V')
#define DAV_ERROR_EVENT_PROCESS_NOT_SUPPORT FFERRTAG('E', 'P', 'N', 'S')
#define DAV_ERROR_EVENT_LAYOUT_UPDATE       FFERRTAG('E', 'P', 'L', 'U')
#define DAV_ERROR_EVENT_MUTE_UNMUTE         FFERRTAG('E', 'M', 'A', 'U')
#define DAV_ERROR_EVENT_BACKGROUD_UPDATE    FFERRTAG('E', 'B', 'G', 'U')

// detailed impl errors
#define DAV_ERROR_IMPL_CODEC_NOT_FOUND           FFERRTAG('I', 'C', 'N', 'F')
#define DAV_ERROR_IMPL_DATA_NOPTS                FFERRTAG('E', 'I', 'D', 'N')
#define DAV_ERROR_IMPL_UNEXPECT_EMPTY_INBUF      FFERRTAG('I', 'U', 'E', 'I')
#define DAV_ERROR_TRAVEL_STATIC_INVALID_CODECPAR FFERRTAG('T', 'S', 'I', 'C')
#define DAV_ERROR_TRAVEL_STATIC_INVALID_VIDEOPAR FFERRTAG('T', 'S', 'I', 'V')
#define DAV_ERROR_TRAVEL_STATIC_INVALID_AUDIOPAR FFERRTAG('T', 'S', 'I', 'A')
#define DAV_ERROR_IMPL_CLEAR_CACHE_BUFFER        FFERRTAG('I', 'C', 'C', 'B')
#define DAV_ERROR_IMPL_DTS_NOT_MONOTONIC         FFERRTAG('I', 'D', 'N', 'M')
#define DAV_ERROR_IMPL_PKT_NO_DTS                FFERRTAG('I', 'P', 'N', 'D')
#define DAV_ERROR_IMPL_CREATE_AUDIO_RESAMPLE     FFERRTAG('I', 'C', 'A', 'R')


//////////////////////////////////////////////////////////////////////////////////////////
// Report Code Definitions (all 'report code' > 0，so the forth tag must > 128)
// Use 0xF0 for INFOTAG, 0xF1 for WARNINGTAG
#define DAV_INFOTAG_SUFFIX (0xF1)
#define DAV_WARNINGTAG_SUFFIX (0xF2)
#define FFINFOTAG(a, b, c) (FFERRTAG(a, b, c, DAV_INFOTAG_SUFFIX))
#define FFWARNINGTAG(a, b, c) (FFERRTAG(a, b, c, DAV_WARNINGTAG_SUFFIX))

// Info Report
#define DAV_INFO_BASE_CONSTRUCT_DONE        FFINFOTAG('B', 'C', 'D')
#define DAV_INFO_BASE_DESTRUCT_DONE         FFINFOTAG('B', 'D', 'D')
#define DAV_INFO_BASE_DELETE_ONE_RECEIVER   FFINFOTAG('B', 'D', 'R')
#define DAV_INFO_BASE_END_PROCESS           FFINFOTAG('B', 'E', 'P')
#define DAV_INFO_RUN_PROCESS_THREAD         FFINFOTAG('R', 'P', 'T')
#define DAV_INFO_END_PROCESS_THREAD         FFINFOTAG('E', 'P', 'T')
#define DAV_INFO_IMPL_INSTANCE_CREATE_DONE  FFINFOTAG('I', 'I', 'C')
#define DAV_INFO_IMPL_INSTANCE_DESTROY_DONE FFINFOTAG('I', 'I', 'D')
#define DAV_INFO_DYNAMIC_CONNECT            FFINFOTAG('D', 'C', 'N')
#define DAV_INFO_DYNAMIC_DISCONNECT         FFINFOTAG('D', 'D', 'C')
#define DAV_INFO_DYNAMIC_SUBSCRIBE          FFINFOTAG('D', 'S', 'C')
#define DAV_INFO_DYNAMIC_UNSUBSCRIBE        FFINFOTAG('D', 'U', 'S')
#define DAV_INFO_ADD_ONE_MIX_STREAM         FFINFOTAG('A', 'M', 'S')
#define DAV_INFO_ONE_FINISHED_MIX_STREAM    FFINFOTAG('O', 'F', 'M')

// warning report
#define DAV_WARNING_IMPL_DROP_DATA          FFWARNINGTAG('I', 'D', 'D')
#define DAV_WARNING_IMPL_CACHE_TOO_MANY     FFWARNINGTAG('I', 'C', 'M')
#define DAV_WARNING_IMPL_UNUSED_OPTS        FFWARNINGTAG('I', 'U', 'O')

//////////////////////////////////////////////////////////////////////////////////////////
// helper functions
extern string davMsg2str(const int reportNum);

//////////////////////////////////////////////////////////////////////////////////////////
struct DavMessager : public std::exception {
    DavMessager() = default;
    DavMessager(const int msgCode, const string & msgDetail = "") noexcept {
        setInfo(msgCode, msgDetail);
    }
    inline virtual string msg2str(const int reportNum) {
        return davMsg2str(reportNum);
    }
    inline void setInfo(const int msgCode, const string & msgDetail = "") noexcept {
        m_msgCode = msgCode;
        m_msgDetail = msg2str(msgCode) + ", " + msgDetail;
    }
    inline bool hasErr() const noexcept {return m_msgCode < 0;}
    friend std::ostream & operator<<(std::ostream & os, const DavMessager & msg);
    int m_msgCode = 0;
    string m_msgDetail;
};

extern std::ostream & operator<<(std::ostream & os, const DavMessager & msg);
using DavMsgError = DavMessager; /* Explictly use DavMessagerError for 'msgCode' < 0 */

//////////////////////////////////////////////////////////////////////////////////////////
/* Class for DavMessager% collection (error and important info, warning) */
class DavMsgCollector {
public:
    static DavMsgCollector & getInstance() {
        static DavMsgCollector s_collector;
        return s_collector;
    }
    int addMsg(const DavMessager & msg);
    std::shared_ptr<AVDictionary> getMsg();
    void setMaxMsgNum(const size_t num) {m_maxMsgNum = num;};
    bool isEmpty() {std::lock_guard<std::mutex> lock(m_lock); return m_msgQueue.empty();}

private:
    inline const char *getMsgType(const int msgCode) {
        return (msgCode < 0 ? "error" :
                ((msgCode & DAV_INFOTAG_SUFFIX) == DAV_INFOTAG_SUFFIX ? "info" :
                 ((msgCode & DAV_WARNINGTAG_SUFFIX) == DAV_WARNINGTAG_SUFFIX ? "warning" : "unknown")));
    }

private:
    DavMsgCollector() = default;
    DavMsgCollector(const DavMsgCollector &) = delete;
    DavMsgCollector & operator=(const DavMsgCollector &) = delete;
    queue<std::shared_ptr<AVDictionary>> m_msgQueue;
    size_t m_maxMsgNum = 1000;
    size_t m_msgCount = 0;
    std::mutex m_lock;
};

} // namespace
