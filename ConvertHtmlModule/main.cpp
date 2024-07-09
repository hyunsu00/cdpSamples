#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <fcntl.h>
#include <fstream>
#include "nlohmann/json.hpp"
#include "ConvertHtmlModule.h"
#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
using json = nlohmann::json;

auto base64Decode = [](const std::string &encoded_string) -> std::vector<unsigned char>
{
    static const std::string base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                            "abcdefghijklmnopqrstuvwxyz"
                                            "0123456789+/";
    auto is_base64 = [](unsigned char c) -> bool
    {
        return (isalnum(c) || (c == '+') || (c == '/'));
    };

    size_t in_len = encoded_string.size();
    size_t i = 0;
    size_t j = 0;
    size_t in_ = 0;
    unsigned char char_array_4[4], char_array_3[3];
    std::vector<unsigned char> ret;

    while (in_len-- && (encoded_string[in_] != '=') && is_base64(encoded_string[in_]))
    {
        char_array_4[i++] = encoded_string[in_];
        in_++;
        if (i == 4)
        {
            for (i = 0; i < 4; i++)
            {
                char_array_4[i] = base64_chars.find(char_array_4[i]);
            }
            char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
            char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
            char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

            for (i = 0; (i < 3); i++)
            {
                ret.push_back(char_array_3[i]);
            }
            i = 0;
        }
    }

    if (i)
    {
        for (j = i; j < 4; j++)
        {
            char_array_4[j] = 0;
        }
        for (j = 0; j < 4; j++)
        {
            char_array_4[j] = base64_chars.find(char_array_4[j]);
        }
        char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
        char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
        char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

        for (j = 0; (j < i - 1); j++)
        {
            ret.push_back(char_array_3[j]);
        }
    }

    return ret;
};

bool _send_request_message(int fd, const std::string& request)
{
    std::vector<char> writeBuf;
    writeBuf.assign(request.begin(), request.end());
    writeBuf.push_back('\0');

    // for debug
    json message = json::parse(&writeBuf[0]); 
    std::cout << "[_send_request_message()] : " << message.dump(4) << std::endl;

    size_t totalWritten = 0;
    size_t bytesToWrite = writeBuf.size();
    while (totalWritten < bytesToWrite) {
        ssize_t result = write(fd, &writeBuf[totalWritten], bytesToWrite - totalWritten);
        if (result < 0) {
            // 오류 처리: write 호출 실패
            if (errno == EINTR) {
                // 시그널에 의해 호출이 중단된 경우, 다시 시도
                continue;
            } else {
                // 다른 오류
                return false;
            }
        }
        totalWritten += result;
    }

    return true;
}

json _recv_response_message(int fd)
{
    std::vector<char> byteBuf;
    const size_t BUF_LEN = 4096;
    std::vector<char> readBuf(BUF_LEN, 0);
    ssize_t readBytes = 0;
    do {
        // fd에서 데이터 읽기
        readBytes = read(fd, &readBuf[0], static_cast<int>(readBuf.size()));
        if (readBytes > 0) {
            byteBuf.insert(byteBuf.end(), readBuf.begin(), readBuf.begin() + readBytes);
        }
    } while (readBytes > 0 && readBuf[readBytes -  1] != '\0'); // 데이터의 끝이 \0이거나 읽을 데이터가 없을 때까지 반복
    
    // for debug
    json rmessage = json::parse(static_cast<char*>(&byteBuf[0]));
    std::cout << "[_recv_response_message()] : " << rmessage.dump(4) << std::endl;
    return rmessage;
}

json _wait_for_message_id(int fd, int id)
{
    while (true) {
        json rmessage = _recv_response_message(fd);
        if (rmessage.contains("error")) {
            return rmessage;
        } else if (rmessage["id"] == id) {
            return rmessage;
        } else {
            continue;
        }
    }
}

json _wait_for_page_load(int fd)
{
     while (true) {
        json rmessage = _recv_response_message(fd);
        if (rmessage.contains("error")) {
            return rmessage;
        } else if (rmessage["method"] == "Page.loadEventFired") {
            return rmessage;
        } else {
            continue;
        }
    }
}

int main() {

#if 0
    ConvertHtmlModule::HtmlToImage(
        L"file:///hancom/dev/github.com/cdpSamples/ConvertHtmlModule/samples/sample_en-US.html",
        L"screenshot.png",
        L"png",
        -1,
        -1,
        -1,
        -1,
        -1,
        -1
    );
#endif

 # if 1   
    pid_t pid;
    int fd3[2];  // 첫 번째 파이프: fd 3 (읽기)
    int fd4[2];  // 두 번째 파이프: fd 4 (쓰기)
    // fd3 파이프 생성 (fd 3용, 읽기 / 쓰기)
    if (pipe(fd3) == -1) {
        perror("pipe 1");
        return 1;
    }
    // fd4 파이프 생성 (fd 3용, 읽기 / 쓰기)
    if (pipe(fd4) == -1) {
        perror("pipe 1");
        return 1;
    }

    printf("fd3[0]: %d, fd3[1]: %d, fd4[0]: %d, fd4[1]: %d\n", fd3[0], fd3[1], fd4[0], fd4[1]);

    // 자식 프로세스 생성
    pid = fork();

    if (pid == -1) {
        perror("fork");
        return 1;
    } else if (pid == 0) {
        // 자식 프로세스
        close(fd3[1]); // fd3 쓰기 닫기 (fd 3 읽기)
        close(fd4[0]); // fd4 읽기 닫기 (fd 4 쓰기)

        // fd3을 읽기 3 복제
        int fd = dup2(fd3[0], 3);
        if (fd == -1) {
            perror("dup2 fd1");
            return 1;
        }

        // fd4 쓰기 4로 복제
        fd = dup2(fd4[1], 4);
        if (fd == -1) {
            perror("dup2 fd2");
            return 1;
        }

        // 원본 파이프의 읽기 닫기 및 필요 없는 파일 디스크립터 닫기
        if (fd3[0] != 3) { // 3일경우 이미 닫혔음
            close(fd3[0]);
        }
        if (fd3[1] != 4) { // 4일경우 이미 닫혔음
            close(fd4[1]);
        }
        execlp("/opt/google/chrome/chrome", "/opt/google/chrome/chrome", "--enable-features=UseOzonePlatform", "--ozone-platform=wayland", "--remote-debugging-pipe", NULL);
      
        // execlp 실패 시
        perror("execlp");
        return 1;
    } else {
        // 부모 프로세스
        close(fd3[0]); // fd3 읽기 닫기
        close(fd4[1]); // fd4 쓰기 닫기

        // int status;
        // waitpid(pid, &status, 0);
        
        // 탭 생성
        std::string message = R"({ "id": 1, "method": "Target.createTarget", "params": { "url": "about:blank" }})";
        bool result = _send_request_message(fd3[1], message);
        json rmessage = _wait_for_message_id(fd4[0], 1);

        // 탭 연결
        message = R"(
            {
                "id": 2,
                "method": "Target.attachToTarget",
                "params": {
                    "targetId": "${targetId}", 
                    "flatten": true
                }
            }
        )";
        size_t pos = message.find("${targetId}");
        if(pos != std::string::npos) {
            message.replace(pos, strlen("${targetId}"), rmessage["result"]["targetId"].get<std::string>());
        }
        result = _send_request_message(fd3[1], message);

        rmessage = _recv_response_message(fd4[0]);
        std::string session_id = rmessage["params"]["sessionId"].get<std::string>();

        // 이벤트 활성화
        message = R"(
            {
                "id": 3,
                "method": "Page.enable",
                "sessionId": "${sessionId}"
            }
        )";
        pos = message.find("${sessionId}");
        if(pos != std::string::npos) {
            message.replace(pos, strlen("${sessionId}"), session_id);
        }
        result = _send_request_message(fd3[1], message);

        // 브라우저 크기 설정 (1280 x 720)
        message = R"(
            {
                "id": 4,
                "method": "Emulation.setDeviceMetricsOverride",
                "params": {
                    "mobile": false,
                    "width": 1280,
                    "height": 720,
                    "screenWidth": 1280,
                    "screenHeight": 720,
                    "deviceScaleFactor": 1,
                    "screenOrientation": {
                        "angle": 0,
                        "type": "landscapePrimary"
                    }
                },
                "sessionId": "${sessionId}"
            }
        )";
        pos = message.find("${sessionId}");
        if(pos != std::string::npos) {
            message.replace(pos, strlen("${sessionId}"), session_id);
        }
        result = _send_request_message(fd3[1], message);

        // 페이지 로드
        // message = R"(
        //     {
        //         "id": 5,
        //         "method": "Page.navigate",
        //         "params": {"url": "file:///hancom/dev/github.com/cdpSamples/ConvertHtmlModule/samples/sample_en-US.html"},
        //         "sessionId": "${sessionId}"
        //     }
        // )";
        message = R"(
            {
                "id": 5,
                "method": "Page.navigate",
                "params": {"url": "https://www.naver.com"},
                "sessionId": "${sessionId}"
            }
        )";
        pos = message.find("${sessionId}");
        if(pos != std::string::npos) {
            message.replace(pos, strlen("${sessionId}"), session_id);
        }
        result = _send_request_message(fd3[1], message);

        rmessage = _wait_for_page_load(fd4[0]);

# if 1
        // 레이아웃 메트릭스 가져오기
        message = R"(
            {
                "id": 6,
                "method": "Page.getLayoutMetrics",
                "sessionId": "${sessionId}"
            }
        )";
        pos = message.find("${sessionId}");
        if(pos != std::string::npos) {
            message.replace(pos, strlen("${sessionId}"), session_id);
        }
        result = _send_request_message(fd3[1], message);
        rmessage = _wait_for_message_id(fd4[0], 6);
        int contentWidth = rmessage["result"]["contentSize"]["width"].get<int>();
        int contentHeight = rmessage["result"]["contentSize"]["height"].get<int>();

        // 10초간 일시 중지한다.
        // std::this_thread::sleep_for(std::chrono::seconds(10));
        
        // 스크린샷 캡처
        message = R"(
            {
                "id": 7,
                "method": "Page.captureScreenshot",
                "params": {
                    "format": "png",
                    "clip": {
                        "x": 0,
                        "y": 0,
                        "width": ${contentWidth},
                        "height": ${contentHeight},
                        "scale": 1
                    },
                    "captureBeyondViewport": true
                },
                "sessionId": "${sessionId}"
            }
        )";
        pos = message.find("${sessionId}");
        if(pos != std::string::npos) {
            message.replace(pos, strlen("${sessionId}"), session_id);
        }
        pos = message.find("${contentWidth}");
        if(pos != std::string::npos) {
            message.replace(pos, strlen("${contentWidth}"), std::to_string(contentWidth));
        }
        pos = message.find("${contentHeight}");
        if(pos != std::string::npos) {
            message.replace(pos, strlen("${contentHeight}"), std::to_string(contentHeight));
        }
        result = _send_request_message(fd3[1], message);
        rmessage = _wait_for_message_id(fd4[0], 7);
        std::string screenshotData = rmessage["result"]["data"].get<std::string>();
        {
            std::vector<unsigned char> decodedData = base64Decode(screenshotData);
            std::ofstream file("screenshot.png", std::ios::binary);
            file.write(reinterpret_cast<const char*>(decodedData.data()), decodedData.size());
            file.close();
        }
#else
        // 10초간 일시 중지한다.
        // std::this_thread::sleep_for(std::chrono::seconds(10));
        
        // PDF로 인쇄
        // A4 사이즈 (210 x 297 mm)
        const float PAPER_WIDTH = 8.27F;
        const float PAPER_HEIGHT = 11.7F;
        message = R"(
            {
                "id": 6,
                "method": "Page.printToPDF",
                "params": {
                    "landscape": false,
                    "displayHeaderFooter": false,
                    "printBackground": false,
                    "scale": 1,
                    "paperWidth": ${PAPER_WIDTH},
                    "paperHeight": ${PAPER_HEIGHT},
                    "marginTop": 0.4,
                    "marginBottom": 0.4,
                    "marginLeft": 0.4,
                    "marginRight": 0.4,
                    "pageRanges": "",
                    "headerTemplate": "",
                    "footerTemplate": "",
                    "preferCSSPageSize": false,
                    "transferMode": "ReturnAsBase64",
                    "generateTaggedPDF": false,
                    "generateDocumentOutline": false
                },
                "sessionId": "${sessionId}"
            }
        )";
        pos = message.find("${sessionId}");
        if(pos != std::string::npos) {
            message.replace(pos, strlen("${sessionId}"), session_id);
        }
        pos = message.find("${PAPER_WIDTH}");
        if(pos != std::string::npos) {
            message.replace(pos, strlen("${PAPER_WIDTH}"), std::to_string(PAPER_WIDTH));
        }
        pos = message.find("${PAPER_HEIGHT}");
        if(pos != std::string::npos) {
            message.replace(pos, strlen("${PAPER_HEIGHT}"), std::to_string(PAPER_HEIGHT));
        }
        result = _send_request_message(fd3[1], message);
        rmessage = _wait_for_message_id(fd4[0], 6);
        std::string screenshotData = rmessage["result"]["data"].get<std::string>();
        {
            std::vector<unsigned char> decodedData = base64Decode(screenshotData);
            std::ofstream file("screenshot.pdf", std::ios::binary);
            file.write(reinterpret_cast<const char*>(decodedData.data()), decodedData.size());
            file.close();
        }
#endif
        // fd3 쓰기 닫기
        close(fd3[1]);
        // fd4 읽기 닫기
        close(fd4[0]);

        // 자식 프로세스가 종료될 때까지 기다림
        waitpid(pid, NULL, 0);
    }

#endif // #if 0

    return 0;
}