#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <iostream>
#include <nlohmann/json.hpp>
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
    send(sock, request.c_str(), request.size(), 0);
    std::cout << "HTTP request sent" << std::endl;

    std::string response;
#if 0
    while (true) {
        char buffer[1024] = {0};
        int bytesReceived = read(sock, buffer, sizeof(buffer) - 1);
        if (bytesReceived <= 0) {
            break;
        }
        response += buffer;
    }
#else
    char buffer[1024] = {0, };
    int bytesReceived = read(sock, buffer, sizeof(buffer) - 1);
    if (bytesReceived <= 0) {
        std::cout << "Error reading server response" << std::endl;
        close(sock);
        return empty;
    }
    response += buffer;
#endif

    std::cout << "Server response: " << response << std::endl;

    // body 영역 추출
    size_t pos = response.find("\r\n\r\n");
    json jBody;
    if (pos != std::string::npos) {
        std::string body = response.substr(pos + 4);
        std::cout << "Body: " << body << std::endl;
        jBody = json::parse(body);
    } else {
        std::cout << "No body found in the response" << std::endl;
        return empty;
    }
    
    close(sock);

    return jBody["webSocketDebuggerUrl"];
}

int main()
{
    std::string webSocketDebuggerUrl = getWebSocketDebuggerUrl();
    
    std::cout << "webSocketDebuggerUrl : " << webSocketDebuggerUrl << std::endl;
    
    return 0;
}

