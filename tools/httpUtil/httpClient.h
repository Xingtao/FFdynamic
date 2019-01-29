#pragma once

#include <random>
#include <vector>
#include <limits>
#include <mutex>
#include <unordered_set>
#include <iostream>

#include <boost/asio.hpp>
#include <boost/asio/steady_timer.hpp>

#include "httpCommon.h"

/* This file is forked and modified from 'https://github.com/eidheim/Simple-Web-Server' */

namespace http_util {
namespace asio = boost::asio;
using error_code = boost::system::error_code;
using ::std::cout;
using ::std::endl;
using ::std::map;
using ::std::string;
using ::std::vector;
using ::std::unique_ptr;
using ::std::shared_ptr;
using ::std::make_shared;

//////////////////////////////////////////////////////////////////////////////////////////

class HttpClient : public std::enable_shared_from_this<HttpClient> {
public:
    HttpClient(const string & serverIp, const uint16_t port) noexcept
        : m_serverEp(asio::ip::address::from_string(serverIp), port)
        , m_host(serverIp), m_port(port) {
        m_socket = make_shared<asio::ip::tcp::socket>(m_ioService);
        m_ioService.run();
    }

    virtual ~HttpClient() noexcept {
        m_socket->shutdown(asio::ip::tcp::socket::shutdown_receive);
        m_socket->close();
        return;
    }

public:
    /* API: user async 'request', then get 'response', processed by registerrd callback */
    void request(const string & method, const string & uri = "/", const string & content = "",
                 const CaseInsensitiveMultimap & headers = CaseInsensitiveMultimap(),
                 const std::function<void(const error_code&)> & callback = nullptr) {
        error_code ec;
        ec = connect();
        if (ec) {
            if (callback)
                callback(ec);
            return;
        }
        auto dataBuf = createRequest(method, uri, content, headers);
        auto self = shared_from_this();
        asio::async_write(*m_socket, *dataBuf,
            [self, callback](const error_code & ec, size_t){
                if (ec) {
                    if (callback)
                        callback(ec);
                }
                auto responseBuf = make_shared<asio::streambuf>();
                asio::async_read_until(*self->m_socket, *responseBuf, g_newLine + g_newLine,
                    [self, responseBuf, callback](const error_code &ec, size_t transferredBytes){
                        if (ec) {
                            if (callback)
                                callback(ec);
                        } // TODO: process response
                   });
            });
        return;
    }

private:
    error_code connect() {
        error_code ec;
        m_socket->connect(m_serverEp, ec);
        if (ec)
            return ec;
        asio::ip::tcp::no_delay option(true);
        m_socket->set_option(option, ec);
        return ec;
    }

    shared_ptr<asio::streambuf> createRequest(const string & method, const string & uri, const string & content,
                                              const CaseInsensitiveMultimap &headers) const {
        auto streambuf = make_shared<asio::streambuf>();
        std::ostream query_stream(streambuf.get());
        query_stream << method << " " << uri << " HTTP/1.0" << g_newLine;
        query_stream << "Host: " << m_host << ':' << std::to_string(m_port) << g_newLine;
        query_stream << "Accept: */*" << g_newLine;
        query_stream << "Content-Length: " << content.size() << g_newLine;
        /* explicit close indication for our usage */
        query_stream << "Connection: close" << g_newLine;
        /* may other header */
        for (const auto & h : headers)
            query_stream << h.first << ": " << h.second << g_newLine;
        /* complete the header with two new lines */
        query_stream << g_newLine;
        /* follow the content */
        query_stream << content;
        return streambuf;
    }

private:
    asio::io_service m_ioService;
    shared_ptr<asio::ip::tcp::socket> m_socket;
    asio::ip::tcp::endpoint m_serverEp;
    string m_host;
    uint16_t m_port;
};

} // namespace http_util
