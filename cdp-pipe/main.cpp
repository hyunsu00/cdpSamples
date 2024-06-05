#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <iostream>

int main() {
    int pipefd[2];
    
    if (pipe(pipefd) == -1) {
        std::cerr << "파이프 생성 실패" << std::endl;
        return 1;
    }
    
    // Chrome 실행
    execl("/opt/google/chrome/chrome", "google-chrome","--headless", "--remote-debugging-pipe", "3<&0", "4>&1");
    sleep(5000);
    return 0;
}