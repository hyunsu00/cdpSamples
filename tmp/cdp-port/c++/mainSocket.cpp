#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <iostream>
#include <regex>
#include <fstream>
#include <functional>
#include "nlohmann/json.hpp"
#include "common/cdp_port_util.h"
// #include "websocket-parser-1.0.2/websocket_parser.h"
using json = nlohmann::json;

std::string getWebSocketDebuggerUrl(const char* addr = "127.0.0.1", uint16_t port = 9222) 
{
    const std::string empty;

    int sock = 0;
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        std::cout << "Socket creation error" << std::endl;
        return empty;
    }

    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    if(inet_pton(AF_INET, addr, &serv_addr.sin_addr) <= 0) {
        std::cout << "Invalid address/ Address not supported" << std::endl;
        return empty;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        std::cout << "Connection Failed" << std::endl;
        return empty;
    }

    std::string request = "GET /json/version HTTP/1.1\r\nHost: ${localhost}\r\n\r\n";
    size_t startPos = request.find("${localhost}");
    if(startPos != std::string::npos) {
        request.replace(startPos, strlen("${localhost}"), std::string(addr) + std::string(":") + std::to_string(port));
    }
    send(sock, request.c_str(), request.size(), 0);
    std::cout << "[request]: \n" << request << std::endl;

    std::string response;
    const int MAX_BUFFER_SIZE = 4096;
    char buffer[MAX_BUFFER_SIZE] = {0, };

    int totalReceived = 0, bytesReceived = 0;
    while (true) {
        bytesReceived = read(sock, buffer, MAX_BUFFER_SIZE - 1);
        if (bytesReceived < 0) {
            std::cout << "Error reading server response" << std::endl;
            close(sock);
            return empty;
        } else if (bytesReceived == 0) {
            std::cout << "No bytes received from server" << std::endl;
            close(sock);
            return empty;
        }
        totalReceived += bytesReceived;
        buffer[bytesReceived] = '\0';
        response += buffer;
        if (bytesReceived < (MAX_BUFFER_SIZE - 1)) {
            // 버퍼 크기보다 작은 데이터를 받았다면, 이는 데이터의 끝을 의미
            break;
        }
    }
    std::cout << "[http][response] :\n" << response << std::endl;

    // body 영역 추출
    size_t pos = response.find("\r\n\r\n");
    json message;
    if (pos != std::string::npos) {
        std::string responseBody = response.substr(pos + 4);
        message = json::parse(responseBody);
        std::cout << "[http][response.body()] :\n" << message.dump(4) << std::endl;
    } else {
        std::cout << "No body found in the response" << std::endl;
        return empty;
    }
    
    close(sock);

    return message["webSocketDebuggerUrl"];
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

    int sock = 0;
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        std::cout << "Socket creation error" << std::endl;
        return;
    }

    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, addr.c_str(), &serv_addr.sin_addr) <= 0) {
        std::cout << "Invalid address/ Address not supported" << std::endl;
        return;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        std::cout << "Connection Failed" << std::endl;
        return;
    }

    // WebSocket 핸드쉐이크 요청
    std::string handshakeRequest = "GET /devtools/browser/1d674235-4774-489b-889f-e4efb0adb5ed HTTP/1.1\r\n";
    size_t startPos = handshakeRequest.find("/devtools/browser/1d674235-4774-489b-889f-e4efb0adb5ed");
    if(startPos != std::string::npos) {
        handshakeRequest.replace(startPos, strlen("/devtools/browser/1d674235-4774-489b-889f-e4efb0adb5ed"), path);
    }
    handshakeRequest += "Host: 127.0.0.1:9222\r\n";
    startPos = handshakeRequest.find("127.0.0.1:9222");
    if(startPos != std::string::npos) {
        handshakeRequest.replace(startPos, strlen("127.0.0.1:9222"), std::string(addr) + std::string(":") + std::to_string(port));
    }
    handshakeRequest += "Upgrade: websocket\r\n";
    handshakeRequest += "Connection: Upgrade\r\n";
    handshakeRequest += "Sec-WebSocket-Version: 13\r\n";
    handshakeRequest += "Sec-WebSocket-Key: SGVsbG8gV29ybGQh\r\n";
    handshakeRequest += "\r\n";

    int ret = send(sock, handshakeRequest.c_str(), handshakeRequest.size(), 0);
    std::cout << "[request]: \n" << handshakeRequest << std::endl;

    // HTTP 응답 수신
    std::string response;
    const int MAX_BUFFER_SIZE = 4096;
    char buffer[MAX_BUFFER_SIZE] = {0, };
    int totalReceived = 0, bytesReceived = 0;
    while (true) {
        memset(buffer, 0, MAX_BUFFER_SIZE);
        bytesReceived = read(sock, buffer, MAX_BUFFER_SIZE - 1);
        if (bytesReceived < 0) {
            std::cout << "Error reading server response" << std::endl;
            close(sock);
            return;
        } else if (bytesReceived == 0) {
            std::cout << "No bytes received from server" << std::endl;
            close(sock);
            return;
        }
        totalReceived += bytesReceived;
        buffer[bytesReceived] = '\0';
        response += buffer;
        if (bytesReceived < (MAX_BUFFER_SIZE - 1)) {
            // 버퍼 크기보다 작은 데이터를 받았다면, 이는 데이터의 끝을 의미
            break;
        }
    }
    std::cout << "[response]: \n" << response << std::endl;

    json rmessage;
    {
        // Target.getTargets 호출
        // 사용가능한 모든 대상 리스트 반환 
        std::string message = R"({
            "id": 1,
            "method": "Target.getTargets"
        })";
        std::vector<char> frame = createWebSocketBuffer(message.c_str(), message.size());
        ret = send(sock, reinterpret_cast<char*>(frame.data()), frame.size(), 0);
        std::cout << "[request]: \n" << json::parse(message).dump(4) << std::endl;

        std::vector<char> byteBuf;
        const size_t BUF_LEN = 4096;
	    std::vector<char> recvBuf(BUF_LEN, 0);
        while (true) {
            int recvBytes = recv(sock, &recvBuf[0], static_cast<int>(recvBuf.size()), 0);
            if (recvBytes < 0) {
                std::cout << "Error reading server response" << std::endl;
                close(sock);
                return;
            } else if (recvBytes == 0) {
                std::cout << "No bytes received from server" << std::endl;
                close(sock);
                return;
            }
            byteBuf.insert(byteBuf.end(), recvBuf.begin(), recvBuf.begin() + recvBytes);
            if (recvBytes < BUF_LEN) {
                // 버퍼 크기보다 작은 데이터를 받았다면, 이는 데이터의 끝을 의미
                break;
            }
        }

        FrameBodyVector frameBodyVector = getFrameBodyVector(&byteBuf[0], byteBuf.size());
        frameBodyVector[0].push_back('\0');
        std::string response = static_cast<char*>(&frameBodyVector[0][0]);
        rmessage = json::parse(response);

        std::cout << "[response] :\n" << rmessage.dump(4) << std::endl;
    }

    {
        // Target.attachToTarget 호출
        // 대상 타겟 중 페이지 타겟을 찾아서 첫 번째 페이지 타겟에 대해 attachToTarget 호출
        std::string message = R"({
            "id": 2,
            "method": "Target.attachToTarget",
            "params": {
                "targetId": "${targetInfo.targetId}",
                "flatten": true
            }
        })";

        size_t startPos = message.find("${targetInfo.targetId}");
        if(startPos != std::string::npos) {
            json message_result = rmessage["result"];
            json target_info;
            for (auto& element : message_result["targetInfos"]) {
                if (element["type"] == "page") {
                    target_info = element;
                    break;
                }
            }
            message.replace(startPos, strlen("${targetInfo.targetId}"), target_info["targetId"].get<std::string>());
        }
        std::vector<char> frame = createWebSocketBuffer(message.c_str(), message.size());
        ret = send(sock, reinterpret_cast<char*>(frame.data()), frame.size(), 0);
        std::cout << "[request]: \n" << json::parse(message).dump(4) << std::endl;

        std::vector<char> byteBuf;
        const size_t BUF_LEN = 4096;
	    std::vector<char> recvBuf(BUF_LEN, 0);
        while (true) {
            int recvBytes = recv(sock, &recvBuf[0], static_cast<int>(recvBuf.size()), 0);
            if (recvBytes < 0) {
                std::cout << "Error reading server response" << std::endl;
                close(sock);
                return;
            } else if (recvBytes == 0) {
                std::cout << "No bytes received from server" << std::endl;
                close(sock);
                return;
            }
            byteBuf.insert(byteBuf.end(), recvBuf.begin(), recvBuf.begin() + recvBytes);
            if (recvBytes < BUF_LEN) {
                // 버퍼 크기보다 작은 데이터를 받았다면, 이는 데이터의 끝을 의미
                break;
            }
        }

        FrameBodyVector frameBodyVector = getFrameBodyVector(&byteBuf[0], byteBuf.size());
        std::cout << "[response] :\n" << std::endl;
        for (auto& frameBody : frameBodyVector) {
            frameBody.push_back('\0');

            std::cout << json::parse(static_cast<char*>(&frameBody[0])).dump(4) << std::endl;
        }

        if (frameBodyVector.size() == 1) {
            byteBuf.clear();
            while (true) {
                int recvBytes = recv(sock, &recvBuf[0], static_cast<int>(recvBuf.size()), 0);
                if (recvBytes < 0) {
                    std::cout << "Error reading server response" << std::endl;
                    close(sock);
                    return;
                } else if (recvBytes == 0) {
                    std::cout << "No bytes received from server" << std::endl;
                    close(sock);
                    return;
                }
                byteBuf.insert(byteBuf.end(), recvBuf.begin(), recvBuf.begin() + recvBytes);
                if (recvBytes < BUF_LEN) {
                    // 버퍼 크기보다 작은 데이터를 받았다면, 이는 데이터의 끝을 의미
                    break;
                }
            }

            frameBodyVector = getFrameBodyVector(&byteBuf[0], byteBuf.size());
            frameBodyVector[0].push_back('\0');
            std::string response = static_cast<char*>(&frameBodyVector[0][0]);
            rmessage = json::parse(response);

            std::cout << "[response] :\n" << rmessage.dump(4) << std::endl;
        } else if (frameBodyVector.size() == 2) {
            rmessage = json::parse(static_cast<char*>(&frameBodyVector[1][0]));
        } else {
            std::cout << "Invalid response" << std::endl;
            close(sock);
            return;
        }
    }

    {
        // Page.captureScreenshot 호출
        std::string message = R"({
            "sessionId": "${message.result.sessionId}",
            "id": 3,
            "method": "Page.captureScreenshot",
            "params": {
                "format": "png",
                "quality": 100,
                "fromSurface": true
            }
        })";

        size_t startPos = message.find("${message.result.sessionId}");
        if(startPos != std::string::npos) {
            message.replace(startPos, strlen("${message.result.sessionId}"), rmessage["result"]["sessionId"].get<std::string>());
        }

        std::vector<char> frame = createWebSocketBuffer(message.c_str(), message.size());
        ret = send(sock, reinterpret_cast<char*>(frame.data()), frame.size(), 0);
        std::cout << "[request]: \n" << json::parse(message).dump(4) << std::endl;

        std::vector<char> byteBuf;
        const size_t BUF_LEN = 4096;
	    std::vector<char> recvBuf(BUF_LEN, 0);
        while (true) {
            int recvBytes = recv(sock, &recvBuf[0], static_cast<int>(recvBuf.size()), 0);
            if (recvBytes < 0) {
                std::cout << "Error reading server response" << std::endl;
                close(sock);
                return;
            } else if (recvBytes == 0) {
                std::cout << "No bytes received from server" << std::endl;
                close(sock);
                return;
            }
            byteBuf.insert(byteBuf.end(), recvBuf.begin(), recvBuf.begin() + recvBytes);
            if (recvBytes < BUF_LEN) {
                // 버퍼 크기보다 작은 데이터를 받았다면, 이는 데이터의 끝을 의미
                break;
            }
        }

        FrameBodyVector frameBodyVector = getFrameBodyVector(&byteBuf[0], byteBuf.size());
        frameBodyVector[0].push_back('\0');
        std::string response = static_cast<char*>(&frameBodyVector[0][0]);
        rmessage = json::parse(response);
        
        std::cout << "[response] :\n" << rmessage.dump(4) << std::endl;

        std::string screenshotData = rmessage["result"]["data"].get<std::string>();
        {
            std::vector<unsigned char> decodedData = base64Decode(screenshotData);
            std::ofstream file("mainSocket-screenshot.png", std::ios::binary);
            file.write(reinterpret_cast<const char*>(decodedData.data()), decodedData.size());
            file.close();
        }
    }
    close(sock);
}

int main()
{
    std::string webSocketDebuggerUrl = getWebSocketDebuggerUrl();
    
    std::cout << "webSocketDebuggerUrl : " << webSocketDebuggerUrl << std::endl;
    
    takeScreenshot(webSocketDebuggerUrl);

    return 0;
}

