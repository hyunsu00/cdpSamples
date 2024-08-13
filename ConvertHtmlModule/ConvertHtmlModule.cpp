#include "stdafx.h"
#include "ConvertHtmlModule.h"

#ifdef _WIN32
#   include <windows.h> // CreateProcessW, WaitForSingleObject, CloseHandle, GetModuleFileNameW, ...
#   include <shlwapi.h> // PathFileExistsW, PathRemoveFileSpecW
#	include <crtdbg.h> // _ASSERTE
#   pragma comment(lib, "Shlwapi.lib") // PathFileExistsW, PathRemoveFileSpecW
#else
#   include <unistd.h> // pipe, fork
#   include <libgen.h> // dirname
#   include <sys/wait.h> // waitpid
#	include <assert.h> // assert
#   include <limits.h> // PATH_MAX
#   ifndef _ASSERTE
#	    define _ASSERTE assert
#   endif
#   ifndef INFINITE
#       define INFINITE 0xFFFFFFFF  // Infinite timeout
#   endif
#endif
#include <cstdint> // uint32_t
#include <cmath> // std::round
#include <cstdio> // perror
#include <cstdlib> // exit
#include <iostream> // std::cerr
#include <string> // std::string, std::wstring
#include <vector> // std::vector
#include <queue> // std::queue
#include <algorithm> // std::find
#include <fstream> // std::ofstream
#include "nlohmann/json.hpp" // nlohmann::json
#include <codecvt> // std::codecvt_utf8
#include <locale> // std::wstring_convert

struct Converter 
{
    static std::string W2UTF8(const std::wstring& wstr) {
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
        return converter.to_bytes(wstr);
    }
}; // struct Converter

struct CDPPipe
{
    CDPPipe() {}
    virtual ~CDPPipe() {}

    virtual bool Launch() = 0;
    virtual void Exit() = 0;
    virtual void SetTimeout(uint32_t milliseconds) = 0;
    virtual bool Write(const std::string& command) = 0;
    virtual bool Read(std::string& message) = 0;
    virtual void SetChromePath(const std::wstring& chromePath) = 0;
    virtual const wchar_t* GetChromePath() const = 0;
}; // struct CDPPipe

#ifdef _WIN32
class CDPPipe_Windows : public CDPPipe
{
public:
    CDPPipe_Windows();
    CDPPipe_Windows(const std::wstring& chromePath);
    virtual ~CDPPipe_Windows();

public:
    // 복사 생성자 / 대입연산자 / 이동생성자/이동대입연산자는 막는다.
    CDPPipe_Windows(const CDPPipe_Windows&) = delete;
    CDPPipe_Windows& operator=(const CDPPipe_Windows&) = delete;
    CDPPipe_Windows(CDPPipe_Windows&&) = delete;
    CDPPipe_Windows& operator=(CDPPipe_Windows&&) = delete;

public:
    // 비동기(중첩된) I/O를 가능한 파이프 생성
    // WIN32 익명 파이프는 비동기(중첩된) I/O를 지원하지 않음 
    static BOOL CreatePipe(
        PHANDLE hReadPipe, 
        PHANDLE hWritePipe, 
        LPSECURITY_ATTRIBUTES lpPipeAttributes, 
        DWORD nSize
    );

public:
    virtual bool Launch() override;
    virtual void Exit() override;
    virtual void SetTimeout(uint32_t milliseconds) override;
    virtual bool Write(const std::string& command) override;
    virtual bool Read(std::string& message) override;
    virtual void SetChromePath(const std::wstring& chromePath) override;
    virtual const wchar_t* GetChromePath() const override;

private:
    HANDLE m_hProcess;
    HANDLE m_hWrite; // 두 번째 파이프: fd 4 (쓰기)
    HANDLE m_hRead; // 첫 번째 파이프: fd 3 (읽기)
    std::queue<std::string> m_MessageQueue; // m_ReadFD로부터 읽은 메시지 큐
    DWORD m_dwTimeout; // 타임아웃설정(초), INFINITE이면 타임아웃 없음
    std::wstring m_ChromePath; // 크롬실행파일 경로
}; // class CDPPipe_Windows

CDPPipe_Windows::CDPPipe_Windows()
: m_hProcess(INVALID_HANDLE_VALUE)
, m_hWrite(INVALID_HANDLE_VALUE)
, m_hRead(INVALID_HANDLE_VALUE)
, m_MessageQueue()
, m_dwTimeout(INFINITE)
, m_ChromePath()
{
}

CDPPipe_Windows::CDPPipe_Windows(
    const std::wstring& chromePath
)
: m_hProcess(INVALID_HANDLE_VALUE)
, m_hWrite(INVALID_HANDLE_VALUE)
, m_hRead(INVALID_HANDLE_VALUE)
, m_MessageQueue()
, m_dwTimeout(INFINITE)
, m_ChromePath(chromePath)
{
}

CDPPipe_Windows::~CDPPipe_Windows()
{
    Exit();
}

BOOL CDPPipe_Windows::CreatePipe(
    PHANDLE lpReadPipe, 
    PHANDLE lpWritePipe, 
    LPSECURITY_ATTRIBUTES lpPipeAttributes, 
    DWORD nSize
)
{
    if (nSize == 0) {
        nSize = 4096;
    }

    static LONG lPipeSerialNumber = 0;
    WCHAR wszPipeName[64];
    swprintf_s(
        wszPipeName, 
        _countof(wszPipeName), 
        L"\\\\.\\Pipe\\RemoteExeAnon.%08x.%08x", 
        ::GetCurrentProcessId(), InterlockedIncrement(&lPipeSerialNumber)
    );

    HANDLE hReadPipe = ::CreateNamedPipeW(
        wszPipeName,
        PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED, // 파이프를 읽기 전용, 비동기(중첩된) I/O를 가능
        PIPE_TYPE_BYTE | PIPE_WAIT, // 바이트 스트림 모드로 설정, 읽기 작업이 완료될 때까지 대기
        1, // 파이프의 최대 인스턴스 
        nSize, // 파이프의 출력 버퍼 크기를 바이트 단위
        nSize, // 파이프의 입력 버퍼 크기를 바이트 단위
        INFINITE, // 파이프의 기본 타임아웃 시간 (무한대기)
        lpPipeAttributes // 보안 특성
    );

    if (INVALID_HANDLE_VALUE == hReadPipe) {
        DWORD dwError = ::GetLastError();
        ::SetLastError(dwError);
        return FALSE;
    }

    HANDLE hWritePipe = ::CreateFileW(
        wszPipeName,
        GENERIC_WRITE, // 쓰기 접근 권한을 지정
        0, // 공유를 허용하지 않음 (다른 프로세스가 이 파이프에 접근할 수 없음)
        lpPipeAttributes, // 보안 특성
        OPEN_EXISTING, // 이미 존재하는 파이프를 열기
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, // 일반 속성을 지정, 비동기(중첩된) I/O를 가능
        NULL // 템플릿 파일 핸들 사용 안함
    );

    if (INVALID_HANDLE_VALUE == hWritePipe) {
        DWORD dwError = ::GetLastError();
        ::CloseHandle(hReadPipe);
        ::SetLastError(dwError);
        return FALSE;
    }

    *lpReadPipe = hReadPipe;
    *lpWritePipe = hWritePipe;
    return TRUE;
}

bool CDPPipe_Windows::Launch() /*override*/
{
    HANDLE fd3[2] = { NULL, NULL }; // 첫 번째 파이프: fd 3 (읽기)
    HANDLE fd4[2] = { NULL, NULL }; // 두 번째 파이프: fd 4 (쓰기)

    // Set up the security attributes struct.
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    // fd3 파이프 생성 (fd 3용, 읽기 / 쓰기)
    if (!CDPPipe_Windows::CreatePipe(&fd3[0], &fd3[1], &sa, 0)) {
        return false;
    }

    // fd4 파이프 생성 (fd 4용, 읽기 / 쓰기)
    if (!CDPPipe_Windows::CreatePipe(&fd4[0], &fd4[1], &sa, 0)) {
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
    // wchar_t wszCommandLine[] = L".\\chrome\\chrome-headless-shell-win32\\chrome-headless-shell.exe --no-sandbox --disable-gpu --remote-debugging-pipe";
    std::wstring applicationName = GetChromePath();
    if (::PathFileExitstW(applicationName.c_str()) == FALSE) {
        wchar_t szAppPath[MAX_PATH] = { 0, };
        ::GetModuleFileNameW(NULL, szAppPath, MAX_PATH);
        ::PathRemoveFileSpecW(szAppPath);
        applicationName = hncstd::wstring(szAppPath) + L"\\" + applicationName;
    }
    std::wstring args = L"--no-sandbox --disable-gpu --remote-debugging-pipe"; // 실행 인자를 설정
    std::wstring commandLine = applicationName + L" " + args;
    wchar_t wszCommandLine[MAX_PATH] = {0, };
    wcscpy_s(wszCommandLine, commandLine.c_str());

    PROCESS_INFORMATION pi = {0, };
    if (!::CreateProcessW(
        applicationName.c_str(), // No module name (use command line)
        wszCommandLine,          // Command line
        &sa,                     // Process handle not inheritable
        &sa,                     // Thread handle not inheritable
        TRUE,                    // Set handle inheritance to TRUE
        NORMAL_PRIORITY_CLASS,   // No creation flags
        NULL,                    // Use parent's environment block
        NULL,                    // Use parent's starting directory
        &si,                     // Pointer to STARTUPINFO structure 
        &pi)                     // Pointer to PROCESS_INFORMATION structure
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

void CDPPipe_Windows::Exit() /*override*/
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

void CDPPipe_Windows::SetTimeout(uint32_t milliseconds) /*override*/
{
    m_dwTimeout = milliseconds;
}

bool CDPPipe_Windows::Write(const std::string& command) /*override*/
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

bool CDPPipe_Windows::Read(std::string& message) /*override*/
{
    if (m_MessageQueue.size() > 0) {
        message = m_MessageQueue.front();
        m_MessageQueue.pop();
        return true;
    }

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

    _ASSERTE(byteBuf.size() > 0 && "No data read from the pipe");
    if (byteBuf.size() == 0) {
        return false;
    }

    auto it = std::find(byteBuf.begin(), byteBuf.end(), '\0');
    while (it != byteBuf.end()) {
        std::string str(byteBuf.begin(), it);
        m_MessageQueue.push(str);
        byteBuf.erase(byteBuf.begin(), it + 1);
        it = std::find(byteBuf.begin(), byteBuf.end(), '\0');
#if defined(DEBUG) || defined(_DEBUG)
    nlohmann::json jmessage = nlohmann::json::parse(str.c_str());
    std::cout << "[CDPPipe_Windows::Read()] : " << jmessage.dump(4) << std::endl;
#endif // #if defined(DEBUG) || defined(_DEBUG)
    }

    message = m_MessageQueue.front();
    m_MessageQueue.pop();

    return true;
}

void CDPPipe_Windows::SetChromePath(const std::wstring& chromePath) /*override*/
{
    m_ChromePath = chromePath;
}

const wchar_t* CDPPipe_Windows::GetChromePath() const /*override*/
{
    return m_ChromePath.c_str();
}
// --------------------------------------------------------------------------------
// End of CDPPipe_Windows class
// --------------------------------------------------------------------------------

#else // #ifdef _WIN32

class CDPPipe_Linux : public CDPPipe
{
public:
    CDPPipe_Linux();
    CDPPipe_Linux(const std::wstring& chromePath);
    virtual ~CDPPipe_Linux();

public:
    // 복사 생성자 / 대입연산자 / 이동생성자/이동대입연산자는 막는다.
    CDPPipe_Linux(const CDPPipe_Linux&) = delete;
    CDPPipe_Linux& operator=(const CDPPipe_Linux&) = delete;
    CDPPipe_Linux(CDPPipe_Linux&&) = delete;
    CDPPipe_Linux& operator=(CDPPipe_Linux&&) = delete;

public:
    virtual bool Launch() override;
    virtual void Exit() override;
    virtual void SetTimeout(uint32_t milliseconds) override;
    virtual bool Write(const std::string& command) override;
    virtual bool Read(std::string& message) override;
    virtual void SetChromePath(const std::wstring& chromePath) override;
    virtual const wchar_t* GetChromePath() const override;

private:
    pid_t m_PID;
    int m_WriteFD; // 두 번째 파이프: fd 4 (쓰기)
    int m_ReadFD; // 첫 번째 파이프: fd 3 (읽기)
    std::queue<std::string> m_MessageQueue; // m_ReadFD로부터 읽은 메시지 큐
    std::unique_ptr<timeval> m_Timeout; // 타임아웃설정(초), nullptr이면 타임아웃 없음
    std::wstring m_ChromePath; // 크롬실행파일 경로
}; // class CDPPipe_Linux

CDPPipe_Linux::CDPPipe_Linux()
: m_PID(-1)
, m_WriteFD(-1)
, m_ReadFD(-1)
, m_MessageQueue()
, m_Timeout()
, m_ChromePath()
{
}

CDPPipe_Linux::CDPPipe_Linux(
    const hncstd::wstring& chromePath
)
: m_PID(-1)
, m_WriteFD(-1)
, m_ReadFD(-1)
, m_MessageQueue()
, m_Timeout()
, m_ChromePath(chromePath)
{
}   

CDPPipe_Linux::~CDPPipe_Linux()
{
    Exit();
}

bool CDPPipe_Linux::Launch() /*override*/
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
            exit(EXIT_FAILURE);
        }

        // fd4 쓰기 4로 복제
        fd = dup2(fd4[1], 4);
        if (fd == -1) {
            perror("Error dup2(fd4[1], 4)");
            exit(EXIT_FAILURE);
        }

        // 원본 파이프의 읽기 닫기 및 필요 없는 파일 디스크립터 닫기
        if (fd3[0] != 3) { // 3일경우 이미 닫혔음
            close(fd3[0]);
        }
        if (fd3[1] != 4) { // 4일경우 이미 닫혔음
            close(fd4[1]);
        }

        std::string chromePath = Converter::W2UTF8(GetChromePath());
        if (access(chromePath.c_str(), F_OK) == -1) {
            char szAppPath[PATH_MAX] = { 0, };
            ssize_t count = readlink("/proc/self/exe", szAppPath, PATH_MAX);
            if (count == -1) {
                perror("Error readlink()");
                exit(EXIT_FAILURE);
            }
            const char* exePath = dirname(szAppPath);
            chromePath = hncstd::string(exePath) + "/" + chromePath;
        }
        // int ret = execlp("/opt/google/chrome/chrome", "/opt/google/chrome/chrome", "--enable-features=UseOzonePlatform", "--ozone-platform=wayland", "--no-sandbox", "--disable-gpu", "--remote-debugging-pipe", NULL);
        // int ret = execlp("/opt/google/chrome/chrome", "/opt/google/chrome/chrome", "--enable-features=UseOzonePlatform", "--ozone-platform=wayland", "--no-sandbox", "--disable-gpu", "--remote-debugging-pipe", "--headless", NULL);
        // int ret = execlp("./chrome/chrome-headless-shell-linux64/chrome-headless-shell", "./chrome/chrome-headless-shell-linux64/chrome-headless-shell", "--no-sandbox", "--disable-gpu", "--remote-debugging-pipe", NULL);
        // int ret = execl("/home/hyunsu00/dev/chromium/src/out/Debug/chrome", "/home/hyunsu00/dev/chromium/src/out/Debug/chrome", "--enable-features=UseOzonePlatform", "--ozone-platform=wayland", "--no-sandbox", "--disable-gpu", "--remote-debugging-pipe", NULL);
        int ret = execlp(chromePath.c_str(), chromePath.c_str(), "--no-sandbox", "--disable-gpu", "--remote-debugging-pipe", NULL);
        if (ret == -1) {
            perror("Error execlp()");
            exit(EXIT_FAILURE);
        }
    } else {
        // 부모 프로세스
        close(fd3[0]); // fd3 읽기 닫기
        close(fd4[1]); // fd4 쓰기 닫기

        int status;
        if (waitpid(pid, &status, WNOHANG) == 0) { // 자식 프로세스가 아직 종료되지 않음
            m_PID = pid;
            m_WriteFD = fd3[1];
            m_ReadFD = fd4[0];
            return true; // 성공 반환
        }

        // 자식프로세스가 정상적으로 종료시 종료 상태코드가 0이 아닌 경우 오류로 간주
        if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
            std::cerr << "자식 프로세스에서 오류 발생: " << WEXITSTATUS(status) << std::endl;
            close(fd3[1]); // fd3 쓰기 닫기
            close(fd4[0]); // fd4 읽기 닫기
            return false;
        }
    }

    return true;
}

void CDPPipe_Linux::Exit() /*override*/
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

void CDPPipe_Linux::SetTimeout(uint32_t milliseconds) /*override*/
{
    if (INFINITE == milliseconds) {
        m_Timeout.reset(nullptr);
    } else {
        m_Timeout.reset(new timeval());
        m_Timeout->tv_sec = std::round(static_cast<double>(milliseconds) / 1000);
        m_Timeout->tv_usec = 0;
    }
}

bool CDPPipe_Linux::Write(const std::string& command) /*override*/
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

bool CDPPipe_Linux::Read(std::string& message) /*override*/
{
    if (m_MessageQueue.size() > 0) {
        message = m_MessageQueue.front();
        m_MessageQueue.pop();
        return true;
    }

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
    
    _ASSERTE(byteBuf.size() > 0 && "No data read from the pipe");
    if (byteBuf.size() == 0) {
        return false;
    }

    auto it = std::find(byteBuf.begin(), byteBuf.end(), '\0');
    while (it != byteBuf.end()) {
        std::string str(byteBuf.begin(), it);
        m_MessageQueue.push(str);
        byteBuf.erase(byteBuf.begin(), it + 1);
        it = std::find(byteBuf.begin(), byteBuf.end(), '\0');
#if defined(DEBUG) || defined(_DEBUG)
        nlohmann::json jmessage = nlohmann::json::parse(str.c_str());
        std::cout << "[CDPPipe_Linux::Read()] : " << jmessage.dump(4) << std::endl;
#endif // #if defined(DEBUG) || defined(_DEBUG)
    }

    message = m_MessageQueue.front();
    m_MessageQueue.pop();
    
    return true;
}

void CDPPipe_Linux::SetChromePath(const std::wstring& chromePath) /*override*/
{
    m_ChromePath = chromePath;
}

const wchar_t* CDPPipe_Linux::GetChromePath() const /*override*/
{
    return m_ChromePath.c_str();
}
// --------------------------------------------------------------------------------
// End of CDPPipe_Linux class
// --------------------------------------------------------------------------------
#endif // !#ifdef _WIN32

class CDPManager
{
public:
    static std::wstring GetChromePath() {
        return s_ChromePath;
    }
    static void SetChromePath(const std::wstring& chromePath) {
        s_ChromePath = chromePath;
    }
public:
    CDPManager();
    ~CDPManager();

public:
    // 복사 생성자 / 대입연산자 / 이동생성자/이동대입연산자는 막는다.
    CDPManager(const CDPManager&) = delete;
    CDPManager& operator=(const CDPManager&) = delete;
    CDPManager(CDPManager&&) = delete;
    CDPManager& operator=(CDPManager&&) = delete;

public:
    bool Launch();
    void Exit();
    void SetTimeout(uint32_t milliseconds = 5000);
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
    bool _Waits(
        const std::vector<std::string>& methods = {"Page.loadEventFired", "Network.loadingFinished"}
    );
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
    const std::string Network_enable;
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

private:
    static std::wstring s_ChromePath;
}; // class CDPManager

std::wstring CDPManager::s_ChromePath = hncstd::wstring();

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
, Network_enable(R"(
    { 
        "id": %d, 
        "method": "Network.enable",
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
#ifdef _WIN32
, m_Pipe(new CDPPipe_Windows(s_ChromePath))
#else // #ifdef _WIN32
, m_Pipe(new CDPPipe_Linux(s_ChromePath))
#endif // !#ifdef _WIN32
{
}

CDPManager::~CDPManager()
{
    Exit();
}

nlohmann::json CDPManager::_Wait(int id)
{
    std::string message;
    while (true) {
        if (!m_Pipe->Read(message)) {
            break;
        }

        nlohmann::json jmessage = nlohmann::json::parse(message);
        if (jmessage.contains("error")) {
            return jmessage;
        } else if (jmessage["id"] == id) {
            return jmessage;
        } else {
            continue;
        }
    }

    return nlohmann::json();
}

nlohmann::json CDPManager::_Wait(const std::string& method /*= "Page.loadEventFired"*/)
{
    std::string message;
    while (true) {
        if (!m_Pipe->Read(message)) {
            break;
        }

        nlohmann::json jmessage = nlohmann::json::parse(message);
        if (jmessage.contains("error")) {
            return jmessage;
        } else if (jmessage["method"] == method) {
            return jmessage;
        } else {
            continue;
        }
    }

    return nlohmann::json();
}

bool CDPManager::_Waits(
    const std::vector<std::string>& methods /*= {"Page.loadEventFired", "Network.loadingFinished"}*/
)
{
    std::vector<std::string> emethods(methods);
    std::string message;
    while (!emethods.empty()) {
        if (!m_Pipe->Read(message)) {
            break;
        }

        nlohmann::json jmessage = nlohmann::json::parse(message);
        if (jmessage.contains("error")) {
            return false;
        } else {
            auto it = std::find(emethods.begin(), emethods.end(), jmessage["method"]);
            if (it != emethods.end()) {
                emethods.erase(it);
            } else {
                continue;
            }
        }
    }

    return true;
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

void CDPManager::SetTimeout(uint32_t milliseconds /*= 5000*/)
{
    m_Pipe->SetTimeout(milliseconds);
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
    nlohmann::json jmessage = _Wait(m_ID);
    if (jmessage.empty() || jmessage.contains("error")) {
        return false;
    }
    std::string targetId = jmessage["result"]["targetId"].get<std::string>();

    // 탭 연결
    result = m_Pipe->Write(_Format(Target_attachToTarget, ++m_ID, targetId.c_str()));
    if (!result) {
        return false;
    }
    jmessage = _Wait(m_ID);
    if (jmessage.empty() || jmessage.contains("error")) {
        return false;
    }
    std::string sessionId = jmessage["result"]["sessionId"].get<std::string>();

    // 페이지 이벤트 활성화
    result = m_Pipe->Write(_Format(Page_enable, ++m_ID, sessionId.c_str()));
    if (!result) {
        return false;
    }
    // 네트워크 이벤트 활성화
    // result = m_Pipe->Write(_Format(Network_enable, ++m_ID, sessionId.c_str()));
    // if (!result) {
    //     return false;
    // }

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
    result = m_Pipe->Write(_Format(Page_navigate, ++m_ID, Converter::W2UTF8(url).c_str(), sessionId.c_str()));
    if (!result) {
        return false;
    }

    // "Page.loadEventFired" 이벤트 대기
    jmessage = _Wait("Page.loadEventFired");
    if (jmessage.empty() || jmessage.contains("error")) {
        return false;
    }
    // "Page.loadEventFired" && "Network.loadingFinished" 이벤트 대기 
    // result = _Waits({"Page.loadEventFired", "Network.loadingFinished"});
    // if (!result) {
    //     return false;
    // }

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

    nlohmann::json jmessage = _Wait(m_ID);
    if (jmessage.empty() || jmessage.contains("error")) {
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
    nlohmann::json jmessage = _Wait(m_ID);
    if (jmessage.empty() || jmessage.contains("error")) {
        return false;
    }

    // 스크린샷 캡처
    std::pair<int, int> contentSize = std::make_pair(
        jmessage["result"]["contentSize"]["width"].get<int>(),
        jmessage["result"]["contentSize"]["height"].get<int>()
    );
    int clipX = (clipPos.first == -1) ? 0 : clipPos.first;
    int clipY = (clipPos.second == -1) ? 0 : clipPos.second;
    int clipWidth = (clipSize.first == -1) ? contentSize.first : clipSize.first;
    int clipHeight = (clipSize.second == -1) ? contentSize.second : clipSize.second;
    result = m_Pipe->Write(
        _Format(
            Page_captureScreenshot, 
            ++m_ID, 
            Converter::W2UTF8(imageType).c_str(), 
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
    jmessage = _Wait(m_ID);
    if (jmessage.empty() || jmessage.contains("error")) {
        return false;
    }
    std::string data = jmessage["result"]["data"].get<std::string>();

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

    nlohmann::json jmessage = _Wait(m_ID);
    if (jmessage.empty() || jmessage.contains("error")) {
        return false;
    }
    std::string data = jmessage["result"]["data"].get<std::string>();

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
    std::ofstream file(Converter::W2UTF8(resultFilePath), std::ios::binary);
    file.write(reinterpret_cast<const char*>(decodedData.data()), decodedData.size());
    file.close();
}
// --------------------------------------------------------------------------------
// End of CDPManager class
// --------------------------------------------------------------------------------

std::wstring ConvertHtmlModule::GetChromePath()
{
    return CDPManager::GetChromePath();
}

void ConvertHtmlModule::SetChromePath(const std::wstring& chromePath)
{
    CDPManager::SetChromePath(chromePath);
}

bool ConvertHtmlModule::HtmlToImage(
    const std::wstring& htmlURL,
    const std::wstring& resultFilePath,
    const std::wstring& imageType,
    int clipX,
    int clipY,
    int clipWidth,
    int clipHeight,
    int viewportWidth,
    int vieweportHeight
) {
    CDPManager cdpManager;
    if (!cdpManager.Launch()) {
        return false;
    }
    cdpManager.SetTimeout(5000);
    if (!cdpManager.Navegate(htmlURL, std::make_pair(viewportWidth, vieweportHeight))) {
        return false;
    }
    if (!cdpManager.Screenshot(resultFilePath, imageType, std::make_pair(clipX, clipY), std::make_pair(clipWidth, clipHeight))) {
        return false;
    }

    return true;
}

bool ConvertHtmlModule::HtmlToPdf(
    const std::wstring& htmlURL,
    const std::wstring& resultFilePath,
    const std::wstring& margin,
    int isLandScape
) {
    CDPManager cdpManager;
    if (!cdpManager.Launch()) {
        return false;
    }
    cdpManager.SetTimeout(5000);
    if (!cdpManager.Navegate(htmlURL, std::make_pair(-1, -1))) {
        return false;
    }

    double marginValue = 0.4F;
    if (!margin.empty()) {
        marginValue = std::stod(Converter::W2UTF8(margin));
    }
    bool landscape = (isLandScape == 1) ? true : false;
    if (!cdpManager.PrintToPDF(resultFilePath, marginValue, landscape)) {
        return false;
    }

    return true;
}
// --------------------------------------------------------------------------------
// End of ConvertHtmlModule class
// --------------------------------------------------------------------------------