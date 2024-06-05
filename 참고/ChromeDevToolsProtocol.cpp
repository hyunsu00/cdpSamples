#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <fstream>
#include <vector>
#pragma comment(lib, "Ws2_32.lib")

const std::string url = "http://example.com";
const std::string savePath = "screenshot.png";

int main() {
    // WinSock 초기화
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed" << std::endl;
        return 1;
    }

    // WebSocket 클라이언트 소켓 생성
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        std::cerr << "Socket creation failed" << std::endl;
        WSACleanup();
        return 1;
    }

    // Chrome Headless와 연결할 서버 정보 설정
    struct addrinfo hints = {};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    struct addrinfo* result = nullptr;
    if (getaddrinfo("127.0.0.1", "9222", &hints, &result) != 0) {
        std::cerr << "getaddrinfo failed" << std::endl;
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    // 서버에 연결
    if (connect(sock, result->ai_addr, (int)result->ai_addrlen) == SOCKET_ERROR) {
        std::cerr << "Connection failed" << std::endl;
        freeaddrinfo(result);
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    // WebSocket 핸드쉐이크 요청
    std::string handshakeRequest = "GET /devtools/browser/1d674235-4774-489b-889f-e4efb0adb5ed HTTP/1.1\r\n";
    handshakeRequest += "Host: 127.0.0.1:9222\r\n";
    handshakeRequest += "Upgrade: websocket\r\n";
    handshakeRequest += "Connection: Upgrade\r\n";
    handshakeRequest += "Sec-WebSocket-Version: 13\r\n";
    handshakeRequest += "Sec-WebSocket-Key: SGVsbG8gV29ybGQh\r\n";
    handshakeRequest += "\r\n";

    int ret = send(sock, handshakeRequest.c_str(), handshakeRequest.size(), 0);

    // HTTP 응답 수신
    char response[1024];
    ret = recv(sock, response, sizeof(response), 0);

    // 스크린샷 요청 (DevTools Protocol 사용)
    //std::string screenshotRequest = R"({"id":1,"method":"Page.captureScreenshot","params":{"format":"png"}})";
    std::string screenshotRequest = R"({"id":1,"method":"Page.navigate","params":{"url":"https://www.naver.com/"}})";
    std::vector<char> frame;
    frame.push_back(0x81); // FIN 비트 설정 및 텍스트 프레임
    frame.push_back(screenshotRequest.size()); // 페이로드 길이
    frame.insert(frame.end(), screenshotRequest.begin(), screenshotRequest.end()); // 페이로드 추가
    ret = send(sock, reinterpret_cast<char*>(frame.data()), frame.size(), 0);

    // 스크린샷 데이터 수신
    char screenshotData[4096] = { 0, };
    int bytesRead = recv(sock, screenshotData, sizeof(screenshotData), 0);

    // 스크린샷 데이터를 파일로 저장
    std::ofstream file(savePath, std::ios::out | std::ios::binary);
    if (file.is_open()) {
        file.write(screenshotData, bytesRead);
        file.close();
        std::cout << "Screenshot saved as " << savePath << std::endl;
    }
    else {
        std::cerr << "Failed to save screenshot" << std::endl;
    }

    // 클라이언트 소켓 닫기 및 WinSock 해제
    closesocket(sock);
    WSACleanup();

    return 0;
}