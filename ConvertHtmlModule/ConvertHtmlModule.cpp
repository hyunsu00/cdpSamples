#include "ConvertHtmlModule.h"
#include <unistd.h> // pipe, fork

class CDP 
{
public:
    CDP();
    ~CDP();
public:
    bool launch();

private:
    int m_readFD; // 첫 번째 파이프: fd 3 (읽기)
    int m_writeFD; // 두 번째 파이프: fd 4 (쓰기)
    pid_t m_PID;
}; // class CDP

CDP::CDP()
{
}

CDP::~CDP()
{
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
            // perror("dup2 fd1");
            return 1;
        }

        // fd4 쓰기 4로 복제
        fd = dup2(fd4[1], 4);
        if (fd == -1) {
            // perror("dup2 fd2");
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
        // perror("execlp");
        return 1;
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