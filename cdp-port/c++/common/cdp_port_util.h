// fpdf_utils.h
#pragma once
#include <vector>
#include <string>

namespace cdp {
    auto base64Decode = [](const std::string &encoded_string) -> std::vector<unsigned char>
    {
        static const std::string base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                                "abcdefghijklmnopqrstuvwxyz"
                                                "0123456789+/";
        auto is_base64 = [](unsigned char c) -> bool
        {
            return (isalnum(c) || (c == '+') || (c == '/'));
        };

        size_t in_len = encoded_string.size();
        size_t i = 0;
        size_t j = 0;
        size_t in_ = 0;
        unsigned char char_array_4[4], char_array_3[3];
        std::vector<unsigned char> ret;

        while (in_len-- && (encoded_string[in_] != '=') && is_base64(encoded_string[in_]))
        {
            char_array_4[i++] = encoded_string[in_];
            in_++;
            if (i == 4)
            {
                for (i = 0; i < 4; i++)
                {
                    char_array_4[i] = base64_chars.find(char_array_4[i]);
                }
                char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
                char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
                char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

                for (i = 0; (i < 3); i++)
                {
                    ret.push_back(char_array_3[i]);
                }
                i = 0;
            }
        }

        if (i)
        {
            for (j = i; j < 4; j++)
            {
                char_array_4[j] = 0;
            }
            for (j = 0; j < 4; j++)
            {
                char_array_4[j] = base64_chars.find(char_array_4[j]);
            }
            char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
            char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
            char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

            for (j = 0; (j < i - 1); j++)
            {
                ret.push_back(char_array_3[j]);
            }
        }

        return ret;
    };

    // -----
    // WebSocket
    // -----
    typedef enum enumWebSocketFlags {
        // opcodes
        WS_OP_CONTINUE = 0x0,
        WS_OP_TEXT = 0x1,
        WS_OP_BINARY = 0x2,
        WS_OP_CLOSE = 0x8,
        WS_OP_PING = 0x9,
        WS_OP_PONG = 0xA,

        // marks
        WS_FINAL_FRAME = 0x10,
        WS_HAS_MASK = 0x20,
    } WebSocketFlags;
    const char WS_OP_MASK = 0xF;
    const char WS_FIN = WS_FINAL_FRAME;

    auto createWebSocketBuffer = [](
        const char* data,
        size_t data_len,
        WebSocketFlags flags = static_cast<WebSocketFlags>(WS_OP_TEXT | WS_FINAL_FRAME | WS_HAS_MASK)) -> std::vector<char>
    {
        auto _calcFrameSize = [](WebSocketFlags flags, size_t data_len) -> size_t
        {
            size_t size = data_len + 2; // body + 2 bytes of head
            if (data_len >= 126) {
                if (data_len > 0xFFFF) {
                    size += 8;
                } else {
                    size += 2;
                }
            }
            if (flags & WS_HAS_MASK) {
                size += 4;
            }

            return size;
        };
        auto _buildFrame = [](
            char *frame,
            WebSocketFlags flags,
            const char mask[4],
            const char *data, size_t data_len) -> size_t
        {
            auto _decode = [](char *dst, const char *src, size_t len, const char mask[4], uint8_t mask_offset) -> uint8_t {
                size_t i = 0;
                for (; i < len; i++) {
                    dst[i] = src[i] ^ mask[(i + mask_offset) % 4];
                }

                return (uint8_t)((i + mask_offset) % 4);
            };

            size_t body_offset = 0;
            frame[0] = 0;
            frame[1] = 0;
            if (flags & WS_FIN) {
                frame[0] = (char)(1 << 7);
            }
            frame[0] |= flags & WS_OP_MASK;
            if (flags & WS_HAS_MASK) {
                frame[1] = (char)(1 << 7);
            }
            if (data_len < 126) {
                frame[1] |= data_len;
                body_offset = 2;
            } else if (data_len <= 0xFFFF) {
                frame[1] |= 126;
                frame[2] = (char)(data_len >> 8);
                frame[3] = (char)(data_len & 0xFF);
                body_offset = 4;
            } else {
                frame[1] |= 127;
                frame[2] = (char)((data_len >> 56) & 0xFF);
                frame[3] = (char)((data_len >> 48) & 0xFF);
                frame[4] = (char)((data_len >> 40) & 0xFF);
                frame[5] = (char)((data_len >> 32) & 0xFF);
                frame[6] = (char)((data_len >> 24) & 0xFF);
                frame[7] = (char)((data_len >> 16) & 0xFF);
                frame[8] = (char)((data_len >> 8) & 0xFF);
                frame[9] = (char)((data_len) & 0xFF);
                body_offset = 10;
            }
            if (flags & WS_HAS_MASK) {
                if (mask != nullptr) {
                    memcpy(&frame[body_offset], mask, 4);
                }
                _decode(&frame[body_offset + 4], data, data_len, &frame[body_offset], 0);
                body_offset += 4;
            } else {
                memcpy(&frame[body_offset], data, data_len);
            }

            return body_offset + data_len;
        };

        std::vector<char> vBuff(_calcFrameSize(flags, data_len), 0 /* 초기화 */);
        _buildFrame(&vBuff[0], flags, nullptr, data, data_len);
        return vBuff;
    };
}
