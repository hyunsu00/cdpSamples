#include <windows.h>
#include <stdio.h>

HANDLE hStdOutWr = NULL;
HANDLE hStdOutRd = NULL;
HANDLE hStdInWr = NULL;
HANDLE hStdInRd = NULL;
HANDLE hReadPipeFD3 = NULL;
HANDLE hWritePipeFD3 = NULL;
HANDLE hReadPipeFD4 = NULL;
HANDLE hWritePipeFD4 = NULL;
HANDLE hProcess = NULL;

typedef struct {
    int number_of_fds;
    int crt_flags[5];
    HANDLE os_handle[5];
    char raw_bytes[256];
} STDIO_BUFFER2;

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
    if (!CreatePipe(&hReadPipeFD3, &hWritePipeFD3, &sa, 2 * 1024 * 1024)) {
        return -2;
    }

    // Create CDP input pipe.
    if (!CreatePipe(&hReadPipeFD4, &hWritePipeFD4, &sa, 0)) {
        return -2;
    }

    // Fill the special structure for passing arbitrary pipes (i.e. fds) to a process
    STDIO_BUFFER2 pipes2;
    pipes2.number_of_fds = 5;
    pipes2.os_handle[0] = hStdInRd;
    pipes2.os_handle[1] = hStdOutWr;
    pipes2.os_handle[2] = hStdOutWr;
    pipes2.os_handle[3] = hReadPipeFD3;
    pipes2.os_handle[4] = hWritePipeFD4;
    pipes2.crt_flags[0] = 9;
    pipes2.crt_flags[1] = 9;
    pipes2.crt_flags[2] = 9;
    pipes2.crt_flags[3] = 9;
    pipes2.crt_flags[4] = 9;

    // Copy data to raw_bytes
    memcpy(pipes2.raw_bytes, &pipes2.number_of_fds, sizeof(pipes2.number_of_fds));
    memcpy(pipes2.raw_bytes + 4, pipes2.crt_flags, sizeof(pipes2.crt_flags));
    memcpy(pipes2.raw_bytes + 24, pipes2.os_handle, sizeof(pipes2.os_handle));

    // Set up the start up info struct.
    STARTUPINFOW si = {0, };
    si.cb = sizeof(STARTUPINFOW);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdOutput = hStdOutWr;
    si.hStdInput = hStdInRd;
    si.hStdError = hStdOutWr;
    si.wShowWindow = SW_SHOWDEFAULT;
    si.cbReserved2 = sizeof(pipes2);
    si.lpReserved2 = (LPBYTE)&pipes2;

    // Create the child process.
    PROCESS_INFORMATION pi = {0, };
    if (!CreateProcessW(NULL,   // No module name (use command line)
        pwszCmdline,            // Command line
        &sa,                    // Process handle not inheritable
        &sa,                    // Thread handle not inheritable
        TRUE,                   // Set handle inheritance to TRUE
        0,                      // No creation flags
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
    CloseHandle(hReadPipeFD3);
    CloseHandle(hWritePipeFD4);

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
    if (hProcess != NULL) {
        CloseHandle(hProcess);
    }
    if (hStdOutRd != NULL) {
        CloseHandle(hStdOutRd);
    }
    if (hStdInWr != NULL) {
        CloseHandle(hStdInWr);
    }
    if (hWritePipeFD3 != NULL) {
        CloseHandle(hWritePipeFD3);
    }
    if (hReadPipeFD4 != NULL) {
        CloseHandle(hReadPipeFD4);
    }
}

int main() {
# if 1
    // Initialize pipes and start the Chromium process
    wchar_t szCmdline[] = L"C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe --remote-debugging-pipe";
    int result = init(szCmdline, 0);  // replace with the actual path to Chromium
    if (result != 0) {
        fprintf(stderr, "Initialization failed with error code %d\n", result);
        return 1;
    }

    // Example: Send a command to the Chromium process
    const char* request = R"({ "id": 1, "method": "Target.createTarget", "params": { "url": "https://www.naver.com" }})";
    const char Null = '\0';
    DWORD written;
    if (!WriteFile(hWritePipeFD3, request, (DWORD)strlen(request), &written, NULL)) {
        fprintf(stderr, "Failed to write to pipe\n");
        cleanup();
        return 1;
    }
    if (!WriteFile(hWritePipeFD3, &Null, 1, &written, NULL)) {
        fprintf(stderr, "Failed to write to pipe\n");
        cleanup();
        return 1;
    }

    // Read output from the Chromium process
    char buffer[1024];
    DWORD read;
    if (!ReadFile(hReadPipeFD4, buffer, sizeof(buffer) - 1, &read, NULL) || read == 0) {
        fprintf(stderr, "Failed to read from pipe\n");
        cleanup();
        return 1;
    }
    buffer[read] = '\0';
    printf("Output from process: %s\n", buffer);

    // Cleanup handles
    cleanup();
#else
    start_chrome();
#endif
    return 0;
}