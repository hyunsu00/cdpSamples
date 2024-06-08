#include <iostream>
#include <string>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <fstream>
#include <vector>
#include "nlohmann/json.hpp"

namespace beast = boost::beast;     // from <boost/beast.hpp>
namespace http = beast::http;       // from <boost/beast/http.hpp>
namespace websocket = beast::websocket; // from <boost/beast/websocket.hpp>
namespace net = boost::asio;        // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;   // from <boost/asio/ip/tcp.hpp>
using json = nlohmann::json;

// Function to decode base64 string to binary data
std::vector<unsigned char> base64Decode(const std::string& encoded_string) {
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

// Function to save screenshot to file
void saveScreenshot(const std::string& base64Data, const std::string& filename) {
    std::vector<unsigned char> decodedData = base64Decode(base64Data);
    std::ofstream file(filename, std::ios::binary);
    file.write(reinterpret_cast<const char*>(decodedData.data()), decodedData.size());
    file.close();
}

int main() {
    try {
        // The io_context is required for all I/O
        net::io_context ioc;

        // These objects perform our I/O
        tcp::resolver resolver{ioc};
        websocket::stream<tcp::socket> ws{ioc};

        // Look up the domain name
        auto const results = resolver.resolve("localhost", "9222");

        // Make the connection on the IP address we get from a lookup
        auto ep = net::connect(ws.next_layer(), results);

        // Update the host_ string. This will provide the value of the
        // Host HTTP header during the WebSocket handshake.
        // See https://tools.ietf.org/html/rfc7230#section-5.4
        std::string host = "localhost:9222";

        // Perform the websocket handshake
        ws.handshake(host, "/devtools/browser/5ec78c48-32ef-4d7b-94f9-ac38b997494c");

        json rmessage;
        {
            // Target.getTargets 호출
            // 사용가능한 모든 대상 리스트 반환 
            std::string message = R"({
                "id": 1,
                "method": "Target.getTargets"
            })";
            ws.write(net::buffer(std::string(message)));

            // Buffer to hold the incoming message
            beast::flat_buffer buffer;

            // Read a message into our buffer
            ws.read(buffer);

            // Convert the buffer to a string
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
            std::size_t bytesWrite = ws.write(net::buffer(std::string(message)));

            // Buffer to hold the incoming message
            beast::flat_buffer buffer;

            // Read a message into our buffer
            ws.read(buffer); buffer.clear();
            ws.read(buffer);

            // Convert the buffer to a string
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

            // Buffer to hold the incoming message
            beast::flat_buffer buffer;

            // Read a message into our buffer
            ws.read(buffer);

            // Convert the buffer to a string
            std::string response = beast::buffers_to_string(buffer.data());
            rmessage = json::parse(response);
            std::string screenshotData = rmessage["result"]["data"].get<std::string>();
            saveScreenshot(screenshotData, "screenshot.png");
        }
        

        // Parse the JSON response
        // Json::CharReaderBuilder reader;
        // Json::Value jsonResponse;
        // std::string errs;
        // std::istringstream s(response);

        // if (Json::parseFromStream(reader, s, &jsonResponse, &errs)) {
        //     if (jsonResponse["id"].asInt() == 2) {
        //         std::string screenshotData = jsonResponse["result"]["data"].asString();
        //         saveScreenshot(screenshotData, "screenshot.png");
        //         std::cout << "Screenshot saved successfully." << std::endl;
        //     } else {
        //         std::cerr << "Failed to take screenshot." << std::endl;
        //     }
        // }

        // Close the WebSocket connection
        ws.close(websocket::close_code::normal);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }

    return 0;
}
