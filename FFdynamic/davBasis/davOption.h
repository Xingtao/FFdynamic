#pragma once

#include <typeinfo>
#include <typeindex>

namespace ff_dynamic {
using ::std::type_index;

/* Be aware object slicing; make sure derived class contain nothing */
struct DavOption {
    DavOption(const type_index & key, const type_index & valueType, const string & name)
        : m_key(key), m_valueType(valueType), m_name(name) {
    }
    inline const string & name() const noexcept {return m_name;}
    inline const type_index valueType() const noexcept {return m_valueType;}
    /* two identical ones, used for different scenario */
    inline const type_index key() const noexcept {return m_key;}
    inline const type_index category() const noexcept {return m_key;}
    inline bool uniqueEqual(const DavOption & o) const {
        return ((m_key == o.key()) && (m_valueType == o.valueType()) && (m_name == o.name()));
    }
private:
    type_index m_key;
    type_index m_valueType;
    string m_name;
};

inline bool operator==(const DavOption & l, const DavOption & r) {
    return ((l.key() == r.key()) && (l.valueType() == l.valueType()));
}

inline bool operator<(const DavOption & l, const DavOption & r) {
    if (l.key() < r.key())
        return true;
    if (l.key() > r.key())
        return false;
    if (l.valueType() < r.valueType())
        return true;
    if (l.valueType() > r.valueType())
        return false;
    if (l.name().compare(r.name()) < 0)
        return true;
    if (l.name().compare(r.name()) > 0)
        return false;
    return false;
}

/* derive new DavOption class  */
struct DavOptionClassCategory : public DavOption {
    DavOptionClassCategory() :
        DavOption(type_index(typeid(*this)), type_index(typeid(DavOption)), "ClassCategory") {}
};

struct DavOptionImplType : public DavOption {
    DavOptionImplType() : DavOption(type_index(typeid(*this)), type_index(typeid(std::string)), "ImplType") {}
};

struct DavOptionCodecName : public DavOption {
    DavOptionCodecName() : DavOption(type_index(typeid(*this)), type_index(typeid(std::string)), "CodecName") {}
};

struct DavOptionLogtag : public DavOption {
    DavOptionLogtag() : DavOption(type_index(typeid(*this)), type_index(typeid(std::string)), "Logtag") {}
};

struct DavOptionInputUrl : public DavOption {
    DavOptionInputUrl() : DavOption(type_index(typeid(*this)), type_index(typeid(std::string)), "InputUrl") {}
};

struct DavOptionInputFpsEmulate : public DavOption {
    DavOptionInputFpsEmulate() :
        DavOption(type_index(typeid(*this)), type_index(typeid(bool)), "InputFpsEmulate") {}
};

struct DavOptionReconnectRetries : public DavOption {
    DavOptionReconnectRetries() :
        DavOption(type_index(typeid(*this)), type_index(typeid(int)), "ReconnectRetries") {}
};

struct DavOptionRWTimeout : public DavOption {
    DavOptionRWTimeout() : DavOption(type_index(typeid(*this)), type_index(typeid(int)), "RWTimeout") {}
};

struct DavOptionFilterDesc : public DavOption {
    DavOptionFilterDesc() : DavOption(type_index(typeid(*this)), type_index(typeid(std::string)), "FilterDesc") {}
};

struct DavOptionVideoMixLayout : public DavOption {
    DavOptionVideoMixLayout() :
        DavOption(type_index(typeid(*this)), type_index(typeid(std::string)), "VideoMixLayout") {}
};

struct DavOptionVideoMixRegeneratePts : public DavOption {
    DavOptionVideoMixRegeneratePts() :
        DavOption(type_index(typeid(*this)), type_index(typeid(bool)), "VideoMixRegeneratePts") {}
};

/* whether start mixing after all current participants joined, useful for static fixed number of inputs.
   set it to false if there are dynamically join/left.
 */
struct DavOptionVideoMixStartAfterAllJoin : public DavOption {
    DavOptionVideoMixStartAfterAllJoin() :
        DavOption(type_index(typeid(*this)), type_index(typeid(bool)), "StartAfterAllJoin") {}
};

struct DavOptionVideoMixQuitIfNoInputs : public DavOption {
    DavOptionVideoMixQuitIfNoInputs() :
        DavOption(type_index(typeid(*this)), type_index(typeid(bool)), "QuitIfNoInputs") {}
};

struct DavOptionOutputUrl : public DavOption {
    DavOptionOutputUrl() : DavOption(type_index(typeid(*this)), type_index(typeid(std::string)), "OutputUrl") {}
};

struct DavOptionContainerFmt : public DavOption {
    DavOptionContainerFmt() : DavOption(type_index(typeid(*this)), type_index(typeid(std::string)), "ContainerFmt") {}
};

////////////////////////////////////////////////////////
//// DavClassOption: Use DavOption Derived class as enum
using DavWaveClassCategory = DavOption;

/* derive new class category */
struct DavWaveClassNotACategory : public DavWaveClassCategory {
    DavWaveClassNotACategory () :
        DavWaveClassCategory(type_index(typeid(*this)), type_index(typeid(std::string)), "NotAClass") {}
};
struct DavWaveClassDataRelay : public DavWaveClassCategory {
    DavWaveClassDataRelay(const string & nameTag = "DataRelay") :
        DavWaveClassCategory(type_index(typeid(*this)), type_index(typeid(std::string)), nameTag) {}
};
struct DavWaveClassDemux : public DavWaveClassCategory {
    DavWaveClassDemux (const string & nameTag = "Demux") :
        DavWaveClassCategory(type_index(typeid(*this)), type_index(typeid(std::string)), nameTag) {}
};
struct DavWaveClassMux : public DavWaveClassCategory {
    DavWaveClassMux (const string & nameTag = "Mux") :
        DavWaveClassCategory(type_index(typeid(*this)), type_index(typeid(std::string)), nameTag) {}
};
struct DavWaveClassVideoDecode : public DavWaveClassCategory {
    DavWaveClassVideoDecode (const string & nameTag = "VideoDecode") :
        DavWaveClassCategory(type_index(typeid(*this)), type_index(typeid(std::string)), nameTag) {}
};
struct DavWaveClassVideoEncode : public DavWaveClassCategory {
    DavWaveClassVideoEncode (const string & nameTag = "VideoEncode") :
        DavWaveClassCategory(type_index(typeid(*this)), type_index(typeid(std::string)), nameTag) {}
};
struct DavWaveClassAudioDecode : public DavWaveClassCategory {
    DavWaveClassAudioDecode (const string & nameTag = "AudioDecode") :
        DavWaveClassCategory(type_index(typeid(*this)), type_index(typeid(std::string)), nameTag) {}
};
struct DavWaveClassAudioEncode : public DavWaveClassCategory {
    DavWaveClassAudioEncode (const string & nameTag = "AudioEncode") :
        DavWaveClassCategory(type_index(typeid(*this)), type_index(typeid(std::string)), nameTag) {}
};
struct DavWaveClassVideoFilter : public DavWaveClassCategory {
    DavWaveClassVideoFilter (const string & nameTag = "VideoFilter") :
        DavWaveClassCategory(type_index(typeid(*this)), type_index(typeid(std::string)), nameTag) {}
};
struct DavWaveClassAudioFilter : public DavWaveClassCategory {
    DavWaveClassAudioFilter (const string & nameTag = "AudioFilter") :
        DavWaveClassCategory(type_index(typeid(*this)), type_index(typeid(std::string)), nameTag) {}
};
struct DavWaveClassVideoMix : public DavWaveClassCategory {
    DavWaveClassVideoMix (const string & nameTag = "VideoMix") :
        DavWaveClassCategory(type_index(typeid(*this)), type_index(typeid(std::string)), nameTag) {}
};
struct DavWaveClassAudioMix : public DavWaveClassCategory {
    DavWaveClassAudioMix (const string & nameTag = "AudioMix") :
        DavWaveClassCategory(type_index(typeid(*this)), type_index(typeid(std::string)), nameTag) {}
};

///////////////////////////////////////////////////////////////////
//// Dav Audio/Video Data Type: Use DavOption Derived class as enum
using DavDataType = DavOption;

/* category can be set as key */
struct DavOptionDataTypeCategory : public DavOption {
    DavOptionDataTypeCategory() :
        DavOption(type_index(typeid(*this)), type_index(typeid(DavOption)), "DataTypeCategory") {}
};

struct DavOptionInputDataTypeCategory : public DavOption {
    DavOptionInputDataTypeCategory() :
        DavOption(type_index(typeid(*this)), type_index(typeid(DavOption)), "InputDataTypeCategory") {}
};

struct DavOptionOutputDataTypeCategory : public DavOption {
    DavOptionOutputDataTypeCategory() :
        DavOption(type_index(typeid(*this)), type_index(typeid(DavOption)), "OutputDataTypeCategory") {}
};

/* data type values */
struct DavDataTypeUndefined : public DavDataType {
    DavDataTypeUndefined() :
        DavOption(type_index(typeid(*this)), type_index(typeid(std::string)), "DataTypeUndefined") {}
};

struct DavDataInVideoBitstream : public DavDataType {
    DavDataInVideoBitstream() :
        DavOption(type_index(typeid(*this)), type_index(typeid(std::string)), "InVideoBitstream") {}
};
struct DavDataInAudioBitstream : public DavDataType {
    DavDataInAudioBitstream() :
        DavOption(type_index(typeid(*this)), type_index(typeid(std::string)), "InAudioBitstream") {}
};
struct DavDataInVideoRaw : public DavDataType {
    DavDataInVideoRaw() :
        DavOption(type_index(typeid(*this)), type_index(typeid(std::string)), "InVideoRaw") {}
};
struct DavDataInAudioRaw : public DavDataType {
    DavDataInAudioRaw() :
        DavOption(type_index(typeid(*this)), type_index(typeid(std::string)), "InAudioRaw") {}
};

struct DavDataOutVideoBitstream : public DavDataType {
    DavDataOutVideoBitstream() :
        DavOption(type_index(typeid(*this)), type_index(typeid(std::string)), "OutVideoBitstream") {}
};
struct DavDataOutAudioBitstream : public DavDataType {
    DavDataOutAudioBitstream() :
        DavOption(type_index(typeid(*this)), type_index(typeid(std::string)), "OutAudioBitstream") {}
};
struct DavDataOutVideoRaw : public DavDataType {
    DavDataOutVideoRaw() :
        DavOption(type_index(typeid(*this)), type_index(typeid(std::string)), "OutVideoRaw") {}
};
struct DavDataOutAudioRaw : public DavDataType {
    DavDataOutAudioRaw() :
        DavOption(type_index(typeid(*this)), type_index(typeid(std::string)), "OutAudioRaw") {}
};

} // namespace ff_dynamic
