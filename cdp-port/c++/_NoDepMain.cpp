#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <openssl/sha.h>
#include <openssl/rand.h>
#include <openssl/bio.h>
#include <openssl/evp.h>

#include <openssl/buffer.h> // Add this line

void base64_encode(const unsigned char* input, int length, char* output) {
    BIO *b64 = BIO_new(BIO_f_base64());
    BIO *mem = BIO_new(BIO_s_mem());
    b64 = BIO_push(b64, mem);
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    BIO_write(b64, input, length);
    BIO_flush(b64);
    BUF_MEM *bptr;
    BIO_get_mem_ptr(b64, &bptr);
    memcpy(output, bptr->data, bptr->length);
    output[bptr->length] = '\0';
    BIO_free_all(b64);
}

void create_websocket_key(char* output) {
    unsigned char key[16];
    RAND_bytes(key, sizeof(key));
    base64_encode(key, sizeof(key), output);
}

int create_socket(const char* host, int port) {
    int sock;
    struct sockaddr_in server_addr;
    struct hostent *server;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("ERROR opening socket");
        exit(1);
    }

    server = gethostbyname(host);
    if (server == NULL) {
        fprintf(stderr, "ERROR, no such host\n");
        exit(1);
    }

    bzero((char*)&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    bcopy((char*)server->h_addr, (char*)&server_addr.sin_addr.s_addr, server->h_length);
    server_addr.sin_port = htons(port);

    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("ERROR connecting");
        exit(1);
    }

    return sock;
}

void send_websocket_handshake(int sock, const char* host, const char* resource) {
    char handshake_request[1024];
    char websocket_key[25];
    create_websocket_key(websocket_key);

    sprintf(handshake_request, 
            "GET %s HTTP/1.1\r\n"
            "Host: %s\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            "Sec-WebSocket-Key: %s\r\n"
            "Sec-WebSocket-Version: 13\r\n\r\n", 
            resource, host, websocket_key);

    write(sock, handshake_request, strlen(handshake_request));
    char response[1024];
    read(sock, response, 1024);

    // if (strstr(response, "101 Switching Protocols") == NULL) {
    //     fprintf(stderr, "WebSocket handshake failed\n");
    //     exit(1);
    // }
}

void send_websocket_frame(int sock, const char* message) {
    unsigned char frame[10];
    size_t length = strlen(message);
    frame[0] = 0x81; // FIN + text frame opcode

    if (length <= 125) {
        frame[1] = (unsigned char)length;
        write(sock, frame, 2);
    } else if (length <= 65535) {
        frame[1] = 126;
        frame[2] = (length >> 8) & 0xFF;
        frame[3] = length & 0xFF;
        write(sock, frame, 4);
    } else {
        frame[1] = 127;
        for (int i = 0; i < 8; ++i) {
            frame[2 + i] = (length >> (8 * (7 - i))) & 0xFF;
        }
        write(sock, frame, 10);
    }

    write(sock, message, length);
}

int main() {
    const char* host = "localhost";
    int port = 9222;
    const char* resource = "/devtools/browser/5ec78c48-32ef-4d7b-94f9-ac38b997494c";
    const char* json_data = "{\"id\": 1, \"method\": \"Page.navigate\", \"params\": {\"url\": \"https://wwe.naver.com\"}}";

    int sock = create_socket(host, port);
    send_websocket_handshake(sock, host, resource);
    send_websocket_frame(sock, json_data);

    const int MAX_BUFFER_SIZE = 4096;
    char buffer[MAX_BUFFER_SIZE] = {0, };
    int bytesReceived = read(sock, buffer, MAX_BUFFER_SIZE);
    close(sock);
    return 0;
}
