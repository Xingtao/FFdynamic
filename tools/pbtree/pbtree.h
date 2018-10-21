#pragma once

#include <map>
#include <string>
#include <iostream>
#include <sstream>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <google/protobuf/message.h>
#include <google/protobuf/util/json_util.h>

namespace pb_tree {
using ::std::string;
using ::std::stringstream;
namespace pb = google::protobuf;
namespace pbutil = google::protobuf::util;
namespace bpt = boost::property_tree;

/* TODO: This project is incomplete. Right now use proto3's direcotly serialize-deserialize from json string,
         Will support proto2 <---> json string convertion later.
 */

/////////////////////////////////////////////////////////////
class PbTree {
public:
    /* pb to --> */
    static string pbToJsonString(const pb::Message & pbmsg) {
        string jsonstr;
        if (!pbutil::MessageToJsonString(pbmsg, &jsonstr, getJsonPrintOptions()).ok())
            return "";
        return jsonstr;
    }

    static int pbToJsonString(const pb::Message & pbmsg, string & outstr) {
        if (!pbutil::MessageToJsonString(pbmsg, &outstr, getJsonPrintOptions()).ok())
            return -1;
        return 0;
    }
    /* pb from <-- */
    static int pbFromJsonString(pb::Message & pbmsg, const string & jsonstr, bool ingnoreUnknown = true) {
        pbutil::JsonParseOptions po;
        po.ignore_unknown_fields = ingnoreUnknown;
        auto status = pbutil::JsonStringToMessage(jsonstr, &pbmsg, po);
        if (!status.ok()) {
            std::cerr << status.error_message() << std::endl;
            return -3;
        }
        return 0;
    }
    /* pb <-> ptree */
    static int pbToPtree(const pb::Message & pbmsg, bpt::ptree & t) {
        string jsonstr;
        pbutil::JsonPrintOptions o = getJsonPrintOptions();
        pbutil::Status s = pbutil::MessageToJsonString(pbmsg, &jsonstr, o);
        if (!s.ok()) {
            std::cerr << "pbmesg to json string fail: " << s.error_message() << "\n";
            return -1;
        }
        return ptreeFromJsonString(t, jsonstr);
    }
    static int pbFromPtree(pb::Message & pbmsg, const bpt::ptree & t) {
        stringstream ss;
        bpt::write_json(ss, t);
        string jsonstr = ss.str();
        return pbFromJsonString(pbmsg, jsonstr);
    }

    /* err description */
    static string errToString(const int err) {
        static const std::map<int, string> errMap = {
            {-1, "pbmsg to json string fail"},
            {-2, "parse json string to ptree fail"},
            {-3, "pbmsg from json string fail"}
        };
        return errMap.count(err) == 0 ? "unknown" : errMap.at(err);
    }

private:
    static int ptreeFromJsonString(bpt::ptree & t, const string & jsonstr) {
        stringstream ss(jsonstr);
        try {
            bpt::json_parser::read_json(ss, t);
        } catch (const bpt::json_parser::json_parser_error & ex) {
            std::cerr << "error parse json " << ex.filename() << " line " << ex.line()
                      << ", msg: " << ex.message() << "\n";
            return -2;
        };
        return 0;
    }
    static pbutil::JsonPrintOptions getJsonPrintOptions(bool bAddSpace = true, bool bPrintPrimitive = true,
                                                        bool bPreserveField = true) {
          pbutil::JsonPrintOptions options;
          options.add_whitespace = bAddSpace;
          options.always_print_primitive_fields = bPrintPrimitive;
          options.preserve_proto_field_names = bPreserveField;
          return options;
    }
};

} // namespace pb_tree
