#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uv.h>

uv_pipe_t stdin_pipe;
uv_pipe_t stdout_pipe;
uv_loop_t *loop;

void alloc_buffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
    buf->base = (char *) malloc(suggested_size);
    buf->len = suggested_size;
}

void on_write(uv_write_t *req, int status) {
    if (status < 0) {
        fprintf(stderr, "Write error %s\n", uv_strerror(status));
    }
    free(req);
}

void on_read(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) {
    if (nread > 0) {
        fwrite(buf->base, nread, 1, stdout);
    } else if (nread < 0) {
        if (nread != UV_EOF) {
            fprintf(stderr, "Read error %s\n", uv_err_name(nread));
        }
        uv_close((uv_handle_t *) &stdin_pipe, NULL);
        uv_close((uv_handle_t *) &stdout_pipe, NULL);
    }
    free(buf->base);
}

int main() {
    loop = uv_default_loop();

    uv_pipe_init(loop, &stdin_pipe, 0);
    uv_pipe_open(&stdin_pipe, 0);  // 0 is the file descriptor for stdin

    uv_pipe_init(loop, &stdout_pipe, 0);
    uv_pipe_open(&stdout_pipe, 1);  // 1 is the file descriptor for stdout

    uv_read_start((uv_stream_t *) &stdout_pipe, alloc_buffer, on_read);

    const char *command = "{\"id\":1,\"method\":\"Page.navigate\",\"params\":{\"url\":\"https://www.naver.com\"}}\n";

    uv_buf_t buffer = uv_buf_init((char *) command, strlen(command));

    uv_write_t *req = (uv_write_t *) malloc(sizeof(uv_write_t));
    uv_write(req, (uv_stream_t *) &stdin_pipe, &buffer, 1, on_write);

    return uv_run(loop, UV_RUN_DEFAULT);
}