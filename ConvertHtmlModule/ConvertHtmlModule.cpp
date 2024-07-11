#include "ConvertHtmlModule.h"

#ifdef OS_WIN
#include <windows.h>
#else
#include <unistd.h> // pipe, fork
#include <sys/wait.h> // waitpid
#endif

#include <cstdio> // perror
#include <cstdlib> // exit
#include <iostream> // std::cerr
#include <string> // std::string
#include <vector> // std::vector
#include <fstream> // std::ofstream
#include "nlohmann/json.hpp" // nlohmann::json
#include <codecvt> // std::codecvt_utf8
#include <locale> // std::wstring_convert

struct CDPPipe
{
    CDPPipe() {}
    virtual ~CDPPipe() {}

    virtual bool Launch() = 0;
    virtual void Exit() = 0;
    virtual void SetTimeout(int timeoutSec) = 0;
    virtual bool Write(const std::string& command) = 0;
    virtual bool Read(std::string& command) = 0;
}; // struct CDPPipe

#ifdef OS_WIN
class CDPPipe_Windows : public CDPPipe
{
public:
    CDPPipe_Windows();
    virtual ~CDPPipe_Windows();

public:
    virtual bool Launch() override;
    virtual void Exit() override;
    virtual void SetTimeout(int timeoutSec) override;
    virtual bool Write(const std::string& command) override;
    virtual bool Read(std::string& command) override;

private:
    HANDLE m_hProcess;
    HANDLE m_hWrite; // 두 번째 파이프: fd 4 (쓰기)
    HANDLE m_hRead; // 첫 번째 파이프: fd 3 (읽기)
    DWORD m_dwTimeout; // 타임아웃설정(초), INFINITE이면 타임아웃 없음
}; // class CDPPipe_Windows

CDPPipe_Windows::CDPPipe_Windows()
: m_hProcess(INVALID_HANDLE_VALUE)
, m_hWrite(INVALID_HANDLE_VALUE)
, m_hRead(INVALID_HANDLE_VALUE)
, m_dwTimeout(INFINITE)
{
}

CDPPipe_Windows::~CDPPipe_Windows()
{
    Exit();
}

bool CDPPipe_Windows::Launch()
{
    HANDLE fd3[2] = { NULL, NULL }; // 첫 번째 파이프: fd 3 (읽기)
    HANDLE fd4[2] = { NULL, NULL }; // 두 번째 파이프: fd 4 (쓰기)

    // Set up the security attributes struct.
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    // fd3 파이프 생성 (fd 3용, 읽기 / 쓰기)
    if (!::CreatePipe(&fd3[0], &fd3[1], &sa, 0)) {
        return false;
    }

    // fd4 파이프 생성 (fd 4용, 읽기 / 쓰기)
    if (!::CreatePipe(&fd4[0], &fd4[1], &sa, 0)) {
       return false;
    }

    const int FD_COUNT = 5;
    BYTE flags[FD_COUNT] = { 0x41, 0x41, 0x41, 0x09, 0x09 };  
    HANDLE fds[FD_COUNT] = { NULL, NULL, NULL, fd3[0], fd4[1] };

    std::vector<BYTE> buffer(sizeof(FD_COUNT) + sizeof(flags) + sizeof(fds), 0);
    
    // FD count 설정
    memcpy(&buffer[0], &FD_COUNT, sizeof(FD_COUNT));
    // FD flag 설정
    memcpy(&buffer[sizeof(FD_COUNT)], &flags, sizeof(flags));
    // FD handle 설정
    memcpy(&buffer[sizeof(FD_COUNT) + sizeof(flags)], &fds[0], sizeof(fds));

    // Set up the start up info struct.
    STARTUPINFOW si = {0, };
    si.cb = sizeof(STARTUPINFOW);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdOutput = NULL;
    si.hStdInput = NULL;
    si.hStdError = NULL;
    si.wShowWindow = SW_SHOWDEFAULT;
    si.cbReserved2 = (WORD)buffer.size();
    si.lpReserved2 = (LPBYTE)&buffer[0];

    // Create the child process.
    wchar_t wszCommandLine[] = L".\\chrome\\chrome-headless-shell-win32\\chrome-headless-shell.exe --no-sandbox --disable-gpu, --remote-debugging-pipe";
    PROCESS_INFORMATION pi = {0, };
    if (!::CreateProcessW(NULL, // No module name (use command line)
        wszCommandLine,         // Command line
        &sa,                    // Process handle not inheritable
        &sa,                    // Thread handle not inheritable
        TRUE,                   // Set handle inheritance to TRUE
        NORMAL_PRIORITY_CLASS,  // No creation flags
        NULL,                   // Use parent's environment block
        NULL,                   // Use parent's starting directory
        &si,                    // Pointer to STARTUPINFO structure 
        &pi)                    // Pointer to PROCESS_INFORMATION structure
    ) {
        return false;
    }

    ::CloseHandle(fd3[0]);
    ::CloseHandle(fd4[1]);

    m_hProcess = pi.hProcess;
    m_hWrite = fd3[1];
    m_hRead = fd4[0];
    return true;
}

void CDPPipe_Windows::Exit()
{
    if (m_hWrite != INVALID_HANDLE_VALUE) {
        ::CloseHandle(m_hWrite);
    }
    if (m_hRead != INVALID_HANDLE_VALUE) {
        ::CloseHandle(m_hRead);
    }
    if (m_hProcess != INVALID_HANDLE_VALUE) {
        ::TerminateProcess(m_hProcess, 0);
    }
    m_hProcess = m_hWrite = m_hRead = INVALID_HANDLE_VALUE;
}

void CDPPipe_Windows::SetTimeout(int timeoutSec)
{
    m_dwTimeout = static_cast<DWORD>(timeoutSec * 1000);
}

bool CDPPipe_Windows::Write(const std::string& command)
{
    std::vector<char> writeBuf;
    writeBuf.assign(command.begin(), command.end());
    writeBuf.push_back('\0');

#if defined(DEBUG) || defined(_DEBUG)
    nlohmann::json message = nlohmann::json::parse(&writeBuf[0]); 
    std::cout << "[CDPPipe_Windows::Write()] : " << message.dump(4) << std::endl;
#endif // #if defined(DEBUG) || defined(_DEBUG)

    size_t totalWritten = 0;
    size_t bytesToWrite = writeBuf.size();

    OVERLAPPED overlapped = {0, };
    overlapped.hEvent = ::CreateEventW(NULL, TRUE, FALSE, NULL);
    if (overlapped.hEvent == NULL) {
        return false; // CreateEventW 실패
    }

    DWORD dwTimeout = m_dwTimeout; // 타임아웃 설정

    while (totalWritten < bytesToWrite) {
        DWORD written = 0;
        BOOL result = ::WriteFile(m_hWrite, &writeBuf[totalWritten], bytesToWrite - totalWritten, &written, &overlapped);
        if (!result) {
            if (::GetLastError() == ERROR_IO_PENDING) {
                DWORD waitResult = ::WaitForSingleObject(overlapped.hEvent, dwTimeout); // 타임아웃 설정
                if (waitResult == WAIT_TIMEOUT) {
                    // 타임아웃
                    ::CancelIo(m_hWrite);
                    ::CloseHandle(overlapped.hEvent);
                    return false;
                } else if (waitResult == WAIT_OBJECT_0) {
                    // 얼마나 쓰였는지 확인
                    if (!::GetOverlappedResult(m_hWrite, &overlapped, &written, FALSE)) {
                        // GetOverlappedResult 호출 실패
                        ::CloseHandle(overlapped.hEvent);
                        return false;
                    }
                    // 데이터 쓰기 성공
                    totalWritten += written;
                } else {
                    // WaitForSingleObject 호출 실패
                    ::CloseHandle(overlapped.hEvent);
                    return false;
                }
            } else {
                // WriteFile 호출 실패
                ::CloseHandle(overlapped.hEvent);
                return false;
            }
        } else {
            totalWritten += written;
        }
    }

    ::CloseHandle(overlapped.hEvent);

    return true;
}

bool CDPPipe_Windows::Read(std::string& command)
{
    std::vector<char> byteBuf;
    const size_t BUF_LEN = 4096;
    std::vector<char> readBuf(BUF_LEN, 0);
    DWORD readBytes = 0;

    OVERLAPPED overlapped = {0, };
    overlapped.hEvent = ::CreateEventW(NULL, TRUE, FALSE, NULL);
    if (overlapped.hEvent == NULL) {
        return false; // CreateEventW 실패
    }

    DWORD dwTimeout = m_dwTimeout; // 타임아웃 설정

    do {
        // fd에서 데이터 읽기
        readBytes = 0;
        BOOL result = ::ReadFile(m_hRead, &readBuf[0], static_cast<int>(readBuf.size()), &readBytes, &overlapped);
        if (!result) {
             if (::GetLastError() == ERROR_IO_PENDING) {
                DWORD waitResult = ::WaitForSingleObject(overlapped.hEvent, dwTimeout); // 타임아웃 설정
                if (waitResult == WAIT_TIMEOUT) {
                    // 타임아웃
                    ::CancelIo(m_hRead);
                    ::CloseHandle(overlapped.hEvent);
                    return false;
                } else if (waitResult == WAIT_OBJECT_0) {
                    // 얼마나 읽였는지 확인
                    if (!::GetOverlappedResult(m_hRead, &overlapped, &readBytes, FALSE)) {
                        // GetOverlappedResult 호출 실패
                        ::CloseHandle(overlapped.hEvent);
                        return false;
                    }
                    // 데이터 읽기 성공
                } else {
                    // WaitForSingleObject 호출 실패
                    ::CloseHandle(overlapped.hEvent);
                    return false;
                }
            } else {
                // ReadFile 호출 실패
                ::CloseHandle(overlapped.hEvent);
                return false;
            }
        }
        if (readBytes > 0) {
            byteBuf.insert(byteBuf.end(), readBuf.begin(), readBuf.begin() + readBytes);
        }
    } while (readBytes > 0 && readBuf[readBytes -  1] != '\0'); // 데이터의 끝이 \0이거나 읽을 데이터가 없을 때까지 반복
    
    ::CloseHandle(overlapped.hEvent);

#if defined(DEBUG) || defined(_DEBUG)
    nlohmann::json rmessage = nlohmann::json::parse(static_cast<char*>(&byteBuf[0]));
    std::cout << "[CDPPipe_Windows::Read()] : " << rmessage.dump(4) << std::endl;
#endif // #if defined(DEBUG) || defined(_DEBUG)
    command = &byteBuf[0];

    return true;
}
// --------------------------------------------------------------------------------
// End of CDPPipe_Windows class
// --------------------------------------------------------------------------------

#else // #ifdef OS_WIN

class CDPPipe_Linux : public CDPPipe
{
public:
    CDPPipe_Linux();
    virtual ~CDPPipe_Linux();

public:
    virtual bool Launch() override;
    virtual void Exit() override;
    virtual void SetTimeout(int timeoutSec) override;
    virtual bool Write(const std::string& command) override;
    virtual bool Read(std::string& command) override;

private:
    pid_t m_PID;
    int m_WriteFD; // 두 번째 파이프: fd 4 (쓰기)
    int m_ReadFD; // 첫 번째 파이프: fd 3 (읽기)
    std::unique_ptr<timeval> m_Timeout; // 타임아웃설정(초), nullptr이면 타임아웃 없음
}; // class CDPPipe_Linux

CDPPipe_Linux::CDPPipe_Linux()
: m_PID(-1)
, m_WriteFD(-1)
, m_ReadFD(-1)
, m_Timeout()
{
}

CDPPipe_Linux::~CDPPipe_Linux()
{
    Exit();
}

bool CDPPipe_Linux::Launch()
{
    int fd3[2] = {0, };  // 첫 번째 파이프: fd 3 (읽기)
    int fd4[2] = {0, };  // 두 번째 파이프: fd 4 (쓰기)
    // fd3 파이프 생성 (fd 3용, 읽기 / 쓰기)
    if (pipe(fd3) == -1) {
        return false;
    }
    // fd4 파이프 생성 (fd 4용, 읽기 / 쓰기)
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
        // int ret = execlp("/opt/google/chrome/chrome", "/opt/google/chrome/chrome", "--enable-features=UseOzonePlatform", "--ozone-platform=wayland", "--no-sandbox", "--disable-gpu", "--remote-debugging-pipe", NULL);
        int ret = execlp("/opt/google/chrome/chrome", "/opt/google/chrome/chrome", "--enable-features=UseOzonePlatform", "--ozone-platform=wayland", "--no-sandbox", "--disable-gpu", "--remote-debugging-pipe", "--headless", NULL);
        // int ret = execlp("./chrome/chrome-headless-shell-linux64/chrome-headless-shell", "./chrome/chrome-headless-shell-linux64/chrome-headless-shell", "--no-sandbox", "--disable-gpu", "--remote-debugging-pipe", NULL);

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

void CDPPipe_Linux::Exit()
{
    if (m_WriteFD != -1) {
        close(m_WriteFD);
    }
    if (m_ReadFD != -1) {
        close(m_ReadFD);
    }
    if (m_PID != -1) {
        kill(m_PID, SIGKILL);
    }
    m_PID = m_WriteFD = m_ReadFD = -1;
}

void CDPPipe_Linux::SetTimeout(int timeoutSec) /*override*/
{
    m_Timeout.reset(new timeval());
    m_Timeout->tv_sec = timeoutSec;
    m_Timeout->tv_usec = 0;
}

bool CDPPipe_Linux::Write(const std::string& command)
{
    std::vector<char> writeBuf;
    writeBuf.assign(command.begin(), command.end());
    writeBuf.push_back('\0');

#if defined(DEBUG) || defined(_DEBUG)
    nlohmann::json message = nlohmann::json::parse(&writeBuf[0]); 
    std::cout << "[CDPPipe_Linux::Write()] : " << message.dump(4) << std::endl;
#endif // #if defined(DEBUG) || defined(_DEBUG)

    size_t totalWritten = 0;
    size_t bytesToWrite = writeBuf.size();

    struct timeval* timeout = m_Timeout.get();

    fd_set writeFds;
    FD_ZERO(&writeFds);
    FD_SET(m_WriteFD, &writeFds);

    while (totalWritten < bytesToWrite) {
        int selectResult = select(m_WriteFD + 1, NULL, &writeFds, NULL, timeout);
        if (selectResult > 0) {
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
        } else if (selectResult == 0) {
            // 타임아웃
            return false;
        } else {
            // select 호출 실패
            return false;
        }
    }

    return true;
}

bool CDPPipe_Linux::Read(std::string& command)
{
    std::vector<char> byteBuf;
    const size_t BUF_LEN = 4096;
    std::vector<char> readBuf(BUF_LEN, 0);
    ssize_t readBytes = 0;

    struct timeval* timeout = m_Timeout.get();

    fd_set readFds;
    FD_ZERO(&readFds);
    FD_SET(m_ReadFD, &readFds);

    do {
        int selectResult = select(m_ReadFD + 1, &readFds, NULL, NULL, timeout);
        if (selectResult > 0) {
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
        } else if (selectResult == 0) {
            // 타임아웃
            return false;
        } else {
            // select 호출 실패
            return false;
        }
    } while (readBytes > 0 && readBuf[readBytes -  1] != '\0'); // 데이터의 끝이 \0이거나 읽을 데이터가 없을 때까지 반복
    
#if defined(DEBUG) || defined(_DEBUG)
    nlohmann::json rmessage = nlohmann::json::parse(static_cast<char*>(&byteBuf[0]));
    std::cout << "[CDPPipe_Linux::Read()] : " << rmessage.dump(4) << std::endl;
#endif // #if defined(DEBUG) || defined(_DEBUG)
    command = &byteBuf[0];
    return true;
}
// --------------------------------------------------------------------------------
// End of CDPPipe_Linux class
// --------------------------------------------------------------------------------
#endif // !#ifdef OS_WIN

class CDPManager
{
private:
    static std::string _W2UTF8(const std::wstring& wstr) {
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
        return converter.to_bytes(wstr);
    }

public:
    CDPManager();
    ~CDPManager();

public:
    bool Launch();
    void Exit();
    void SetTimeout(int timeoutSec = 5);
    bool Navegate(
        const std::wstring& url, 
        const std::pair<int, int>& viewportSize = std::make_pair(-1, -1)
    );
    bool CloseTab();
    bool Screenshot(
        const std::wstring& resultFilePath,
        const std::wstring& imageType = L"png",
        const std::pair<int, int>& clipPos = std::make_pair(-1, -1),
        const std::pair<int, int>& clipSize = std::make_pair(-1, -1)
    );
    bool PrintToPDF(
        const std::wstring& resultFilePath,
        double margin = 0.4F,
        bool landscape = false
    );

private:
    nlohmann::json _Wait(int id);
    nlohmann::json _Wait(const std::string& method = "Page.loadEventFired");
    void _SaveFile(const std::wstring& resultFilePath, const std::string& base64Str);

private:
    template<typename... Args>
    std::string _Format(const std::string& format, Args... args) {
        std::size_t size = snprintf(nullptr, 0, format.c_str(), args...) + 1; // Extra space for '\0'
        std::unique_ptr<char[]> buf(new char[size]);
        snprintf(buf.get(), size, format.c_str(), args...);
        return std::string(buf.get(), buf.get() + size - 1); // We don't want the '\0' inside
    }

private:
    const int TIMEOUT_SEC;
    const std::string Target_createTarget;
    const std::string Target_attachToTarget;
    const std::string Target_closeTarget;
    const std::string Page_enable;
    const std::string Emulation_setDeviceMetricsOverride;
    const std::string Page_navigate;
    const std::string Page_getLayoutMetrics;
    const std::string Page_captureScreenshot;
    const std::string Page_printToPDF;

private:
    int m_ID;
    std::string m_TargetID;
    std::string m_SessionID;

private:
    std::unique_ptr<CDPPipe> m_Pipe;
}; // class CDPManager

CDPManager::CDPManager()
: TIMEOUT_SEC(5)
, Target_createTarget(R"(
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
, Target_closeTarget(R"(
    { 
        "id": %d, 
        "method": "Target.closeTarget", 
        "params": { 
            "targetId": "%s" 
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
, Page_printToPDF(R"(
    { 
        "id": %d, 
        "method": "Page.printToPDF",
        "params": {
            "landscape": %s,
            "displayHeaderFooter": false,
            "printBackground": false,
            "scale": 1,
            "paperWidth": %f,
            "paperHeight": %f,
            "marginTop": %f,
            "marginBottom": %f,
            "marginLeft": %f,
            "marginRight": %f,
            "pageRanges": "",
            "headerTemplate": "",
            "footerTemplate": "",
            "preferCSSPageSize": false,
            "transferMode": "ReturnAsBase64",
            "generateTaggedPDF": false,
            "generateDocumentOutline": false
        },
        "sessionId": "%s"
    }
)")
, m_ID(0)
, m_TargetID()
, m_SessionID()
#ifdef OS_WIN
, m_Pipe(new CDPPipe_Windows())
#else // #ifdef OS_WIN
, m_Pipe(new CDPPipe_Linux())
#endif // !#ifdef OS_WIN
{
}

CDPManager::~CDPManager()
{
    Exit();
}

nlohmann::json CDPManager::_Wait(int id)
{
    std::string command;
    while (true) {
        if (!m_Pipe->Read(command)) {
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

nlohmann::json CDPManager::_Wait(const std::string& method /*= "Page.loadEventFired"*/)
{
    std::string command;
    while (true) {
        if (!m_Pipe->Read(command)) {
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

bool CDPManager::Launch()
{
    return m_Pipe->Launch();
}

void CDPManager::Exit()
{
    m_Pipe->Exit();
    m_ID = 0;
    m_TargetID.clear();
    m_SessionID.clear();
}

void CDPManager::SetTimeout(int timeoutSec /*= 5*/)
{
    m_Pipe->SetTimeout(timeoutSec);
}

bool CDPManager::Navegate(
    const std::wstring& url, 
    const std::pair<int, int>& viewportSize /*= std::make_pair(-1, -1)*/
)
{
    // 탭 생성
    bool result = m_Pipe->Write(_Format(Target_createTarget, ++m_ID));
    if (!result) {
        return false;
    }
    nlohmann::json message = _Wait(m_ID);
    if (message.empty() || message.contains("error")) {
        return false;
    }
    std::string targetId = message["result"]["targetId"].get<std::string>();

    // 탭 연결
    result = m_Pipe->Write(_Format(Target_attachToTarget, ++m_ID, targetId.c_str()));
    if (!result) {
        return false;
    }
    std::string command;
    result = m_Pipe->Read(command);
    if (!result) {
        return false;
    }
    message = nlohmann::json::parse(command);
    if (message.empty() || message.contains("error")) {
        return false;
    }
    std::string sessionId = message["params"]["sessionId"].get<std::string>();

    // 이벤트 활성화
    result = m_Pipe->Write(_Format(Page_enable, ++m_ID, sessionId.c_str()));
    if (!result) {
        return false;
    }

    const int DEFAULT_WIDTH = 1280;
    const int DEFAULT_HEIGHT = 720;
    // 브라우저 크기 설정 (1280 x 720)
    int width = (viewportSize.first == -1) ? DEFAULT_WIDTH : viewportSize.first;
    int height = (viewportSize.second == -1) ? DEFAULT_HEIGHT : viewportSize.second;
    int screenWidth = width, screenHeight = height;
    result = m_Pipe->Write(
        _Format(
            Emulation_setDeviceMetricsOverride, 
            ++m_ID, 
            width, 
            height, 
            screenWidth, 
            screenHeight, 
            sessionId.c_str()
        )
    );
    if (!result) {
        return false;
    }

    // 페이지 로드
    result = m_Pipe->Write(_Format(Page_navigate, ++m_ID, _W2UTF8(url).c_str(), sessionId.c_str()));
    if (!result) {
        return false;
    }

    message = _Wait("Page.loadEventFired");
    if (message.empty() || message.contains("error")) {
        return false;
    }

    m_TargetID = targetId;
    m_SessionID = sessionId;
    return true;
}

bool CDPManager::CloseTab()
{
    // 탭 생성
    bool result = m_Pipe->Write(_Format(Target_closeTarget, ++m_ID, m_TargetID.c_str()));
    if (!result) {
        return false;
    }

    nlohmann::json message = _Wait(m_ID);
    if (message.empty() || message.contains("error")) {
        return false;
    }

    return true;
}

bool CDPManager::Screenshot(
    const std::wstring& resultFilePath,
    const std::wstring& imageType /*= L"png"*/,
    const std::pair<int, int>& clipPos /*= std::make_pair(-1, -1)*/,
    const std::pair<int, int>& clipSize /*= std::make_pair(-1, -1)*/
)
{
    // 레이아웃 메트릭스 가져오기
    bool result = m_Pipe->Write(_Format(Page_getLayoutMetrics, ++m_ID, m_SessionID.c_str()));
    if (!result) {
        return false;
    }
    nlohmann::json message = _Wait(m_ID);
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
    result = m_Pipe->Write(
        _Format(
            Page_captureScreenshot, 
            ++m_ID, 
            _W2UTF8(imageType).c_str(), 
            clipX, 
            clipY, 
            clipWidth, 
            clipHeight, 
            m_SessionID.c_str()
        )
    );
    if (!result) {
        return false;
    }
    message = _Wait(m_ID);
    if (message.empty() || message.contains("error")) {
        return false;
    }
    std::string data = message["result"]["data"].get<std::string>();

    // 파일로 저장
    _SaveFile(resultFilePath, data);

    return true;
}
        
bool CDPManager::PrintToPDF(
    const std::wstring& resultFilePath,
    double margin /*= 0.4F*/,
    bool landscape /*= false*/
)
{
    auto boolToString = [](bool value) -> std::string {
        return value ? "true" : "false";
    };

    // PDF로 인쇄
    // A4 사이즈 (210 x 297 mm)
    const double PAPER_WIDTH = 8.27F;
    const double PAPER_HEIGHT = 11.7F;
    bool result = m_Pipe->Write(
        _Format(
            Page_printToPDF, 
            ++m_ID, 
            boolToString(landscape).c_str(), 
            PAPER_WIDTH,
            PAPER_HEIGHT,
            margin,
            margin,
            margin,
            margin,
            m_SessionID.c_str()
        )
    );
    if (!result) {
        return false;
    }

    nlohmann::json message = _Wait(m_ID);
    if (message.empty() || message.contains("error")) {
        return false;
    }
    std::string data = message["result"]["data"].get<std::string>();

    // 파일로 저장
    _SaveFile(resultFilePath, data);

    return true;
}

void CDPManager::_SaveFile(const std::wstring& resultFilePath, const std::string& base64Str)
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
                    char_array_4[i] = static_cast<unsigned char>(base64_chars.find(char_array_4[i]));
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
                char_array_4[j] = static_cast<unsigned char>(base64_chars.find(char_array_4[j]));
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
    std::ofstream file(_W2UTF8(resultFilePath), std::ios::binary);
    file.write(reinterpret_cast<const char*>(decodedData.data()), decodedData.size());
    file.close();
}
// --------------------------------------------------------------------------------
// End of CDPManager class
// --------------------------------------------------------------------------------

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
    CDPManager cdpManager;
    cdpManager.Launch();
    cdpManager.SetTimeout(5);
    cdpManager.Navegate(htmlURL, std::make_pair(viewportWidth, vieweportHeight));
    cdpManager.Screenshot(resultFilePath, imageType, std::make_pair(clipX, clipY), std::make_pair(clipWidth, clipHeight));

    return true;
}

bool ConvertHtmlModule::HtmlToPdf(
    const wchar_t* htmlURL,
    const wchar_t* resultFilePath,
    const wchar_t* margin,
    int isLandScape
) {
    CDPManager cdpManager;
    cdpManager.Launch();
    cdpManager.SetTimeout(5);
    cdpManager.Navegate(htmlURL, std::make_pair(-1, -1));

    double marginValue = 0.4F;
    if (margin != nullptr) {
        marginValue = std::stod(margin);
    }
    bool landscape = (isLandScape == 1) ? true : false;
    cdpManager.PrintToPDF(resultFilePath, marginValue, landscape);

    return true;
}
// --------------------------------------------------------------------------------
// End of ConvertHtmlModule class
// --------------------------------------------------------------------------------
