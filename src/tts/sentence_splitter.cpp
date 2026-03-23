#include "tts/sentence_splitter.hpp"

#include <cstdint>

namespace tts_bot {

static bool is_sentence_end(const std::string& text, size_t pos) {
    uint8_t b = static_cast<uint8_t>(text[pos]);

    // ASCII 句読点
    if (b == '.' || b == '!' || b == '?' || b == '\n') return true;

    // 3バイト UTF-8 (日本語句読点)
    if (b >= 0xE0 && pos + 2 < text.size()) {
        uint32_t cp = ((b & 0x0F) << 12) |
                      ((static_cast<uint8_t>(text[pos + 1]) & 0x3F) << 6) |
                      (static_cast<uint8_t>(text[pos + 2]) & 0x3F);
        // 。= U+3002, ！= U+FF01, ？= U+FF1F
        if (cp == 0x3002 || cp == 0xFF01 || cp == 0xFF1F) return true;
    }

    return false;
}

static size_t utf8_char_len(uint8_t b) {
    if (b < 0x80) return 1;
    if (b < 0xE0) return 2;
    if (b < 0xF0) return 3;
    return 4;
}

std::vector<std::string> split_sentences(const std::string& text,
                                          size_t min_length) {
    // 短いテキストは分割しない
    if (text.size() < min_length * 3) { // UTF-8 で約 min_length 文字
        return {text};
    }

    std::vector<std::string> result;
    size_t start = 0;
    size_t pos = 0;

    while (pos < text.size()) {
        if (is_sentence_end(text, pos)) {
            size_t char_len = utf8_char_len(static_cast<uint8_t>(text[pos]));
            size_t end = pos + char_len;
            auto sentence = text.substr(start, end - start);

            // 空白のみの文は除外
            bool has_content = false;
            for (char c : sentence) {
                if (c != ' ' && c != '\n' && c != '\r' && c != '\t') {
                    has_content = true;
                    break;
                }
            }

            if (has_content) result.push_back(sentence);
            start = end;
            pos = end;
        } else {
            pos += utf8_char_len(static_cast<uint8_t>(text[pos]));
        }
    }

    // 残りがあれば追加
    if (start < text.size()) {
        auto tail = text.substr(start);
        bool has_content = false;
        for (char c : tail) {
            if (c != ' ' && c != '\n' && c != '\r' && c != '\t') {
                has_content = true;
                break;
            }
        }
        if (has_content) result.push_back(tail);
    }

    if (result.empty()) return {text};
    return result;
}

} // namespace tts_bot
