#include <libwebsockets.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int callback(struct lws *wsi, enum lws_callback_reasons reason,
                    void *user, void *in, size_t len) {
    switch (reason) {
        case LWS_CALLBACK_CLIENT_ESTABLISHED:
            printf("Connection established\n");
            break;
        case LWS_CALLBACK_CLIENT_RECEIVE:
            printf("Received: %s\n", (char *)in);
            break;
        case LWS_CALLBACK_CLIENT_WRITEABLE:
            printf("Writable\n");
            break;
        case LWS_CALLBACK_CLIENT_CLOSED:
            printf("Connection closed\n");
            break;
        default:
            break;
    }
    return 0;
}

int main(void) {
    struct lws_context_creation_info info;
    struct lws_client_connect_info ccinfo = {0};
    struct lws_context *context;
    const char *protocol;
    int n = 0;

    memset(&info, 0, sizeof info);
    info.port = CONTEXT_PORT_NO_LISTEN;
    info.protocols = protocols;
    context = lws_create_context(&info);

    if (context == NULL) {
        fprintf(stderr, "lws init failed\n");
        return -1;
    }

    ccinfo.context = context;
    ccinfo.address = "localhost";
    ccinfo.port = 7681;
    ccinfo.path = "/";
    ccinfo.host = lws_canonical_hostname(context);
    ccinfo.origin = "origin";
    ccinfo.protocol = protocol;
    ccinfo.ietf_version_or_minus_one = -1;
    ccinfo.userdata = NULL;

    if (lws_client_connect_via_info(&ccinfo) == NULL) {
        fprintf(stderr, "Client connect failed\n");
        lws_context_destroy(context);
        return -1;
    }

    while (n >= 0) {
        n = lws_service(context, 1000);
    }

    lws_context_destroy(context);
    return 0;
}