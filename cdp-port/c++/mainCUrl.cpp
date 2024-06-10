#include <curl/curl.h>
#include <iostream>
#include <sstream>
#include "nlohmann/json.hpp"

using json = nlohmann::json;

size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    userp->append((char*)contents, size * nmemb);
    return size * nmemb;
}

std::string getWebSocketDebuggerUrl(const char* addr = "127.0.0.1", uint16_t port = 9222) {
    CURL* curl;
    CURLcode res;
    std::string readBuffer;

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();

    if(curl) {
        std::stringstream ss;
        ss << "http://" << addr << ":" << port << "/json/version";
        curl_easy_setopt(curl, CURLOPT_URL, ss.str().c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        curl_easy_setopt(curl, CURLOPT_HEADER, 1L); // 헤더도 받아오기
        res = curl_easy_perform(curl);

        if(res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        }

        curl_easy_cleanup(curl);
    }

    curl_global_cleanup();

    std::cout << "[http][response.header()] :\n" << readBuffer << std::endl;

    size_t pos = readBuffer.find("\r\n\r\n");
    json message;
    if (pos != std::string::npos) {
        std::string responseBody = readBuffer.substr(pos + 4);
        message = json::parse(responseBody);
        std::cout << "[http][response.body()] :\n" << message.dump(4) << std::endl;
    } else {
        std::cout << "No body found in the response" << std::endl;
        return "";
    }

    return message["webSocketDebuggerUrl"];
}

int main() {

    std::string webSocketDebuggerUrl = getWebSocketDebuggerUrl();
    std::cout << "webSocketDebuggerUrl : " << webSocketDebuggerUrl << std::endl;

    return 0;
}
