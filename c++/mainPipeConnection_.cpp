#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <vector>
#include <string.h>
#include <fcntl.h>

int main() {
    pid_t pid;
    int fd1[2];  // 첫 번째 파이프: fd 3 (읽기)
    int fd2[2];  // 두 번째 파이프: fd 4 (쓰기)
    int fd3[2];  // 첫 번째 파이프: fd 3 (읽기)
    // 첫 번째 파이프 생성 (fd 3용, 읽기)
    if (pipe(fd3) == -1) {
        perror("pipe 1");
        return 1;
    }
    // 첫 번째 파이프 생성 (fd 3용, 읽기)
    if (pipe(fd1) == -1) {
        perror("pipe 1");
        return 1;
    }
    // 두 번째 파이프 생성 (fd 4용, 쓰기)
    if (pipe(fd2) == -1) {
        perror("pipe 2");
        return 1;
    }
    close(fd3[0]);
    close(fd3[1]);
    printf("fd1[0]: %d, fd1[1]: %d, fd2[0]: %d, fd2[1]: %d\n", fd1[0], fd1[1], fd2[0], fd2[1]);

    // 자식 프로세스 생성
    pid = fork();

    if (pid == -1) {
        perror("fork");
        return 1;
    } else if (pid == 0) {
        // 자식 프로세스
        close(fd1[1]); // 첫 번째 파이프의 쓰기 닫기 (fd 3 읽기)
        close(fd2[0]); // 두 번째 파이프의 읽기 닫기 (fd 4 쓰기)

        // fd 3을 첫 번째 파이프의 읽기로 복제
        int fd = dup2(fd1[0], 3);
        if (fd == -1) {
            perror("dup2 fd1");
            return 1;
        }

        // fd 4를 두 번째 파이프의 쓰기로 복제
        fd = dup2(fd2[1], 4);
        if (fd == -1) {
            perror("dup2 fd2");
            return 1;
        }

        // 원본 파이프의 읽기 닫기 및 필요 없는 파일 디스크립터 닫기
        close(fd1[0]);
        close(fd2[1]);

        // 프로그램과 인자 실행
        // const char* args[] = {
        //     "/usr/bin/chrome",
        //     "--enable-features=UseOzonePlatform",
        //     "--ozone-platform=wayland",
        //     "--enable-logging",
        //     "--v=1",
        //     "--disable-gpu",
        //     "--remote-debugging-pip",
        //     NULL
        // };
        // execvp("/usr/bin/chrome", (char* const*)args);
        execl("/usr/bin/chrome", "/usr/bin/chrome", "--enable-features=UseOzonePlatform", "--ozone-platform=wayland", "--remote-debugging-pipe", NULL);

        // execlp 실패 시
        perror("execlp");
        return 1;

    } else {
        // 부모 프로세스
        // close(fd1[0]); // 첫 번째 파이프의 읽기 닫기 (fd 3 읽기)
        // close(fd2[1]); // 두 번째 파이프의 쓰기 닫기 (fd 4 쓰기)

        const char* request = R"({ "id": 1, "method": "Target.createTarget", "params": { "url": "https://www.naver.com" })";
        write(fd1[1], request, strlen(request));
        const char* Null = R"(\0)";
        write(fd1[1], Null, 1);

        // const size_t BUF_SIZE = 1024;
        // char buf[BUF_SIZE];
        // int num_bytes = 0;
        // while ((num_bytes = read(fd2[0], buf, BUF_SIZE)) > 0) {
        //     buf[num_bytes] = '\0'; // 문자열 끝을 표시하기 위해 널 문자 추가
        //     printf("부모 프로세스가 받은 데이터: %s\n", buf);
        // }

        // close(fd1[1]); // 첫 번째 파이프의 쓰기 닫기 (fd 3 읽기)
        // close(fd2[0]); // 두 번째 파이프의 읽기 닫기 (fd 4 쓰기)

        // 자식 프로세스가 종료될 때까지 기다림
        waitpid(pid, NULL, 0);
    }

    return 0;
}