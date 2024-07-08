#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <fcntl.h>

int main() {
    pid_t pid;
    int fd3[2];  // 첫 번째 파이프: fd 3 (읽기)
    int fd4[2];  // 두 번째 파이프: fd 4 (쓰기)
    // fd3 파이프 생성 (fd 3용, 읽기 / 쓰기)
    if (pipe(fd3) == -1) {
        perror("pipe 1");
        return 1;
    }
    // fd3 파이프 생성 (fd 3용, 읽기 / 쓰기)
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
        // 프로그램과 인자 실행
        // const char* args[] = {
        //     "/home/hyunsu00/dev/chromium/src/out/Debug/chrome",
        //     "--enable-features=UseOzonePlatform",
        //     "--ozone-platform=wayland",
        //     "--enable-logging",
        //     "--v=1",
        //     "--disable-gpu",
        //     "--remote-debugging-pip",
        //     NULL
        // };
        // execvp("/home/hyunsu00/dev/chromium/src/out/Debug/chrome", (char* const*)args);
        execlp("/opt/google/chrome/chrome", "/opt/google/chrome/chrome", "--enable-features=UseOzonePlatform", "--ozone-platform=wayland", "--remote-debugging-pipe", NULL);
        // execl("/home/hyunsu00/dev/chromium/src/out/Debug/chrome", "/home/hyunsu00/dev/chromium/src/out/Debug/chrome", "--enable-features=UseOzonePlatform", "--ozone-platform=wayland", "--remote-debugging-pipe", NULL);
      
        // execlp 실패 시
        perror("execlp");
        return 1;
    } else {
        // 부모 프로세스
        close(fd3[0]); // fd3 읽기 닫기
        close(fd4[1]); // fd4 쓰기 닫기

        const char* request = R"({ "id": 1, "method": "Target.createTarget", "params": { "url": "https://www.naver.com" }})";
        write(fd3[1], request, strlen(request));
        const char Null = '\0';
        write(fd3[1], &Null, 1);

        const size_t BUF_SIZE = 1024;
        char buf[BUF_SIZE];
        int num_bytes = 0;
        while ((num_bytes = read(fd4[0], buf, BUF_SIZE)) > 0) {
            buf[num_bytes] = '\0'; // 문자열 끝을 표시하기 위해 널 문자 추가
            printf("부모 프로세스가 받은 데이터: %s\n", buf);
        }

        // fd3 쓰기 닫기
        close(fd3[1]);
        // fd4 읽기 닫기
        close(fd4[0]);

        // 자식 프로세스가 종료될 때까지 기다림
        waitpid(pid, NULL, 0);
    }

    return 0;
}