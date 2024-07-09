#include "ConvertHtmlModule.h"
#include <unistd.h> // pipe, fork
#include <cstdio> // perror
#include <cstdlib> // exit
#include <sys/wait.h> // waitpid
#include <iostream> // std::cerr
#include <string> // std::string
#include <vector> // std::vector
#include <fstream> // std::ofstream
#include "nlohmann/json.hpp" // nlohmann::json

class CDP 
{
public:
    CDP();
    ~CDP();

public:
    bool Launch();
    bool Navegate(
        const std::string& url, 
        const std::pair<int, int>& viewportSize
    );
    bool Screenshot(
        const std::string& resultFilePath,
        const std::string& imageType,
        const std::pair<int, int>& clipPos,
        const std::pair<int, int>& clipSize
    );

private:
    bool _SendCommand(const std::string& command);
    bool _RecvCommand(std::string& command);
    nlohmann::json _WaitCommand(int id);
    nlohmann::json _WaitCommand(const std::string& method = "Page.loadEventFired");
    void _SaveFile(const std::string& resultFilePath, const std::string& base64Str);

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
    const std::string Page_getLayoutMetrics;
    const std::string Page_captureScreenshot;

private:
    int m_ID;
    std::string m_TargetID;
    std::string m_SessionID;

private:
    pid_t m_PID;
    int m_WriteFD; // 두 번째 파이프: fd 4 (쓰기)
    int m_ReadFD; // 첫 번째 파이프: fd 3 (읽기)
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
, Page_getLayoutMetrics(R"(
    { 
        "id": %d, 
        "method": "Page.getLayoutMetrics", 
        "sessionId": "%s"
    }
)")
, Page_captureScreenshot(R"(
    { 
        "id": %d, 
        "method": "Page.captureScreenshot",
        "params": {
            "format": "%s",
            "clip": {
                "x": %d,
                "y": %d,
                "width": %d,
                "height": %d,
                "scale": 1
            },
            "captureBeyondViewport": true
        },
        "sessionId": "%s"
    }
)")
, m_ID(0)
, m_TargetID()
, m_SessionID()
, m_PID(0)
{
}

CDP::~CDP()
{
}

bool CDP::_SendCommand(const std::string& command)
{
    std::vector<char> writeBuf;
    writeBuf.assign(command.begin(), command.end());
    writeBuf.push_back('\0');

#if defined(DEBUG) || defined(_DEBUG)
    nlohmann::json message = nlohmann::json::parse(&writeBuf[0]); 
    std::cout << "[CDP::_SendCommand()] : " << message.dump(4) << std::endl;
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

bool CDP::_RecvCommand(std::string& command)
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

nlohmann::json CDP::_WaitCommand(int id)
{
    std::string command;
    while (true) {
        if (!_RecvCommand(command)) {
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

nlohmann::json CDP::_WaitCommand(const std::string& method /*= "Page.loadEventFired"*/)
{
    std::string command;
    while (true) {
        if (!_RecvCommand(command)) {
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

bool CDP::Launch()
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

bool CDP::Navegate(const std::string& url, const std::pair<int, int>& viewportSize)
{
    // 탭 생성
    bool result = _SendCommand(_Format(Target_createTarget, ++m_ID));
    if (!result) {
        return false;
    }
    nlohmann::json message = _WaitCommand(m_ID);
    if (message.empty() || message.contains("error")) {
        return false;
    }
    std::string targetId = message["result"]["targetId"].get<std::string>();

    // 탭 연결
    result = _SendCommand(_Format(Target_attachToTarget, ++m_ID, targetId.c_str()));
    if (!result) {
        return false;
    }
    std::string command;
    result = _RecvCommand(command);
    if (!result) {
        return false;
    }
    message = nlohmann::json::parse(command);
    if (message.empty() || message.contains("error")) {
        return false;
    }
    std::string sessionId = message["params"]["sessionId"].get<std::string>();

    // 이벤트 활성화
    result = _SendCommand(_Format(Page_enable, ++m_ID, sessionId.c_str()));
    if (!result) {
        return false;
    }

    const int DEFAULT_WIDTH = 1280;
    const int DEFAULT_HEIGHT = 720;
    // 브라우저 크기 설정 (1280 x 720)
    int width = (viewportSize.first == -1) ? DEFAULT_WIDTH : viewportSize.first;
    int height = (viewportSize.second == -1) ? DEFAULT_HEIGHT : viewportSize.second;
    int screenWidth = width, screenHeight = height;
    result = _SendCommand(_Format(Emulation_setDeviceMetricsOverride, ++m_ID, width, height, screenWidth, screenHeight, sessionId.c_str()));
    if (!result) {
        return false;
    }

    // 페이지 로드
    result = _SendCommand(_Format(Page_navigate, ++m_ID, url.c_str(), sessionId.c_str()));
    if (!result) {
        return false;
    }

    message = _WaitCommand("Page.loadEventFired");
    if (message.empty() || message.contains("error")) {
        return false;
    }

    m_TargetID = targetId;
    m_SessionID = sessionId;
    return true;
}

bool CDP::Screenshot(
    const std::string& resultFilePath,
    const std::string& imageType,
    const std::pair<int, int>& clipPos,
    const std::pair<int, int>& clipSize
)
{
    // 레이아웃 메트릭스 가져오기
    bool result = _SendCommand(_Format(Page_getLayoutMetrics, ++m_ID, m_SessionID.c_str()));
    if (!result) {
        return false;
    }
    nlohmann::json message = _WaitCommand(m_ID);
    if (message.empty() || message.contains("error")) {
        return false;
    }

    // 스크린샷 캡처
    std::pair<int, int> contentSize = std::make_pair(
        message["result"]["contentSize"]["width"].get<int>(),
        message["result"]["contentSize"]["height"].get<int>()
    );
    int clipX = (clipPos.first == -1) ? 0 : clipPos.first;
    int clipY = (clipPos.second == -1) ? 0 : clipPos.second;
    int clipWidth = (clipSize.first == -1) ? contentSize.first : clipSize.first;
    int clipHeight = (clipSize.second == -1) ? contentSize.second : clipSize.second;
    result = _SendCommand(_Format(Page_captureScreenshot, ++m_ID, imageType.c_str(), clipX, clipY, clipWidth, clipHeight, m_SessionID.c_str()));
    if (!result) {
        return false;
    }
    message = _WaitCommand(m_ID);
    if (message.empty() || message.contains("error")) {
        return false;
    }
    std::string screenshotData = message["result"]["data"].get<std::string>();

    // 파일로 저장
    _SaveFile(resultFilePath, screenshotData);

    return true;
}

void CDP::_SaveFile(const std::string& resultFilePath, const std::string& base64Str)
{
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

        while (in_len-- && (encoded_string[in_] != '=') && is_base64(encoded_string[in_])) {
            char_array_4[i++] = encoded_string[in_];
            in_++;
            if (i == 4) {
                for (i = 0; i < 4; i++) {
                    char_array_4[i] = base64_chars.find(char_array_4[i]);
                }
                char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
                char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
                char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

                for (i = 0; (i < 3); i++) {
                    ret.push_back(char_array_3[i]);
                }
                i = 0;
            }
        }

        if (i) {
            for (j = i; j < 4; j++) {
                char_array_4[j] = 0;
            }
            for (j = 0; j < 4; j++) {
                char_array_4[j] = base64_chars.find(char_array_4[j]);
            }
            char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
            char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
            char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

            for (j = 0; (j < i - 1); j++) {
                ret.push_back(char_array_3[j]);
            }
        }

        return ret;
    };

    std::vector<unsigned char> decodedData = base64Decode(base64Str);
    std::ofstream file(resultFilePath, std::ios::binary);
    file.write(reinterpret_cast<const char*>(decodedData.data()), decodedData.size());
    file.close();
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
    cdp.Launch();
    cdp.Navegate("https://www.naver.com", std::make_pair(viewportWidth, vieweportHeight));
    cdp.Screenshot("screenshot.png", "png", std::make_pair(clipX, clipY), std::make_pair(clipWidth, clipHeight));

    return true;
}

bool ConvertHtmlModule::HtmlToPdf(
    const wchar_t* htmlURL,
    const wchar_t* resultFilePath,
    const wchar_t* margin,
    int isLandScape
) {
    return false;
}