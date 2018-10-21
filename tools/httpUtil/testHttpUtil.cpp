#include <algorithm>
#include <fstream>
#include <vector>
#include <memory>
#include <string>
#include <unistd.h>

#define BOOST_SYSTEM_NO_DEPRECATED
#include <boost/filesystem.hpp>
// Added for the default_resource example
#define BOOST_SPIRIT_THREADSAFE
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include "httpServer.h"
#include "httpClient.h"

using namespace ::boost::property_tree;
using namespace http_util;
using ::std::shared_ptr;
using ::std::string;
using ::std::cout;
using ::std::cerr;
using ::std::endl;

static const string g_serverAddr = "127.0.0.1";
static const uint16_t g_serverPort = 8081;

int main() {
    HttpServer server(g_serverAddr, g_serverPort);
    server.m_resources["^/string$"]["POST"] =
        [](std::shared_ptr<Response> & response, std::shared_ptr<Request> & request) {
        // Retrieve string:
        cout << "it is here" << endl;
        auto & content = request->m_request;
        // request->content.string() is a convenience function for:
        // stringstream ss;
        // ss << request->content.rdbuf();
        string responseContent = "test ok";
        *response << "HTTP/1.1 200 OK\r\nContent-Length: " << responseContent.length() << "\r\n\r\n";
        // Alternatively, use one of the convenience functions, for instance:
        response->write(responseContent);
        return 0;
    };

    server.m_onError = [](std::shared_ptr <Request>&, const error_code & e) {
      // Handle errors here
      cout << e << endl;
    };

    std::thread serverThread([&server]() {server.start();});
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Note:
    shared_ptr<HttpClient> client = make_shared<HttpClient>(g_serverAddr, g_serverPort);
    string json_string = "{\"firstName\": \"John\",\"lastName\": \"Smith\",\"age\": 25}";
    // Synchronous request examples
    try {
        // set your callback to do error & response process
        client->request("POST", "/string", json_string);
    }
    catch(const std::exception & e) {
        cerr << "Client request exception occur: " << e.what() << endl;
    }

    cout << "good; server thread join " << endl;
    serverThread.join();
    return 0;
}
