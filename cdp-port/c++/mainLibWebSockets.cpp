#include <iostream>
#include <regex>
#include <string>
#include <fstream>
#include <vector>
#include <cstring>
#include <libwebsockets.h>
#include "nlohmann/json.hpp"
#include "common/cdp_port_util.h"

using json = nlohmann::json;

# if 0
struct websocket_data {
    std::string message;
    std::string response;
    bool response_received;
};

static int callback_http(struct lws *wsi, enum lws_callback_reasons reason, 
                         void *user, void *in, size_t len) {
    switch (reason) {
        case LWS_CALLBACK_CLIENT_ESTABLISHED:
            std::cout << "Connection established" << std::endl;
            break;
        
        case LWS_CALLBACK_CLIENT_RECEIVE:
            std::cout << "Received: " << (char *)in << std::endl;
            break;
        
        case LWS_CALLBACK_CLIENT_WRITEABLE:
            std::cout << "Writable" << std::endl;
            break;
        
        case LWS_CALLBACK_CLIENT_CLOSED:
            std::cout << "Connection closed" << std::endl;
            break;
        
        default:
            break;
    }
    return 0;
}

static int callback_dumb_increment(struct lws *wsi, enum lws_callback_reasons reason, 
                                   void *user, void *in, size_t len) {
    websocket_data *ws_data = (websocket_data *)user;

    switch (reason) {
        case LWS_CALLBACK_CLIENT_ESTABLISHED:
            lws_callback_on_writable(wsi);
            break;
        
        case LWS_CALLBACK_CLIENT_WRITEABLE: {
            std::string message = ws_data->message;
            unsigned char *buf = new unsigned char[LWS_PRE + message.size()];
            std::memcpy(buf + LWS_PRE, message.c_str(), message.size());
            lws_write(wsi, buf + LWS_PRE, message.size(), LWS_WRITE_TEXT);
            delete[] buf;
            break;
        }
        
        case LWS_CALLBACK_CLIENT_RECEIVE: {
            ws_data->response.append((char *)in, len);
            ws_data->response_received = true;
            break;
        }
        
        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
            std::cerr << "Connection error" << std::endl;
            break;

        case LWS_CALLBACK_CLIENT_CLOSED:
            std::cout << "Connection closed" << std::endl;
            break;

        default:
            break;
    }
    return 0;
}

// Define your protocols
static struct lws_protocols protocols[] = {
    {"http-only", callback_http, 0, 0},
    {"dumb-increment-protocol", callback_dumb_increment, sizeof(websocket_data), 128},
    {NULL, NULL, 0, 0}
};

void takeScreenshot(const std::string &webSocketDebuggerUrl) {
    std::regex ip_regex("(\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}\\.\\d{1,3})");
    std::regex port_regex(":(\\d+)");
    std::regex path_regex("(/devtools/browser/[a-z0-9\\-]+)");
    std::smatch match;

    std::string addr = "127.0.0.1";
    if (std::regex_search(webSocketDebuggerUrl, match, ip_regex) && match.size() > 1) {
        addr = match.str(1);
        std::cout << "IP: " << addr << std::endl;
    } else {
        std::cout << "No valid IP found in string" << std::endl;
        return;
    }

    uint16_t port = 9222;
    if (std::regex_search(webSocketDebuggerUrl, match, port_regex) && match.size() > 1) {
        port = static_cast<uint16_t>(std::stoi(match.str(1)));
        std::cout << "Port: " << port << std::endl;
    } else {
        std::cout << "No valid port found in string" << std::endl;
        return;
    }

    std::string path;
    if (std::regex_search(webSocketDebuggerUrl, match, path_regex) && match.size() > 1) {
        path = match.str(1);
        std::cout << "Path: " << path << std::endl;
    } else {
        std::cout << "No valid path found in string" << std::endl;
        return;
    }

    struct lws_context_creation_info ctx_info;
    struct lws_client_connect_info client_info;
    struct lws_context *context;
    struct lws *wsi;
    websocket_data ws_data;

    memset(&ctx_info, 0, sizeof(ctx_info));
    ctx_info.port = CONTEXT_PORT_NO_LISTEN;
    ctx_info.protocols = protocols;
    ctx_info.gid = -1;
    ctx_info.uid = -1;

    context = lws_create_context(&ctx_info);
    if (context == NULL) {
        std::cerr << "lws init failed" << std::endl;
        return;
    }

    memset(&client_info, 0, sizeof(client_info));
    client_info.context = context;
    client_info.address = addr.c_str();
    client_info.port = port;
    client_info.path = path.c_str();
    client_info.host = lws_canonical_hostname(context);
    client_info.origin ="origin";
    client_info.protocol = protocols[0].name;
    client_info.pwsi = &wsi;
    client_info.userdata = &ws_data;

    wsi = lws_client_connect_via_info(&client_info);
    if (wsi == NULL) {
        std::cerr << "Client connection failed" << std::endl;
        lws_context_destroy(context);
        return;
    }

    // Send Target.getTargets request
    ws_data.message = R"({"id":1,"method":"Target.getTargets"})";
    ws_data.response_received = false;

    while (!ws_data.response_received) {
        lws_service(context, 1000);
    }
    json rmessage = json::parse(ws_data.response);
    std::cout << "[response] :\n" << rmessage.dump(4) << std::endl;

    // Send Target.attachToTarget request
    json target_info;
    for (const auto &element : rmessage["result"]["targetInfos"]) {
        if (element["type"] == "page") {
            target_info = element;
            break;
        }
    }
    ws_data.message = R"({"id":2,"method":"Target.attachToTarget","params":{"targetId":")" + target_info["targetId"].get<std::string>() + R"(","flatten":true}})";
    ws_data.response_received = false;

    while (!ws_data.response_received) {
        lws_service(context, 1000);
    }
    rmessage = json::parse(ws_data.response);
    std::cout << "[response] :\n" << rmessage.dump(4) << std::endl;

    // Send Page.captureScreenshot request
    ws_data.message = R"({"sessionId":")" + rmessage["result"]["sessionId"].get<std::string>() + R"(","id":3,"method":"Page.captureScreenshot","params":{"format":"png","quality":100,"fromSurface":true}})";
    ws_data.response_received = false;

    while (!ws_data.response_received) {
        lws_service(context, 1000);
    }
    rmessage = json::parse(ws_data.response);
    std::cout << "[response] :\n" << rmessage.dump(4) << std::endl;

    // Save the screenshot
    std::string screenshotData = rmessage["result"]["data"].get<std::string>();
    std::vector<unsigned char> decodedData = base64Decode(screenshotData);
    std::ofstream file("mainSocket-screenshot.png", std::ios::binary);
    file.write(reinterpret_cast<const char *>(decodedData.data()), decodedData.size());
    file.close();

    lws_context_destroy(context);
}
#endif

static int callback(lws *wsi, lws_callback_reasons reason, void *user, void *in, size_t len) {
    switch (reason) {
        case LWS_CALLBACK_CLIENT_ESTABLISHED:
            // 연결이 수립되었을 때의 처리
            break;
        case LWS_CALLBACK_CLIENT_RECEIVE:
            // 메시지를 받았을 때의 처리
            break;
        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
            // 연결에 실패했을 때의 처리
            break;
        default:
            break;
    }
    return 0;
}

static struct lws_protocols protocols[] = {
    {
        "devtools", // 프로토콜 이름
        callback, // 콜백 함수
        0, // per_session_data_size
        0, // rx_buffer_size
        0, // id
        NULL, // user
        0 // tx_packet_size
    },
    { NULL, NULL, 0, 0, 0, NULL, 0 } // 배열 종료를 위한 NULL 항목
};

int main() {

    std::string webSocketDebuggerUrl = "ws://127.0.0.1:9222/devtools/browser/8db5f2ce-e638-4405-ab84-27c3d61c659d";
    
    std::cout << "webSocketDebuggerUrl : " << webSocketDebuggerUrl << std::endl;

    // takeScreenshot(webSocketDebuggerUrl);

    struct lws_context_creation_info context_info;
    memset(&context_info, 0, sizeof(context_info));

    context_info.port = CONTEXT_PORT_NO_LISTEN;
    context_info.protocols = protocols;
    context_info.gid = -1;
    context_info.uid = -1;

    struct lws_context *context = lws_create_context(&context_info);

    struct lws_client_connect_info client_info;
    memset(&client_info, 0, sizeof(client_info));

    client_info.context = context;
    client_info.address = "127.0.0.1";
    client_info.port = 9222;
    client_info.path = "/devtools/browser/8db5f2ce-e638-4405-ab84-27c3d61c659d";
    client_info.host = client_info.address;
    client_info.origin = client_info.address;
    client_info.protocol = protocols[0].name;

    lws *wsi = lws_client_connect_via_info(&client_info);

    while (1) {
        lws_service(context, 50);
    }

    lws_context_destroy(context);

    return 0;
}