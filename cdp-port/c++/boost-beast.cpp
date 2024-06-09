#include <iostream>
#include <string>
#include <regex>
#include <fstream>
#include <vector>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include "nlohmann/json.hpp"

namespace beast = boost::beast;     // from <boost/beast.hpp>
namespace http = beast::http;       // from <boost/beast/http.hpp>
namespace net = boost::asio;        // from <boost/asio.hpp>
namespace websocket = beast::websocket; // from <boost/beast/websocket.hpp>
using tcp = boost::asio::ip::tcp;   // from <boost/asio/ip/tcp.hpp>
using json = nlohmann::json;

std::string getWebSocketDebuggerUrl(const char* addr = "127.0.0.1", uint16_t port = 9222)
{
    auto const host = std::string(addr);
    auto const port_str = std::to_string(port);
    auto const target = "/json/version";
    int version = 11;

    net::io_context ioc;

    tcp::resolver resolver{ioc};
    tcp::socket socket{ioc};

    auto const results = resolver.resolve(host, port_str.c_str());
    net::connect(socket, results);

    http::request<http::string_body> request{http::verb::get, target, version};
    request.set(http::field::host, host);
    request.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

    http::write(socket, request);

    beast::flat_buffer buffer;
    http::response<http::dynamic_body> response;
    http::read(socket, buffer, response);

    auto const responseBody = boost::beast::buffers_to_string(response.body().data());
    auto const message = json::parse(responseBody);
    return message["webSocketDebuggerUrl"];
}

// Function to decode base64 string to binary data
std::vector<unsigned char> base64Decode(const std::string& encoded_string) 
{
    static const std::string base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                            "abcdefghijklmnopqrstuvwxyz"
                                            "0123456789+/";
    auto is_base64 = [](unsigned char c) -> bool {
        return (isalnum(c) || (c == '+') || (c == '/'));
    };

    size_t in_len = encoded_string.size();
    size_t i = 0;
    size_t j = 0;
    size_t in_ = 0;
    unsigned char char_array_4[4], char_array_3[3];
    std::vector<unsigned char> ret;

    while (in_len-- && (encoded_string[in_] != '=') && is_base64(encoded_string[in_])) {
        char_array_4[i++] = encoded_string[in_]; in_++;
        if (i == 4) {
            for (i = 0; i < 4; i++)
                char_array_4[i] = base64_chars.find(char_array_4[i]);

            char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
            char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
            char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

            for (i = 0; (i < 3); i++)
                ret.push_back(char_array_3[i]);
            i = 0;
        }
    }

    if (i) {
        for (j = i; j < 4; j++)
            char_array_4[j] = 0;

        for (j = 0; j < 4; j++)
            char_array_4[j] = base64_chars.find(char_array_4[j]);

        char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
        char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
        char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

        for (j = 0; (j < i - 1); j++) ret.push_back(char_array_3[j]);
    }

    return ret;
}

void takeScreenshot(const std::string& webSocketDebuggerUrl) 
{
    std::regex ip_regex("(\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}\\.\\d{1,3})");
    std::regex port_regex(":(\\d+)");
    std::regex path_regex("(/devtools/browser/[a-z0-9\\-]+)");
    std::smatch match;

    std::string addr = "127.0.0.1";
    if (std::regex_search(webSocketDebuggerUrl, match, ip_regex) && match.size() > 1) {
        addr = match.str(1);
        std::cout << "IP: " << addr << std::endl;
    } else {
        std::cout << "No valid IP found in string" << std::endl;
        return;
    }

    uint16_t port = 9222;
    if (std::regex_search(webSocketDebuggerUrl, match, port_regex) && match.size() > 1) {
        port = static_cast<uint16_t>(std::stoi(match.str(1)));
        std::cout << "Port: " << port << std::endl;
    } else {
        std::cout << "No valid port found in string" << std::endl;
        return;
    }

    std::string path;
    if (std::regex_search(webSocketDebuggerUrl, match, path_regex) && match.size() > 1) {
        path = match.str(1);
        std::cout << "Path: " << path << std::endl;
    } else {
        std::cout << "No valid path found in string" << std::endl;
        return;
    }

    // io_context는 모든 I/O에 필요합니다.
    net::io_context ioc;

    // 이 객체는 I/O를 수행합니다.
    tcp::resolver resolver{ioc};
    websocket::stream<tcp::socket> ws{ioc};

    // 도메인 이름을 찾는다.
    auto const results = resolver.resolve(addr, std::to_string(port));

    // 조회를 통해 얻은 IP 주소로 연결합니다.
    auto ep = net::connect(ws.next_layer(), results);

    // Host 문자열을 업데이트합니다. 
    // 이는 WebSocket 핸드셰이크 중에 호스트 HTTP 헤더 값을 제공합니다.
    // See https://tools.ietf.org/html/rfc7230#section-5.4
    std::string host = addr + ":" + std::to_string(port);

    // Perform the websocket handshake
    ws.handshake(host, path);

    json rmessage;
    {
        // Target.getTargets 호출
        // 사용가능한 모든 대상 리스트 반환 
        std::string message = R"({
            "id": 1,
            "method": "Target.getTargets"
        })";
        ws.write(net::buffer(std::string(message)));

        // 들어오는 메시지를 보관할 버퍼
        beast::flat_buffer buffer;

        // 버퍼로 메시지 읽기
        ws.read(buffer);

        // 버퍼를 문자열로 변환
        std::string response = beast::buffers_to_string(buffer.data());
        rmessage = json::parse(response);
    }
    
    {
        std::cout << rmessage << std::endl;
        // Target.attachToTarget 호출
        // 대상 타겟 중 페이지 타겟을 찾아서 첫 번째 페이지 타겟에 대해 attachToTarget 호출
        std::string message = R"({
            "id": 2,
            "method": "Target.attachToTarget",
            "params": {
                "targetId": "{targetInfo.targetId}",
                "flatten": true
            }
        })";

        size_t startPos = message.find("{targetInfo.targetId}");
        if(startPos != std::string::npos) {
            json message_result = rmessage["result"];
            json target_info;
            for (auto& element : message_result["targetInfos"]) {
                if (element["type"] == "page") {
                    target_info = element;
                    break;
                }
            }
            message.replace(startPos, strlen("{targetInfo.targetId}"), target_info["targetId"].get<std::string>());
        }
        ws.write(net::buffer(std::string(message)));

        // 들어오는 메시지를 보관할 버퍼
        beast::flat_buffer buffer;

        // 버퍼로 메시지 읽기
        ws.read(buffer); buffer.consume(buffer.size());
        ws.read(buffer);

        // 버퍼를 문자열로 변환
        std::string response = beast::buffers_to_string(buffer.data());
        rmessage = json::parse(response);
    }

    {
        std::cout << rmessage << std::endl;

        // Page.captureScreenshot 호출
        std::string message = R"({
            "sessionId": "{message.result.sessionId}",
            "id": 3,
            "method": "Page.captureScreenshot",
            "params": {
                "format": "png",
                "quality": 100,
                "fromSurface": true
            }
        })";

        size_t startPos = message.find("{message.result.sessionId}");
        if(startPos != std::string::npos) {
            message.replace(startPos, strlen("{message.result.sessionId}"), rmessage["result"]["sessionId"].get<std::string>());
        }
        ws.write(net::buffer(std::string(message)));

        // 들어오는 메시지를 보관할 버퍼
        beast::flat_buffer buffer;

        // 버퍼로 메시지 읽기
        ws.read(buffer);

        // 버퍼를 문자열로 변환
        std::string response = beast::buffers_to_string(buffer.data());
        rmessage = json::parse(response);
        std::string screenshotData = rmessage["result"]["data"].get<std::string>();
        {
            std::vector<unsigned char> decodedData = base64Decode(screenshotData);
            std::ofstream file("screenshot.png", std::ios::binary);
            file.write(reinterpret_cast<const char*>(decodedData.data()), decodedData.size());
            file.close();
        }
    }
    // WebSocket 연결을 닫습니다.
    ws.close(websocket::close_code::normal);
}

int main() 
{
    try {
        std::string webSocketDebuggerUrl = getWebSocketDebuggerUrl();
        std::cout << "webSocketDebuggerUrl : " << webSocketDebuggerUrl << std::endl;
        takeScreenshot(webSocketDebuggerUrl);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }

    return 0;
}
