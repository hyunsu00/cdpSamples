#include <uv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void on_exit(uv_process_t *req, int64_t exit_status, int term_signal) {
    printf("Process exited with status %lld, signal %d\n", exit_status, term_signal);
    // uv_close((uv_handle_t *) &write_fd, NULL);
    uv_close((uv_handle_t *) req, NULL);
}

void on_write(uv_write_t *req, int status) {
    if (status) {
        fprintf(stderr, "Write error %s\n", uv_strerror(status));
    }
    free(req);
}

// 루프를 무한 대기 상태로 유지하기 위한 더미 작업
void idle_callback(uv_idle_t* handle) {
    // 이 콜백은 무한히 호출되므로, 아무 작업도 하지 않습니다.
    // 필요하다면 여기에 실제 작업을 추가할 수 있습니다.
}

int main() {

    uv_loop_t* loop = uv_default_loop();

    uv_pipe_t write_fd, read_fd;
    uv_pipe_init(loop, &write_fd, 0);
    uv_pipe_init(loop, &read_fd, 0);
    
    uv_stdio_container_t stdio[5];  // 파일 디스크립터 배열
    // 표준 입력 (stdin)
    stdio[0].flags = UV_IGNORE;
    stdio[0].data.fd = 0;
    // 표준 출력 (stdout)
    stdio[1].flags = UV_IGNORE;
    stdio[1].data.fd = 1;
    // 표준 에러 (stderr)
    stdio[2].flags = UV_IGNORE;
    stdio[2].data.fd = 2;
    // 사용자 정의 파일 디스크립터 (fd 3)
    stdio[3].flags = (uv_stdio_flags)(UV_CREATE_PIPE | UV_READABLE_PIPE);
    stdio[3].data.stream = (uv_stream_t*)&write_fd;
    stdio[4].flags = (uv_stdio_flags)(UV_CREATE_PIPE | UV_WRITABLE_PIPE);
    stdio[4].data.stream = (uv_stream_t*)&read_fd;

    uv_process_options_t options = {0, };
    options.stdio = stdio;
    options.stdio_count = 5;  // 사용할 파일 디스크립터의 수
    const char* args[3] = {
        "/opt/google/chrome/chrome", 
        "--remote-debugging-pipe", 
        NULL
    };
    options.file = "/opt/google/chrome/chrome";
    options.args = (char**)args;
    options.exit_cb = on_exit;

    uv_process_t process;
    int r = uv_spawn(loop, &process, &options);
    if (r) {
        fprintf(stderr, "Error spawning process: %s\n", uv_strerror(r));
        return 1;
    }

    fprintf(stderr, "Launched sleep with PID %d\n", process.pid);
    uv_unref((uv_handle_t*)&process);

    const char* request = R"({ "id": 1, "method": "Target.createTarget", "params": { "url": "https://www.naver.com" })";
    uv_buf_t buffer = uv_buf_init((char*)request, strlen(request));
    uv_write_t* write_req = (uv_write_t *) malloc(sizeof(uv_write_t));
    uv_write(write_req, (uv_stream_t *)&write_fd, &buffer, 1, on_write);


    uv_idle_t idle_handle;
    // idle 핸들 초기화 및 시작: 이벤트 루프가 계속 실행되도록 만듭니다.
    uv_idle_init(loop, &idle_handle);
    uv_idle_start(&idle_handle, idle_callback);

    uv_run(loop, UV_RUN_DEFAULT);

    uv_loop_close(loop);
    return 0;
}