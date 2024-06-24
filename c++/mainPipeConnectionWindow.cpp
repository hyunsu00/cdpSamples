#include <windows.h>
#include <stdio.h>

HANDLE hStdOutWr = NULL;
HANDLE hStdOutRd = NULL;
HANDLE hStdInWr = NULL;
HANDLE hStdInRd = NULL;
HANDLE hCDPOutWr = NULL;
HANDLE hCDPOutRd = NULL;
HANDLE hCDPInWr = NULL;
HANDLE hCDPInRd = NULL;
HANDLE hProcess = NULL;

#pragma pack(push, 1)  // 현재 패킹 설정을 스택에 저장하고, 패킹을 1바이트로 변경
typedef struct {
    INT     number_of_fds;
    BYTE    crt_flags[5];
    HANDLE  os_handle[5];
} STDIO_BUFFER;

typedef struct {
    INT number_of_fds;
    BYTE raw_bytes[25];
} STDIO_BUFFER2;
#pragma pack(pop)  // 패킹 설정을 이전 상태(기본값)로 복원

int init(wchar_t* pwszCmdline, int aboolSerializable) {
    // Set up the security attributes struct.
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    // Create stdout and stderr pipe.
    if (!CreatePipe(&hStdOutRd, &hStdOutWr, &sa, 0)) {
        return -2;
    }

    // Create stdin pipe.
    if (!CreatePipe(&hStdInRd, &hStdInWr, &sa, 0)) {
        return -2;
    }

    // Create CDP output pipe.
    if (!CreatePipe(&hCDPOutRd, &hCDPOutWr, &sa, 1024 * 1024)) {
        return -2;
    }

    // Create CDP input pipe.
    if (!CreatePipe(&hCDPInRd, &hCDPInWr, &sa, 0)) {
        return -2;
    }

    // Fill the special structure for passing arbitrary pipes (i.e. fds) to a process
    STDIO_BUFFER pipes;
    pipes.number_of_fds = 5;
    pipes.os_handle[0] = NULL;
    pipes.os_handle[1] = NULL;
    pipes.os_handle[2] = NULL;
    pipes.os_handle[3] = hCDPInRd;
    pipes.os_handle[4] = hCDPOutWr;

    pipes.crt_flags[0] = 0x41;
    pipes.crt_flags[1] = 0x41;
    pipes.crt_flags[2] = 0x41;
    pipes.crt_flags[3] = 0x09;
    pipes.crt_flags[4] = 0x09;

    STDIO_BUFFER2 pipes2;
    pipes2.number_of_fds = pipes.number_of_fds;

    // Copy data to raw_bytes
    memcpy(pipes2.raw_bytes, &pipes.number_of_fds, sizeof(pipes.number_of_fds));
    memcpy(pipes2.raw_bytes + 5, &pipes.os_handle[0], sizeof(pipes.os_handle));

    // Set up the start up info struct.
    STARTUPINFOW si = {0, };
    si.cb = sizeof(STARTUPINFOW);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdOutput = hStdOutWr;
    si.hStdInput = hStdInRd;
    si.hStdError = hStdOutWr;
    si.wShowWindow = SW_SHOWDEFAULT;
    si.cbReserved2 = sizeof(pipes);
    si.lpReserved2 = (LPBYTE)&pipes;

    // Create the child process.
    PROCESS_INFORMATION pi = {0, };
    if (!CreateProcessW(NULL,   // No module name (use command line)
        pwszCmdline,            // Command line
        &sa,                    // Process handle not inheritable
        &sa,                    // Thread handle not inheritable
        TRUE,                   // Set handle inheritance to TRUE
        NORMAL_PRIORITY_CLASS,  // No creation flags
        NULL,                   // Use parent's environment block
        NULL,                   // Use parent's starting directory
        &si,                    // Pointer to STARTUPINFO structure 
        &pi)                    // Pointer to PROCESS_INFORMATION structure
    ) {
        return -1;
    }

    // Wait until child process exits.
    // WaitForSingleObject(pi.hProcess, INFINITE);

    // Close handles that are no longer needed.
    CloseHandle(hStdOutWr);
    CloseHandle(hStdInRd);
    CloseHandle(hCDPOutWr);
    CloseHandle(hCDPInRd);

    // Store process handle.
    hProcess = pi.hProcess;

    return 0;
}

int start_chrome() {
    STARTUPINFOW si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    // Command line for Chrome
    wchar_t szCmdline[] = L"C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe";

    // Start the child process.
    if (!CreateProcessW(NULL,   // No module name (use command line)
        szCmdline,        // Command line
        NULL,             // Process handle not inheritable
        NULL,             // Thread handle not inheritable
        FALSE,            // Set handle inheritance to FALSE
        0,                // No creation flags
        NULL,             // Use parent's environment block
        NULL,             // Use parent's starting directory 
        &si,              // Pointer to STARTUPINFO structure
        &pi)              // Pointer to PROCESS_INFORMATION structure
    ) {
        return -1;
    }

    // Wait until child process exits.
    WaitForSingleObject(pi.hProcess, INFINITE);

    // Close process and thread handles.
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return 0;
}

void cleanup() {
/*
    CloseHandle(hStdOutRd);
    CloseHandle(hStdOutWr);
    CloseHandle(hStdInRd);
    CloseHandle(hStdInWr);

    CloseHandle(hCDPOutRd);
    CloseHandle(hCDPOutWr);
    CloseHandle(hCDPInRd);
    CloseHandle(hCDPInWr);

    CloseHandle(hProcess);
*/
}

int main() {
# if 1
    // Initialize pipes and start the Chromium process
    wchar_t szCmdline[] = L"C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe --remote-debugging-pipe --enable-automation --enable-logging";
    int result = init(szCmdline, 0);  // replace with the actual path to Chromium
    if (result != 0) {
        fprintf(stderr, "Initialization failed with error code %d\n", result);
        return 1;
    }  

    // Example: Send a command to the Chromium process
    const char* request = R"({ "id": 1, "method": "Target.createTarget", "params": { "url": "https://www.naver.com" }})";
    const char Null = '\0';
    DWORD written;
    if (!WriteFile(hCDPInWr, request, (DWORD)strlen(request), &written, NULL)) {
        fprintf(stderr, "Failed to write to pipe\n");
        cleanup();
        return 1;
    }
    if (!WriteFile(hCDPInWr, &Null, 1, &written, NULL)) {
        fprintf(stderr, "Failed to write to pipe\n");
        cleanup();
        return 1;
    }

    // Read output from the Chromium process
/*
    char buffer[1024] = { 0, };
    DWORD read;
    if (!ReadFile(hCDPOutRd, buffer, sizeof(buffer) - 1, &read, NULL) || read == 0) {
        fprintf(stderr, "Failed to read from pipe\n");
        cleanup();
        return 1;
    }
    buffer[read] = '\0';
    printf("Output from process: %s\n", buffer);
*/
    // Cleanup handles
    cleanup();
#else
    start_chrome();
#endif
    return 0;
}