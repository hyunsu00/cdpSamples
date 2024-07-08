#include "ConvertHtmlModule.h"
#include <unistd.h> // pipe, fork
#include <cstdio> // perror
#include <cstdlib> // exit
#include <sys/wait.h> // waitpid
#include <iostream> // std::cerr
#include <string> // std::string
#include <vector> // std::vector
#include "nlohmann/json.hpp" // nlohmann::json

class CDP 
{
public:
    CDP();
    ~CDP();

public:
    bool launch();
    bool navegate(const std::string& url);

private:
    bool _sendCommand(const std::string& command);
    bool _recvCommand(std::string& command);
    nlohmann::json _waitCommand(int id);
    nlohmann::json _waitCommand(const std::string& method = "Page.loadEventFired");

private:
    template<typename... Args>
    std::string _Format(const std::string& format, Args... args) {
        std::size_t size = snprintf(nullptr, 0, format.c_str(), args...) + 1; // Extra space for '\0'
        std::unique_ptr<char[]> buf(new char[size]);
        snprintf(buf.get(), size, format.c_str(), args...);
        return std::string(buf.get(), buf.get() + size - 1); // We don't want the '\0' inside
    }

private:
    const std::string Target_createTarget;
    const std::string Target_attachToTarget;
    const std::string Page_enable;
    const std::string Emulation_setDeviceMetricsOverride;
    const std::string Page_navigate;
private:
    pid_t m_PID;
    int m_WriteFD; // 두 번째 파이프: fd 4 (쓰기)
    int m_ReadFD; // 첫 번째 파이프: fd 3 (읽기)
private:
    int m_ID;
    std::string m_SessionID;
}; // class CDP

CDP::CDP()
: Target_createTarget(R"(
    { 
        "id": %d, 
        "method": "Target.createTarget", 
        "params": { 
            "url": "about:blank" 
        }
    }
)")
, Target_attachToTarget(R"(
    { 
        "id": %d, 
        "method": "Target.attachToTarget", 
        "params": { 
            "targetId": "%s", 
            "flatten": true 
        }
    }
)")
, Page_enable(R"(
    { 
        "id": %d, 
        "method": "Page.enable",
        "sessionId": "%s"
    }
)")
, Emulation_setDeviceMetricsOverride(R"(
    { 
        "id": %d, 
        "method": "Emulation.setDeviceMetricsOverride", 
        "params": { 
            "mobile": false,
            "width": %d, 
            "height": %d, 
            "screenWidth": %d,
            "screenHeight": %d,
            "deviceScaleFactor": 1, 
            "screenOrientation": {
                "angle": 0,
                "type": "landscapePrimary"
            }
        },
        "sessionId": "%s"
    }
)")
, Page_navigate(R"(
    { 
        "id": %d, 
        "method": "Page.navigate", 
        "params": { 
            "url": "%s"
        },
        "sessionId": "%s"
    }
)")
, m_PID(0)
, m_ID(0)
, m_SessionID()
{
}

CDP::~CDP()
{
}

bool CDP::_sendCommand(const std::string& command)
{
    std::vector<char> writeBuf;
    writeBuf.assign(command.begin(), command.end());
    writeBuf.push_back('\0');

#if defined(DEBUG) || defined(_DEBUG)
    nlohmann::json message = nlohmann::json::parse(&writeBuf[0]); 
    std::cout << "[CDP::_sendCommand()] : " << message.dump(4) << std::endl;
#endif // #if defined(DEBUG) || defined(_DEBUG)

    size_t totalWritten = 0;
    size_t bytesToWrite = writeBuf.size();
    while (totalWritten < bytesToWrite) {
        ssize_t result = write(m_WriteFD, &writeBuf[totalWritten], bytesToWrite - totalWritten);
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

bool CDP::_recvCommand(std::string& command)
{
    std::vector<char> byteBuf;
    const size_t BUF_LEN = 4096;
    std::vector<char> readBuf(BUF_LEN, 0);
    ssize_t readBytes = 0;
    do {
        // fd에서 데이터 읽기
        readBytes = read(m_ReadFD, &readBuf[0], static_cast<int>(readBuf.size()));
        if (readBytes > 0) {
            byteBuf.insert(byteBuf.end(), readBuf.begin(), readBuf.begin() + readBytes);
        } else if (readBytes < 0) {
            // 오류 처리: read 호출 실패
            if (errno == EINTR) {
                // 시그널에 의해 호출이 중단된 경우, 다시 시도
                continue;
            } else {
                // 다른 오류
                return false;
            }
        } 
    } while (readBytes > 0 && readBuf[readBytes -  1] != '\0'); // 데이터의 끝이 \0이거나 읽을 데이터가 없을 때까지 반복
    
#if defined(DEBUG) || defined(_DEBUG)
    nlohmann::json rmessage = nlohmann::json::parse(static_cast<char*>(&byteBuf[0]));
    std::cout << "[_recv_response_message()] : " << rmessage.dump(4) << std::endl;
#endif // #if defined(DEBUG) || defined(_DEBUG)
    command = &byteBuf[0];
    return true;
}

nlohmann::json CDP::_waitCommand(int id)
{
    std::string command;
    while (true) {
        if (!_recvCommand(command)) {
            break;
        }

        nlohmann::json message = nlohmann::json::parse(command);
        if (message.contains("error")) {
            return message;
        } else if (message["id"] == id) {
            return message;
        } else {
            continue;
        }
    }

    return nlohmann::json();
}

nlohmann::json CDP::_waitCommand(const std::string& method /*= "Page.loadEventFired"*/)
{
    std::string command;
    while (true) {
        if (!_recvCommand(command)) {
            break;
        }

        nlohmann::json message = nlohmann::json::parse(command);
        if (message.contains("error")) {
            return message;
        } else if (message["method"] == method) {
            return message;
        } else {
            continue;
        }
    }

    return nlohmann::json();
}

bool CDP::launch()
{
    int fd3[2] = {0, };  // 첫 번째 파이프: fd 3 (읽기)
    int fd4[2] = {0, };  // 두 번째 파이프: fd 4 (쓰기)
    // fd3 파이프 생성 (fd 3용, 읽기 / 쓰기)
    if (pipe(fd3) == -1) {
        return false;
    }
    // fd4 파이프 생성 (fd 3용, 읽기 / 쓰기)
    if (pipe(fd4) == -1) {
        return false;
    }

    // 자식 프로세스 생성
    pid_t pid = fork();
    if (pid == -1) {
        return false;
    } else if (pid == 0) {
        // 자식 프로세스
        close(fd3[1]); // fd3 쓰기 닫기 (fd 3 읽기)
        close(fd4[0]); // fd4 읽기 닫기 (fd 4 쓰기)

        // fd3을 읽기 3 복제
        int fd = dup2(fd3[0], 3);
        if (fd == -1) {
            perror("Error dup2(fd3[0], 3)");
            exit(1);
        }

        // fd4 쓰기 4로 복제
        fd = dup2(fd4[1], 4);
        if (fd == -1) {
            perror("Error dup2(fd4[1], 4)");
            exit(1);
        }

        // 원본 파이프의 읽기 닫기 및 필요 없는 파일 디스크립터 닫기
        if (fd3[0] != 3) { // 3일경우 이미 닫혔음
            close(fd3[0]);
        }
        if (fd3[1] != 4) { // 4일경우 이미 닫혔음
            close(fd4[1]);
        }
        int ret = execlp("/opt/google/chrome/chrome", "/opt/google/chrome/chrome", "--enable-features=UseOzonePlatform", "--ozone-platform=wayland", "--remote-debugging-pipe", NULL);

        if (ret == -1) {
            perror("Error execlp()");
            exit(1);
        }
    } else {
        // 부모 프로세스
        close(fd3[0]); // fd3 읽기 닫기
        close(fd4[1]); // fd4 쓰기 닫기

        int status;
        if (waitpid(pid, &status, WNOHANG) == 0) {
            m_PID = pid;
            m_WriteFD = fd3[1];
            m_ReadFD = fd4[0];
            return true; // 성공 반환
        }

        if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
            std::cerr << "자식 프로세스에서 오류 발생: " << WEXITSTATUS(status) << std::endl;
            return false;
        }
    }

    return true;
}

bool CDP::navegate(const std::string& url)
{
    // 탭 생성
    bool result = _sendCommand(_Format(Target_createTarget, ++m_ID));
    if (!result) {
        return false;
    }
    nlohmann::json message = _waitCommand(m_ID);
    if (message.empty() || message.contains("error")) {
        return false;
    }
    std::string targetId = message["result"]["targetId"].get<std::string>();

    // 탭 연결
    result = _sendCommand(_Format(Target_attachToTarget, ++m_ID, targetId.c_str()));
    if (!result) {
        return false;
    }
    std::string command;
    result = _recvCommand(command);
    if (!result) {
        return false;
    }
    message = nlohmann::json::parse(command);
    if (message.empty() || message.contains("error")) {
        return false;
    }
    std::string sessionId = message["params"]["sessionId"].get<std::string>();

    // 이벤트 활성화
    result = _sendCommand(_Format(Page_enable, ++m_ID, sessionId.c_str()));
    if (!result) {
        return false;
    }

    // 브라우저 크기 설정 (1280 x 720)
    result = _sendCommand(_Format(Emulation_setDeviceMetricsOverride, ++m_ID, 1280, 720, 1280, 720, sessionId.c_str()));
    if (!result) {
        return false;
    }

    // 페이지 로드
    result = _sendCommand(_Format(Page_navigate, ++m_ID, url.c_str(), sessionId.c_str()));
    if (!result) {
        return false;
    }

    message = _waitCommand("Page.loadEventFired");
    if (message.empty() || message.contains("error")) {
        return false;
    }
    return true;
}

bool ConvertHtmlModule::HtmlToImage(
        const wchar_t* htmlURL, 
        const wchar_t* resultFilePath, 
        const wchar_t* imageType, 
        int clipX, 
        int clipY, 
        int clipWidth, 
        int clipHeight, 
        int viewportWidth, 
        int vieweportHeight
) {
    CDP cdp;
    cdp.launch();
    cdp.navegate("https://www.naver.com");

    return false;
}

bool ConvertHtmlModule::HtmlToPdf(
    const wchar_t* htmlURL,
    const wchar_t* resultFilePath,
    const wchar_t* margin,
    int isLandScape
) {
    return false;
}