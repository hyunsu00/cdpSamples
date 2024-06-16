// chrome_launcher.c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <vector>
#include <sys/wait.h>

int main() {
    int pipe_to_chrome[2];  // Pipe to Chrome (fd 3 for reading)
    int pipe_from_chrome[2];  // Pipe from Chrome (fd 4 for writing)

    // Create the pipes
    if (pipe(pipe_to_chrome) == -1 || pipe(pipe_from_chrome) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    // Set pipe ends to non-blocking
    // fcntl(pipe_to_chrome[0], F_SETFL, O_NONBLOCK);
    // fcntl(pipe_to_chrome[1], F_SETFL, O_NONBLOCK);
    // fcntl(pipe_from_chrome[0], F_SETFL, O_NONBLOCK);
    // fcntl(pipe_from_chrome[1], F_SETFL, O_NONBLOCK);

    // Fork a new process
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (pid == 0) {
        // Child process

        // Duplicate the file descriptors to fd 3 and fd 4
        if (dup2(pipe_to_chrome[0], 3) == -1 || dup2(pipe_from_chrome[1], 4) == -1) {
            perror("dup2");
            exit(EXIT_FAILURE);
        }

        // Close original pipe file descriptors in the child process
        close(pipe_to_chrome[0]);
        close(pipe_to_chrome[1]);
        close(pipe_from_chrome[0]);
        close(pipe_from_chrome[1]);

        // Execute Chrome with remote debugging pipe
        execl("/usr/bin/chrome", "--enable-features=UseOzonePlatform", "--ozone-platform=wayland", 
            "--enable-logging", "--v=2", "--no-sandbox", "--disable-gpu", "--headless",
            "--remote-debugging-pipe", NULL);

        // If execl fails
        perror("execl");
        exit(EXIT_FAILURE);
    } else {
        // Parent process

        // Close unused file descriptors in the parent process
        close(pipe_to_chrome[0]);
        close(pipe_from_chrome[1]);

        // Now, pipe_to_chrome[1] (fd 3) is for writing to Chrome
        // and pipe_from_chrome[0] (fd 4) is for reading from Chrome

        // Example: Write a command to Chrome
        const char* command = "{\"id\": 1, \"method\": \"Target.setDiscoverTargets\", \"params\": {\"discover\": true}}\n";
        std::vector<char> commandBuffer(strlen(command) + 1, 0 /* zero-initialized */);
        strcpy(commandBuffer.data(), command);
        write(pipe_to_chrome[1], commandBuffer.data(), commandBuffer.size());

        // Example: Read response from Chrome
        char buffer[1024] = {0, };
        ssize_t bytes_read = read(pipe_to_chrome[0], buffer, sizeof(buffer) - 1);
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';
            printf("Received: %s\n", buffer);
        }

        // Close the file descriptors when done
        close(pipe_to_chrome[1]);
        close(pipe_from_chrome[0]);

        // Wait for the child process to finish
        wait(NULL);
    }

    return 0;
}
