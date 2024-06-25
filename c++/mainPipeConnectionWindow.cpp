#include <windows.h>
#include <stdio.h>
#include <vector>

HANDLE fd3[2] = { NULL, NULL };
HANDLE fd4[2] = { NULL, NULL };
HANDLE ghProcess = NULL;

int init(wchar_t* pwszCmdline, int aboolSerializable) {
    // Set up the security attributes struct.
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    // Create CDP output pipe.
    if (!::CreatePipe(&fd3[0], &fd3[1], &sa, 0)) {
        return -2;
    }

    // Create CDP input pipe.
    if (!::CreatePipe(&fd4[0], &fd4[1], &sa, 0)) {
        return -2;
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
    PROCESS_INFORMATION pi = {0, };
    if (!::CreateProcessW(NULL,   // No module name (use command line)
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
    // ::WaitForSingleObject(pi.hProcess, INFINITE);

    // Close handles that are no longer needed.
    ::CloseHandle(fd3[0]);
    ::CloseHandle(fd4[1]);

    // Store process handle.
    ghProcess = pi.hProcess;

    return 0;
}

void cleanup() 
{
    ::CloseHandle(fd3[1]);
    ::CloseHandle(fd4[0]);
    ::CloseHandle(ghProcess);
}

int main() {
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
    if (!::WriteFile(fd3[1], request, (DWORD)strlen(request), &written, NULL)) {
        fprintf(stderr, "Failed to write to pipe\n");
        cleanup();
        return 1;
    }
    if (!::WriteFile(fd3[1], &Null, 1, &written, NULL)) {
        fprintf(stderr, "Failed to write to pipe\n");
        cleanup();
        return 1;
    }

    // Read output from the Chromium process
    char buffer[1024] = { 0, };
    DWORD read;
    if (!::ReadFile(fd4[0], buffer, sizeof(buffer) - 1, &read, NULL) || read == 0) {
        fprintf(stderr, "Failed to read from pipe\n");
        cleanup();
        return 1;
    }
    buffer[read] = '\0';
    printf("Output from process: %s\n", buffer);

    // Cleanup handles
    cleanup();
    return 0;
}