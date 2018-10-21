#pragma once

#include <map>
#include <mutex>
#include <regex>
#include <limits>
#include <thread>
#include <sstream>
#include <string>
#include <iostream>
#include <functional>
#include <unordered_set>
#define BOOST_SYSTEM_NO_DEPRECATED
#include <boost/asio.hpp>
#include <boost/asio/steady_timer.hpp>

#include "httpCommon.h"

/* Note:
   1. This file is forked and modified from 'https://github.com/eidheim/Simple-Web-Server'
   2. For asynchronized read/write, be aware our buffer/socket outlives the operation. */

namespace http_util {
using ::std::map;
using ::std::vector;
using ::std::string;
using ::std::size_t;
using ::std::istream;
using ::std::ostream;
using ::std::streamsize;
using ::std::stringstream;
using ::std::shared_ptr;
using ::std::unique_ptr;
using ::std::make_shared;
namespace asio = boost::asio;
using error_code = boost::system::error_code;
namespace errc = boost::system::errc;
namespace make_error_code = boost::system::errc;

//////////////////////////////////////////////////////////////////////////////////////////

class RegexOrderable : public std::regex {
public:
    RegexOrderable(const char *str) : std::regex(str), m_str(str) {}
    RegexOrderable(const string &str) : std::regex(str), m_str(str) {}
    bool operator<(const RegexOrderable &rhs) const noexcept {
        return m_str < rhs.m_str;
    }
private:
    string m_str;
};


class Request {
public:
    Request(shared_ptr<asio::ip::tcp::endpoint> & remoteEndpoint) noexcept
        : m_remoteEndpoint(std::move(remoteEndpoint)) {
    }
    shared_ptr<asio::ip::tcp::endpoint> & getRemoteEndpoint(){
        return m_remoteEndpoint;
    }
    CaseInsensitiveMultimap parseQueryString() noexcept {
        return http_util::QueryString::parse(m_queryString);
    }

public:
    asio::streambuf m_request;
    CaseInsensitiveMultimap m_header;
    string m_method, m_path, m_queryString, m_httpVersion;
    std::chrono::system_clock::time_point m_headerReadTime;
    shared_ptr<asio::ip::tcp::endpoint> m_remoteEndpoint;
    std::smatch m_pathMatch;
};

// Connection Class
class Connection : public std::enable_shared_from_this<Connection> {
public:
    Connection(asio::io_service & ioService)
        : m_socket(new asio::ip::tcp::socket(ioService)) {
    }
    unique_ptr<asio::ip::tcp::socket> m_socket;
    std::mutex m_socketCloseMutex;
    unique_ptr<asio::steady_timer> m_timer;
    shared_ptr<asio::ip::tcp::endpoint> m_remoteEndpoint;

public:
    void setTimeout(int64_t seconds) noexcept {
        if (seconds == 0) {
            m_timer = nullptr;
            return;
        }
        m_timer = unique_ptr<asio::steady_timer>(new asio::steady_timer(m_socket->get_io_service()));
        m_timer->expires_from_now(std::chrono::seconds(seconds));
        auto self = this->shared_from_this();
        m_timer->async_wait([self](const error_code & ec) {
            if (!ec)
                self->close();
       });
    }
    void cancelTimeout() noexcept {
        error_code ec;
        if (m_timer)
            m_timer->cancel(ec);
    }
    void close() noexcept {
        error_code ec;
        std::unique_lock<std::mutex> lock(m_socketCloseMutex);
        m_socket->lowest_layer().shutdown(asio::ip::tcp::socket::shutdown_both, ec);
        m_socket->lowest_layer().close(ec);
    }
};

class Session {
public:
    Session(shared_ptr<Connection> connection) noexcept
        : m_connection(std::move(connection)) {
        if (m_connection->m_remoteEndpoint == nullptr) {
            error_code ec;
            m_connection->m_remoteEndpoint =
                make_shared<asio::ip::tcp::endpoint>(m_connection->m_socket->lowest_layer().remote_endpoint(ec));
        }
        m_request = make_shared<Request>(m_connection->m_remoteEndpoint);
        return;
    }
    shared_ptr<Connection> m_connection;
    shared_ptr<Request> m_request;
};

// response
class Response : public std::enable_shared_from_this<Response>, public ostream {
public:
    explicit Response(shared_ptr<Session> session) noexcept
        : ostream(&m_responseBuf), m_session(std::move(session)) {
    }

    void writeHeader(const CaseInsensitiveMultimap & header, const size_t contentSize) {
        bool contentLengthWritten = false;
        bool chunkedTransferEncoding = false;
        for (auto & field : header) {
            if (!contentLengthWritten &&
                caseInsensitiveEqual(field.first, "content-length"))
                contentLengthWritten = true;
            else if (!chunkedTransferEncoding &&
                     caseInsensitiveEqual(field.first, "transfer-encode") &&
                     caseInsensitiveEqual(field.second, "chunked"))
                chunkedTransferEncoding = true;
            *this << field.first << ": " << field.second << g_newLine;
        }

        if (!contentLengthWritten && !chunkedTransferEncoding &&
            m_closeConnectionAfterResponse) {
            *this << "Content-Length: " << contentSize << g_newLine << g_newLine;
        }
        else {
            *this << g_newLine;
        }
    }

    void write(const StatusCode statusCode = StatusCode::success_ok,
               const CaseInsensitiveMultimap & header = CaseInsensitiveMultimap()) {
        *this << g_httpVersion << " " << status_code(statusCode) << g_newLine;
        writeHeader(header, 0);
    }
    void write(const string & content, const StatusCode statusCode = StatusCode::success_ok,
               const CaseInsensitiveMultimap & header = CaseInsensitiveMultimap()) {
        *this << g_httpVersion << " " << status_code(statusCode) << g_newLine;
        writeHeader(header, content.size());
        if (!content.empty())
            *this << content;
    }

    void send(const std::function<void(const error_code &)> & callback = nullptr) noexcept {
        auto self = this->shared_from_this();
        asio::async_write(*m_session->m_connection->m_socket, m_responseBuf,
                          [self, callback](const error_code & ec, size_t) {
                              if (callback)
                                  callback(ec);
                          });
    }

public:
    asio::streambuf m_responseBuf;
    shared_ptr<Session> m_session;
    static const bool m_closeConnectionAfterResponse = true; /* for our usage, this is always true */
};

using HttpRequestHandle = std::function<int(shared_ptr<Response> &, shared_ptr<Request> &)>;

class HttpServer {
public:
    HttpServer() = default;
    HttpServer(const string & addr, const int16_t port) {init(addr, port);}
    int init(const string & addr, const int16_t port) {
        m_ioService = make_shared<asio::io_service>();
        assert(m_ioService != nullptr);
        m_acceptor = unique_ptr<asio::ip::tcp::acceptor>(new asio::ip::tcp::acceptor(*m_ioService));
        assert(m_acceptor != nullptr);
        m_bindEp = asio::ip::tcp::endpoint(asio::ip::address::from_string(addr), port);
        m_acceptor->open(m_bindEp.protocol());
        m_acceptor->set_option(asio::socket_base::reuse_address(true));
        m_acceptor->bind(m_bindEp);
        return 0;
    }

    /* order for matching the uri */
    map<RegexOrderable, map<std::string, HttpRequestHandle>> m_resources;
    std::function<void(shared_ptr<Request> &, const error_code &)> m_onError;

private:
    vector<std::thread> m_threads;
    unique_ptr<asio::ip::tcp::acceptor> m_acceptor;
    std::mutex m_connectionMutex;
    std::unordered_set<Connection *> m_connections;
    shared_ptr<asio::io_service> m_ioService;
    asio::ip::tcp::endpoint m_bindEp;

public:
    void start() {
        if (m_ioService->stopped())
            m_ioService->reset();
        m_acceptor->listen();
        /* wait the first asynchronized operation and then run io service in thradds */
        std::cout << "Http service run on " << m_bindEp << " \n";
        waitConnection();
        m_threads.clear();
        /* will run posted async tasks in threads. for our case, g_serverThreads = 2 is more than enough, */
        for (size_t k = 1; k < g_serverThreads; k++)
            m_threads.emplace_back([this]() {this->m_ioService->run();});
        m_ioService->run();
        for (auto & t : m_threads)
            t.join();
        return;
    }

    void stop() noexcept {
        if (m_acceptor) {
            error_code ec;
            m_acceptor->close(ec);
            {
                std::unique_lock<std::mutex> lock(m_connectionMutex);
                for (auto &connection : m_connections)
                    connection->close();
                m_connections.clear();
            }
            m_ioService->stop();
        }
    }
    virtual ~HttpServer() noexcept {stop();}

private:
    shared_ptr<Connection> createConnection() {
        auto connection = shared_ptr<Connection>(new Connection(*m_ioService),
            [&](Connection *connection) {
                {
                    std::unique_lock<std::mutex> lock(m_connectionMutex);
                    auto it = m_connections.find(connection);
                    if (it != m_connections.end())
                        m_connections.erase(it);
                }
                delete connection;
            });
        std::unique_lock <std::mutex> lock(m_connectionMutex);
        m_connections.emplace(connection.get());
        return connection;
    }

    void waitConnection() {
        auto connection = createConnection();
        m_acceptor->async_accept(*connection->m_socket,
            [this, connection] (const error_code & ec)
            {
                if (ec != asio::error::operation_aborted) // wait new connection immediately when accepting one
                    this->waitConnection();
                auto session = make_shared<Session>(connection);
                if (!ec) {
                    error_code socketOptionsEc;
                    session->m_connection->m_socket->set_option(asio::ip::tcp::no_delay(true), socketOptionsEc);
                    if (!socketOptionsEc)
                        onRead(session);
                } else if (m_onError)
                    m_onError(session->m_request, ec);
            }
        );
    }

    // read header first.
    void onRead(shared_ptr<Session> session) {
        session->m_connection->setTimeout(G_READ_REQUEST_HEADER_TIMEOUT_SECOND);
        asio::async_read_until(*session->m_connection->m_socket, session->m_request->m_request,
                               g_newLine + g_newLine, /* delimiter or complete condition */
            [this, session] (const error_code & ec, size_t byteTransferred) {
                auto & theRequest = session->m_request;
                session->m_connection->cancelTimeout();
                if (ec) {
                    if (m_onError)
                        m_onError(session->m_request, ec);
                    return;
                }
                /* async_read_until may get more bytes then 'until' */
                size_t additionalBytes = theRequest->m_request.size() - byteTransferred;
                // asio::streambuf header;
                // header.commit(buffer_copy(header.prepare(byteTransferred), theRequest->m_request.data()));
                // theRequest->m_request.consume(byteTransferred);
                std::istream iheader(&theRequest->m_request);
                if (RequestMessage::parse(iheader,
                                          theRequest->m_method,
                                          theRequest->m_path,
                                          theRequest->m_queryString,
                                          theRequest->m_httpVersion,
                                          theRequest->m_header) == false) {
                    if (m_onError)
                        m_onError(theRequest, asio::error::no_protocol_option);
                    return;
                }

                /* 1. Content-Length 2. Transfer-Encoding 3. both not found */
                /* if both Content-Length and Transfer-Encoding found, ignore Content-Length */
                auto headerTransferEncoding = theRequest->m_header.find(g_transferEncodingField);
                if (headerTransferEncoding != theRequest->m_header.end()) {
                    readChunkedTransferEncoded(session);
                    return;
                }

                /* process find the Content-Length field or nothing at all */
                auto headerCL = theRequest->m_header.find(g_contentLenField);
                if (headerCL == theRequest->m_header.end()) {
                    matchAndResponse(session);
                    return;
                }
                unsigned long long contentLen = 0;
                try {
                    contentLen = std::stoull(headerCL->second);
                }
                catch (const std::exception &) {
                    if (m_onError)
                        m_onError(theRequest, asio::error::no_protocol_option);
                    return;
                }
                if (contentLen == additionalBytes) { /* already read all, process request */
                    matchAndResponse(session);
                    return;
                }
                /* TODO: warning this situation if bigger then max content len; right now, truncate it */
                unsigned long long readContentLen = /* 10M data max */
                    contentLen > G_REQUEST_CONTENT_MAX_LEN ? G_REQUEST_CONTENT_MAX_LEN : contentLen;
                /* set read content timeout */
                session->m_connection->setTimeout(G_READ_REQUEST_CONTENT_TIMEOUT_SECOND);
                asio::async_read(*session->m_connection->m_socket, theRequest->m_request,
                                 asio::transfer_exactly(readContentLen - additionalBytes),
                                 [this, session](const error_code &ec, size_t) {
                                     session->m_connection->cancelTimeout();
                                     if (!ec) {
                                         matchAndResponse(session);
                                     }
                                     else if (m_onError)
                                         m_onError(session->m_request, ec);
                                     return;
                                 });
                /////////////////
                return;
            }
        );
    }

    void matchAndResponse(shared_ptr<Session> session) {
        /* Find method & path match, then call write (handle the request and do response) */
        for (auto & regexMethod : m_resources) {
            auto it = regexMethod.second.find(session->m_request->m_method);
            if (it != regexMethod.second.end()) {
                std::smatch smRes;
                if (std::regex_match(session->m_request->m_path, smRes, regexMethod.first)) {
                    session->m_request->m_pathMatch = std::move(smRes);
                    write(session, it->second);
                }
            }
        }
    }

    void write(shared_ptr<Session> session, HttpRequestHandle & f) {
        auto response = shared_ptr<Response>(new Response(session), [this] (Response *response_ptr) {
                /* for asyn sending is out of deleter scope, use shared again */
                auto response = shared_ptr<Response> (response_ptr);
                response->send([this, response] (const error_code & ec) {
                        if (!ec) {
                            if (response->m_closeConnectionAfterResponse)
                                return;
                        }
                        else if (this->m_onError)
                            this->m_onError(response->m_session->m_request, ec);
                        return;
                    });
            }
        );
        /* user defined handler, generate response content to 'response' */
        int ret = f (response, session->m_request);
        if (ret < 0 && m_onError)
            m_onError(session->m_request, asio::error::operation_aborted);
        return;
    }

    /* it is ok for args as ref, for they will be captured later by value*/
    void readChunkedTransferEncoded(const shared_ptr<Session> & session) {
       auto chunkBuf = make_shared<asio::streambuf>(G_REQUEST_CONTENT_MAX_LEN);
       session->m_connection->setTimeout(G_READ_REQUEST_CONTENT_TIMEOUT_SECOND);
       asio::async_read_until(*session->m_connection->m_socket, *chunkBuf, g_newLine,
           [this, session, chunkBuf] (const error_code & ec, size_t byteTransferred) {
               session->m_connection->cancelTimeout();
               if (ec) {
                   if (m_onError)
                       m_onError(session->m_request, ec);
                   return;
               }
               auto additionalBytes = chunkBuf->size() - byteTransferred;
               string line;
               std::istream chunkStream(chunkBuf.get());
               getline(chunkStream, line);
               /* the last newline \r\n is 2 byte, getline will take \r as the last char and throw away \n */
               line.pop_back();
               unsigned long long thisChunkSize = 0;
               try {
                   thisChunkSize = std::stoull(line, 0, 16); /* in hex */
               } catch(...) {
                   if (m_onError)
                       m_onError(session->m_request, make_error_code::make_error_code(errc::message_size));
                   return;
               }

               if (thisChunkSize == 0) { /* ok then, process this request */
                   matchAndResponse(session);
                   return;
               }

               size_t nextReceiveSize = thisChunkSize + g_newLine.size() - additionalBytes;
               session->m_connection->setTimeout(G_READ_REQUEST_CONTENT_TIMEOUT_SECOND);
               asio::async_read(*session->m_connection->m_socket, *chunkBuf, asio::transfer_exactly(nextReceiveSize),
                  [this, session, chunkBuf] (const error_code &ec, std::size_t /* bytesTransferred */) {
                       session->m_connection->cancelTimeout();
                       if (ec) {
                           auto response = make_shared<Response>(session);
                           response->write(StatusCode::client_error_bad_request);
                           response->send();
                           if (m_onError)
                               m_onError(session->m_request, ec);
                       }
                       auto & requestBuf = session->m_request->m_request;
                       if (requestBuf.size() + chunkBuf->size() - g_newLine.size() > G_REQUEST_CONTENT_MAX_LEN) {
                           auto response = make_shared<Response>(session);
                           response->write(StatusCode::client_error_payload_too_large);
                           response->send();
                           if (m_onError)
                               m_onError(session->m_request, make_error_code::make_error_code(errc::message_size));
                       }
                       /* ok then, get this chunk, copy to request's streambuf */
                       requestBuf.commit(asio::buffer_copy(requestBuf.prepare(chunkBuf->size() - g_newLine.size()),
                                                           chunkBuf->data()));
                       /* recursive call, receive next chunk */
                       readChunkedTransferEncoded(session);
             });
        });
        ////// take as code archor
        return;
    } /* end of 'readChunkedTransferEncoded' */
};

} // namespace
