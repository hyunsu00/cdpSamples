#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <iostream>
#include <regex>
#include <fstream>
#include "nlohmann/json.hpp"
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

    std::string request = "GET /json/version HTTP/1.1\r\nHost: localhost\r\n\r\n";
    size_t startPos = request.find("localhost");
    if(startPos != std::string::npos) {
        request.replace(startPos, strlen("localhost"), std::string(addr) + std::string(":") + std::to_string(port));
    }
    send(sock, request.c_str(), request.size(), 0);
    std::cout << "[request]: \n" << request << std::endl;

    std::string response;
    const int MAX_BUFFER_SIZE = 4096;
    char buffer[MAX_BUFFER_SIZE] = {0, };
#if 0    
    int totalReceived = 0, bytesReceived = 0;
    int contentLength = -1;
    while ((bytesReceived = recv(sock, buffer, MAX_BUFFER_SIZE - 1, 0)) > 0) {
        totalReceived += bytesReceived;
        buffer[bytesReceived] = '\0';
        response += buffer;

        if (contentLength == -1) {
            char* headerEnd = strstr(buffer, "\r\n\r\n");
            if (headerEnd) {
                char* contentLengthStr = strstr(buffer, "Content-Length:");
                if (contentLengthStr) {
                    sscanf(contentLengthStr, "Content-Length:%d", &contentLength);
                }
                int headerLength = headerEnd - buffer + 4;
                contentLength += headerLength;
            }
        }

        if (totalReceived >= contentLength) {
            break;
        }

        memset(buffer, 0, MAX_BUFFER_SIZE);
    }

    if (bytesReceived < 0) {
        std::cout << "Error reading server response" << std::endl;
        close(sock);
        return empty;
    }
#else
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
#endif
    std::cout << "[response]: \n" << response << std::endl;

    // body 영역 추출
    size_t pos = response.find("\r\n\r\n");
    json jBody;
    if (pos != std::string::npos) {
        std::string body = response.substr(pos + 4);
        std::cout << "[Body]: \n" << body << std::endl;
        jBody = json::parse(body);
    } else {
        std::cout << "No body found in the response" << std::endl;
        return empty;
    }
    
    close(sock);

    return jBody["webSocketDebuggerUrl"];
}

std::string getWebSocketKey() 
{
    return "dGhlIHNhbXBsZSBub25jZQ==";
}

void send_websocket_frame(int sock, const char* message) {
    unsigned char frame[10];
    size_t length = strlen(message);
    frame[0] = 0x81;

    if (length <= 125) {
        frame[1] = (unsigned char)length;
        write(sock, frame, 2);
    } else if (length <= 65535) {
        frame[1] = 126;
        frame[2] = (length >> 8) & 0xFF;
        frame[3] = length & 0xFF;
        write(sock, frame, 4);
    } else {
        frame[1] = 127;
        for (int i = 0; i < 8; ++i) {
            frame[2 + i] = (length >> (8 * (7 - i))) & 0xFF;
        }
        write(sock, frame, 10);
    }

    write(sock, message, length);
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
    handshakeRequest += std::string("Sec-WebSocket-Key: ") + getWebSocketKey() + "\r\n";
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

    {
        std::string navigateRequest = R"({"id":1,"method":"Page.navigate","params":{"url":"https://www.naver.com/"}})";
        std::vector<char> frame;
        frame.push_back(0x81); // FIN 비트 설정 및 텍스트 프레임
        frame.push_back(navigateRequest.size()); // 페이로드 길이
        frame.insert(frame.end(), navigateRequest.begin(), navigateRequest.end()); // 페이로드 추가
        ret = send(sock, &frame[0], frame.size(), 0);
        std::cout << "[request]: \n" << navigateRequest << std::endl;

        std::string navigateResponse;
        totalReceived = bytesReceived = 0;
        while (true) {
            memset(buffer, 0, MAX_BUFFER_SIZE);
            bytesReceived = read(sock, buffer, MAX_BUFFER_SIZE - 1);
            if (bytesReceived < 0) {
                std::cout << "Error reading server response" << std::endl;
                close(sock);
                return;
            } else if (bytesReceived == 0) {
                continue;
            }
            totalReceived += bytesReceived;
            buffer[bytesReceived] = '\0';
            navigateResponse += buffer;
            if (bytesReceived < (MAX_BUFFER_SIZE - 1)) {
                // 버퍼 크기보다 작은 데이터를 받았다면, 이는 데이터의 끝을 의미
                break;
            }
        }
        std::cout << "[response]: \n" << navigateResponse << std::endl;


    }

    {
        std::string screenshotRequest = R"({"id":2,"method":"Page.captureScreenshot","params":{"format":"png"}})";
        std::vector<char> frame;
        frame.push_back(0x81); // FIN 비트 설정 및 텍스트 프레임
        frame.push_back(screenshotRequest.size()); // 페이로드 길이
        frame.insert(frame.end(), screenshotRequest.begin(), screenshotRequest.end()); // 페이로드 추가
        ret = send(sock, reinterpret_cast<char*>(frame.data()), frame.size(), 0);
        std::cout << "[request]: \n" << screenshotRequest << std::endl;

        std::string screenshotResponse;
        totalReceived = bytesReceived = 0;
        const std::string savePath = "screenshot.png";
        std::ofstream file(savePath, std::ios::out | std::ios::binary);
        while (true) {
            memset(buffer, 0, MAX_BUFFER_SIZE);
            bytesReceived = read(sock, buffer, MAX_BUFFER_SIZE);
            if (bytesReceived < 0) {
                std::cout << "Error reading server response" << std::endl;
                close(sock);
                return;
            }
            totalReceived += bytesReceived;

            if (file.is_open()) {
                file.write(buffer, bytesReceived);
            } else {
                std::cerr << "Failed to save screenshot" << std::endl;
            }
            
            if (bytesReceived < (MAX_BUFFER_SIZE - 1)) {
                // 버퍼 크기보다 작은 데이터를 받았다면, 이는 데이터의 끝을 의미
                break;
            }
        }
        file.close();
        std::cout << "Screenshot saved as " << savePath << std::endl;
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

