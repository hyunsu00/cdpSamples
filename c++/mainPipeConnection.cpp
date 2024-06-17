#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <vector>
#include <string.h>
#include <sys/socket.h>

int main() {
    pid_t pid;
    int fd1[2];  // 소켓 쌍을 저장할 배열
    int fd2[2];  // 소켓 쌍을 저장할 배열
    // 소켓 쌍 생성
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, fd1) == -1) {
        perror("socketpair failed");
        exit(EXIT_FAILURE);
    }
    // 소켓 쌍 생성
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, fd2) == -1) {
        perror("socketpair failed");
        exit(EXIT_FAILURE);
    }

    // 자식 프로세스 생성
    pid = fork();

    if (pid == -1) {
        perror("fork");
        return 1;
    } else if (pid == 0) {
        if (dup2(fd1[0], 3) < 0) {
            perror("dup2 failed");
             return 1;
        }
        if (dup2(fd2[1], 4) < 0) {
            perror("dup2 failed");
             return 1;
        }
        // 원본 파이프의 읽기 닫기 및 필요 없는 파일 디스크립터 닫기
        // close(fd1[0]);
        // close(fd2[1]);

        // 프로그램과 인자 실행
        execlp("/opt/google/chrome/chrome", 
            "/opt/google/chrome/chrome", 
            "--remote-debugging-pip", 
            NULL
        );

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

        const size_t BUF_SIZE = 1024;
        char buf[BUF_SIZE];
        int num_bytes = 0;
        while ((num_bytes = read(fd2[0], buf, BUF_SIZE)) > 0) {
            buf[num_bytes] = '\0'; // 문자열 끝을 표시하기 위해 널 문자 추가
            printf("부모 프로세스가 받은 데이터: %s\n", buf);
        }

        // close(fd1[1]); // 첫 번째 파이프의 쓰기 닫기 (fd 3 읽기)
        // close(fd2[0]); // 두 번째 파이프의 읽기 닫기 (fd 4 쓰기)

        // 자식 프로세스가 종료될 때까지 기다림
        waitpid(pid, NULL, 0);
    }

    return 0;
}