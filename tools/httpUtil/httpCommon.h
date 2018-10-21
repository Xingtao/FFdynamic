#pragma once

#include <atomic>
#include <memory>
#include <map>
#include <string>
#include <vector>
#include <iostream>
#include <unordered_map>

/* This file is forked and modified from 'https://github.com/eidheim/Simple-Web-Server' */

namespace http_util {
using ::std::map;
using ::std::vector;
using ::std::string;

//////////////////////////////////////////////////////////////////////////////////////////
const string g_httpVersion = "HTTP/1.1";
const string g_contentLenField = "Content-Length";
const string g_transferEncodingField = "Transfer-Encoding";
const string g_newLine = "\r\n";
const uint32_t g_serverThreads = 2;
const uint32_t G_READ_REQUEST_HEADER_TIMEOUT_SECOND = 5;    // 5s for reading header
const uint32_t G_READ_REQUEST_CONTENT_TIMEOUT_SECOND = 50; // 50s for reading content
const uint32_t G_REQUEST_CONTENT_MAX_LEN = 10*1024*1024; // 10m

inline bool caseInsensitiveEqual(const string &str1, const string &str2) noexcept {
    return str1.size() == str2.size() &&
        std::equal(str1.begin(), str1.end(), str2.begin(),
                   [](char a, char b) {
                       return tolower(a) == tolower(b);
                   });
}

class CaseInsensitiveEqual {
public:
  bool operator()(const string &str1, const string &str2) const
      noexcept {
    return caseInsensitiveEqual(str1, str2);
  }
};

// Based on
// https://stackoverflow.com/questions/2590677/how-do-i-combine-hash-values-in-c0x/2595226#2595226
class CaseInsensitiveHash {
public:
    std::size_t operator()(const string &str) const noexcept {
        std::size_t h = 0;
        std::hash<int> hash;
        for (auto c : str)
            h ^= hash(tolower(c)) + 0x9e3779b9 + (h << 6) + (h >> 2);
        return h;
    }
};

using CaseInsensitiveMultimap = std::unordered_multimap<
    string, string, CaseInsensitiveHash, CaseInsensitiveEqual>;

/// Percent encoding and decoding
class Percent {
public:
  /// Returns percent-encoded string
  static string encode(const string &value) noexcept {
      static auto hex_chars = "0123456789ABCDEF";

      string result;
      result.reserve(value.size()); // Minimum size of result

      for (auto &chr : value) {
          if (!((chr >= '0' && chr <= '9') || (chr >= 'A' && chr <= 'Z') ||
                (chr >= 'a' && chr <= 'z') || chr == '-' || chr == '.' ||
                chr == '_' || chr == '~'))
              result += string("%") +
                  hex_chars[static_cast<unsigned char>(chr) >> 4] +
                  hex_chars[static_cast<unsigned char>(chr) & 15];
          else
              result += chr;
      }
      return result;
  }

    /// Returns percent-decoded string
    static string decode(const string &value) noexcept {
        string result;
        result.reserve(value.size() / 3 +
                       (value.size() % 3)); // Minimum size of result

        for (std::size_t i = 0; i < value.size(); ++i) {
            auto &chr = value[i];
            if (chr == '%' && i + 2 < value.size()) {
                auto hex = value.substr(i + 1, 2);
                auto decoded_chr =
                    static_cast<char>(std::strtol(hex.c_str(), nullptr, 16));
                result += decoded_chr;
                i += 2;
            } else if (chr == '+')
                result += ' ';
            else
                result += chr;
        }

        return result;
    }
};

/// Query string creation and parsing
class QueryString {
public:
    /// Returns query string created from given field names and values
    static string create(const CaseInsensitiveMultimap &fields) noexcept {
        string result;
        bool first = true;
        for (auto &field : fields) {
            result += (!first ? "&" : "") + field.first + '=' +
                Percent::encode(field.second);
            first = false;
        }
        return result;
    }

    // Returns query keys with percent-decoded values.
    static CaseInsensitiveMultimap
    parse(const string &query_string) noexcept {
        CaseInsensitiveMultimap result;
        if (query_string.empty())
            return result;
        std::size_t name_pos = 0;
        auto name_end_pos = string::npos;
        auto value_pos = string::npos;
        for (std::size_t c = 0; c < query_string.size(); ++c) {
            if (query_string[c] == '&') {
                auto name = query_string.substr(name_pos,
                                                (name_end_pos == string::npos ? c : name_end_pos) - name_pos);
                if (!name.empty()) {
                    auto value = value_pos == string::npos
                        ? string()
                        : query_string.substr(value_pos, c - value_pos);
                    result.emplace(std::move(name), Percent::decode(value));
                }
                name_pos = c + 1;
                name_end_pos = string::npos;
                value_pos = string::npos;
            } else if (query_string[c] == '=') {
                name_end_pos = c;
                value_pos = c + 1;
            }
        }
        if (name_pos < query_string.size()) {
            auto name = query_string.substr(name_pos, name_end_pos - name_pos);
            if (!name.empty()) {
                auto value = value_pos >= query_string.size()
                    ? string()
                    : query_string.substr(value_pos);
                result.emplace(std::move(name), Percent::decode(value));
            }
        }

        return result;
    }
};

class HttpHeader {
public:
    /// Parse header fields
    static CaseInsensitiveMultimap parse(std::istream &stream) noexcept {
        CaseInsensitiveMultimap result;
        string line;
        getline(stream, line);
        std::size_t param_end;
        while ((param_end = line.find(':')) != string::npos) {
            std::size_t value_start = param_end + 1;
            std::cout << "header line --> " << line << "\n";
            while (value_start + 1 < line.size() && line[value_start] == ' ') {
                ++value_start;
            }
            if (value_start < line.size())
                result.emplace(line.substr(0, param_end),
                               line.substr(value_start, line.size() - value_start - 1));
            getline(stream, line);
        }
        return result;
    }

    class FieldValue {
    public:
        class SemicolonSeparatedAttributes {
        public:
            /// Parse Set-Cookie or Content-Disposition header field value.
            /// Attribute
            /// values are percent-decoded.
            static CaseInsensitiveMultimap parse(const string &str) {
                CaseInsensitiveMultimap result;

                std::size_t name_start_pos = string::npos;
                std::size_t name_end_pos = string::npos;
                std::size_t value_start_pos = string::npos;
                for (std::size_t c = 0; c < str.size(); ++c) {
                    if (name_start_pos == string::npos) {
                        if (str[c] != ' ' && str[c] != ';')
                            name_start_pos = c;
                    } else {
                        if (name_end_pos == string::npos) {
                            if (str[c] == ';') {
                                result.emplace(str.substr(name_start_pos, c - name_start_pos), string());
                                name_start_pos = string::npos;
                            } else if (str[c] == '=')
                                name_end_pos = c;
                        } else {
                            if (value_start_pos == string::npos) {
                                if (str[c] == '"' && c + 1 < str.size())
                                    value_start_pos = c + 1;
                                else
                                    value_start_pos = c;
                            } else if (str[c] == '"' || str[c] == ';') {
                                result.emplace(str.substr(name_start_pos, name_end_pos - name_start_pos),
                                               Percent::decode(str.substr(value_start_pos, c - value_start_pos)));
                                name_start_pos = string::npos;
                                name_end_pos = string::npos;
                                value_start_pos = string::npos;
                            }
                        }
                    }
                }
                if (name_start_pos != string::npos) {
                    if (name_end_pos == string::npos)
                        result.emplace(str.substr(name_start_pos), string());
                    else if (value_start_pos != string::npos) {
                        if (str.back() == '"')
                            result.emplace(str.substr(name_start_pos, name_end_pos - name_start_pos),
                                           Percent::decode(str.substr(value_start_pos, str.size() - 1)));
                        else
                            result.emplace(str.substr(name_start_pos, name_end_pos - name_start_pos),
                                           Percent::decode(str.substr(value_start_pos)));
                    }
                }
                return result;
            }
        };
    };
};

class RequestMessage {
public:
    /// Parse request line and header fields
    static bool parse(std::istream & stream, string & method, string & path, string & query_string,
                      string & version, CaseInsensitiveMultimap & header) noexcept {
        header.clear();
        string line;
        getline(stream, line);
        std::cout << "[start parse request] => " << line << std::endl;
        std::size_t method_end;
        if ((method_end = line.find(' ')) != string::npos) {
            method = line.substr(0, method_end);
            std::cout << method << std::endl;
            // POST /string HTTP/1.0
            std::size_t query_start = string::npos;
            std::size_t path_and_query_string_end = string::npos;
            for (std::size_t i = method_end + 1; i < line.size(); ++i) {
                if (line[i] == '?' && (i + 1) < line.size())
                    query_start = i + 1;
                else if (line[i] == ' ') {
                    path_and_query_string_end = i;
                    break;
                }
            }

            if (path_and_query_string_end != string::npos) {
                if (query_start != string::npos) {
                    path = line.substr(method_end + 1, query_start - method_end - 2);
                    query_string =
                        line.substr(query_start, path_and_query_string_end - query_start);
                } else {
                    path = line.substr(method_end + 1,
                                       path_and_query_string_end - method_end - 1);
                }
                std::size_t protocol_end;
                if ((protocol_end = line.find('/', path_and_query_string_end + 1)) != string::npos) {
                    if (line.compare(path_and_query_string_end + 1,
                                     protocol_end - path_and_query_string_end - 1, "HTTP") != 0) {
                        return false;
                    }
                    version = line.substr(protocol_end + 1, line.size() - protocol_end - 2);
                } else {
                    return false;
                }
                header = HttpHeader::parse(stream);
            } else {
                return false;
            }
        } else {
            return false;
        }
        return true;
    }
};

class ResponseMessage {
public:
    /// Parse status line and header fields
    static bool parse(std::istream & stream, string &version, string &status_code,
                      CaseInsensitiveMultimap & header) noexcept {
        header.clear();
        string line;
        getline(stream, line);
        std::size_t version_end = line.find(' ');
        if (version_end != string::npos) {
            if (5 < line.size())
                version = line.substr(5, version_end - 5);
            else
                return false;
            if ((version_end + 1) < line.size())
                status_code =
                    line.substr(version_end + 1, line.size() - (version_end + 1) - 1);
            else
                return false;

            header = HttpHeader::parse(stream);
        } else
            return false;
        return true;
    }
};

// Status Code Part
enum class StatusCode {
    unknown = 0,
    information_continue = 100,
    information_switching_protocols,
    information_processing,
    success_ok = 200,
    success_created,
    success_accepted,
    success_non_authoritative_information,
    success_no_content,
    success_reset_content,
    success_partial_content,
    success_multi_status,
    success_already_reported,
    success_im_used = 226,
    redirection_multiple_choices = 300,
    redirection_moved_permanently,
    redirection_found,
    redirection_see_other,
    redirection_not_modified,
    redirection_use_proxy,
    redirection_switch_proxy,
    redirection_temporary_redirect,
    redirection_permanent_redirect,
    client_error_bad_request = 400,
    client_error_unauthorized,
    client_error_payment_required,
    client_error_forbidden,
    client_error_not_found,
    client_error_method_not_allowed,
    client_error_not_acceptable,
    client_error_proxy_authentication_required,
    client_error_request_timeout,
    client_error_conflict,
    client_error_gone,
    client_error_length_required,
    client_error_precondition_failed,
    client_error_payload_too_large,
    client_error_uri_too_long,
    client_error_unsupported_media_type,
    client_error_range_not_satisfiable,
    client_error_expectation_failed,
    client_error_im_a_teapot,
    client_error_misdirection_required = 421,
    client_error_unprocessable_entity,
    client_error_locked,
    client_error_failed_dependency,
    client_error_upgrade_required = 426,
    client_error_precondition_required = 428,
    client_error_too_many_requests,
    client_error_request_header_fields_too_large = 431,
    client_error_unavailable_for_legal_reasons = 451,
    server_error_internal_server_error = 500,
    server_error_not_implemented,
    server_error_bad_gateway,
    server_error_service_unavailable,
    server_error_gateway_timeout,
    server_error_http_version_not_supported,
    server_error_variant_also_negotiates,
    server_error_insufficient_storage,
    server_error_loop_detected,
    server_error_not_extended = 510,
    server_error_network_authentication_required
};

static const std::map<StatusCode, string> statusCodeMap = {
    {StatusCode::unknown, "unknown"},
    {StatusCode::information_continue, "100 Continue"},
    {StatusCode::information_switching_protocols, "101 Switching Protocols"},
    {StatusCode::information_processing, "102 Processing"},
    {StatusCode::success_ok, "200 OK"},
    {StatusCode::success_created, "201 Created"},
    {StatusCode::success_accepted, "202 Accepted"},
    {StatusCode::success_non_authoritative_information, "203 Non-Authoritative Information"},
    {StatusCode::success_no_content, "204 No Content"},
    {StatusCode::success_reset_content, "205 Reset Content"},
    {StatusCode::success_partial_content, "206 Partial Content"},
    {StatusCode::success_multi_status, "207 Multi-Status"},
    {StatusCode::success_already_reported, "208 Already Reported"},
    {StatusCode::success_im_used, "226 IM Used"},
    {StatusCode::redirection_multiple_choices, "300 Multiple Choices"},
    {StatusCode::redirection_moved_permanently, "301 Moved Permanently"},
    {StatusCode::redirection_found, "302 Found"},
    {StatusCode::redirection_see_other, "303 See Other"},
    {StatusCode::redirection_not_modified, "304 Not Modified"},
    {StatusCode::redirection_use_proxy, "305 Use Proxy"},
    {StatusCode::redirection_switch_proxy, "306 Switch Proxy"},
    {StatusCode::redirection_temporary_redirect, "307 Temporary Redirect"},
    {StatusCode::redirection_permanent_redirect, "308 Permanent Redirect"},
    {StatusCode::client_error_bad_request, "400 Bad Request"},
    {StatusCode::client_error_unauthorized, "401 Unauthorized"},
    {StatusCode::client_error_payment_required, "402 Payment Required"},
    {StatusCode::client_error_forbidden, "403 Forbidden"},
    {StatusCode::client_error_not_found, "404 Not Found"},
    {StatusCode::client_error_method_not_allowed, "405 Method Not Allowed"},
    {StatusCode::client_error_not_acceptable, "406 Not Acceptable"},
    {StatusCode::client_error_proxy_authentication_required, "407 Proxy Authentication Required"},
    {StatusCode::client_error_request_timeout, "408 Request Timeout"},
    {StatusCode::client_error_conflict, "409 Conflict"},
    {StatusCode::client_error_gone, "410 Gone"},
    {StatusCode::client_error_length_required, "411 Length Required"},
    {StatusCode::client_error_precondition_failed, "412 Precondition Failed"},
    {StatusCode::client_error_payload_too_large, "413 Payload Too Large"},
    {StatusCode::client_error_uri_too_long, "414 URI Too Long"},
    {StatusCode::client_error_unsupported_media_type, "415 Unsupported Media Type"},
    {StatusCode::client_error_range_not_satisfiable, "416 Range Not Satisfiable"},
    {StatusCode::client_error_expectation_failed, "417 Expectation Failed"},
    {StatusCode::client_error_im_a_teapot, "418 I'm a teapot"},
    {StatusCode::client_error_misdirection_required, "421 Misdirected Request"},
    {StatusCode::client_error_unprocessable_entity, "422 Unprocessable Entity"},
    {StatusCode::client_error_locked, "423 Locked"},
    {StatusCode::client_error_failed_dependency, "424 Failed Dependency"},
    {StatusCode::client_error_upgrade_required, "426 Upgrade Required"},
    {StatusCode::client_error_precondition_required, "428 Precondition Required"},
    {StatusCode::client_error_too_many_requests, "429 Too Many Requests"},
    {StatusCode::client_error_request_header_fields_too_large, "431 Request Header Fields Too Large"},
    {StatusCode::client_error_unavailable_for_legal_reasons, "451 Unavailable For Legal Reasons"},
    {StatusCode::server_error_internal_server_error, "500 Internal Server Error"},
    {StatusCode::server_error_not_implemented, "501 Not Implemented"},
    {StatusCode::server_error_bad_gateway, "502 Bad Gateway"},
    {StatusCode::server_error_service_unavailable, "503 Service Unavailable"},
    {StatusCode::server_error_gateway_timeout, "504 Gateway Timeout"},
    {StatusCode::server_error_http_version_not_supported, "505 HTTP Version Not Supported"},
    {StatusCode::server_error_variant_also_negotiates, "506 Variant Also Negotiates"},
    {StatusCode::server_error_insufficient_storage, "507 Insufficient Storage"},
    {StatusCode::server_error_loop_detected, "508 Loop Detected"},
    {StatusCode::server_error_not_extended, "510 Not Extended"},
    {StatusCode::server_error_network_authentication_required, "511 Network Authentication Required"}
};

inline const string & status_code(StatusCode statusCode) noexcept {
    auto pos = statusCodeMap.find(statusCode);
    if (pos == statusCodeMap.end()) {
        static string unknown_string("unknown");
        return unknown_string;
    }
    return pos->second;
}

} // namespace http_utils
