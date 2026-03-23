#include "tts/preprocessor.hpp"
#include "db/database.hpp"

#include <regex>

namespace tts_bot {

std::string TextPreprocessor::process(const std::string& text,
                                      const std::vector<DictEntry>& dict,
                                      size_t max_chars) {
    auto result = text;
    result = remove_urls(result);
    result = convert_mentions(result);
    result = convert_channel_mentions(result);
    result = convert_custom_emoji(result);
    result = strip_unicode_emoji(result);
    if (!dict.empty()) result = apply_dict(result, dict);
    result = normalize_whitespace(result);
    result = truncate_utf8(result, max_chars);
    return result;
}

std::string TextPreprocessor::apply_dict(const std::string& text,
                                          const std::vector<DictEntry>& dict) {
    std::string result = text;
    for (auto& entry : dict) {
        size_t pos = 0;
        while ((pos = result.find(entry.word, pos)) != std::string::npos) {
            result.replace(pos, entry.word.size(), entry.reading);
            pos += entry.reading.size();
        }
    }
    return result;
}

std::string TextPreprocessor::remove_urls(const std::string& text) {
    static const std::regex url_re(R"(https?://\S+)");
    return std::regex_replace(text, url_re, "URL省略");
}

std::string TextPreprocessor::convert_mentions(const std::string& text) {
    // <@!123456> or <@123456> → 「誰か」
    static const std::regex mention_re(R"(<@!?\d+>)");
    return std::regex_replace(text, mention_re, "誰か");
}

std::string TextPreprocessor::convert_channel_mentions(const std::string& text) {
    // <#123456> → 「どこか」
    static const std::regex ch_re(R"(<#\d+>)");
    return std::regex_replace(text, ch_re, "どこか");
}

std::string TextPreprocessor::convert_custom_emoji(const std::string& text) {
    // <:name:123456> or <a:name:123456> → 除去
    static const std::regex custom_re(R"(<a?:\w+:\d+>)");
    auto result = std::regex_replace(text, custom_re, "");
    // :emoji_name: 形式のショートコード → 除去
    static const std::regex shortcode_re(R"(:\w+:)");
    return std::regex_replace(result, shortcode_re, "");
}

std::string TextPreprocessor::strip_unicode_emoji(const std::string& text) {
    std::string result;
    result.reserve(text.size());
    size_t i = 0;

    while (i < text.size()) {
        uint8_t b = static_cast<uint8_t>(text[i]);
        size_t len;
        uint32_t cp = 0;

        if (b < 0x80) {
            len = 1;
            cp = b;
        } else if (b < 0xE0) {
            len = 2;
            if (i + 1 < text.size())
                cp = ((b & 0x1F) << 6) | (text[i + 1] & 0x3F);
        } else if (b < 0xF0) {
            len = 3;
            if (i + 2 < text.size())
                cp = ((b & 0x0F) << 12) | ((text[i + 1] & 0x3F) << 6) |
                     (text[i + 2] & 0x3F);
        } else {
            len = 4;
            if (i + 3 < text.size())
                cp = ((b & 0x07) << 18) | ((text[i + 1] & 0x3F) << 12) |
                     ((text[i + 2] & 0x3F) << 6) | (text[i + 3] & 0x3F);
        }

        bool is_emoji = false;

        // 4バイト文字 (U+10000+) はほぼ絵文字
        if (len == 4) is_emoji = true;

        // 3バイト絵文字範囲
        if (cp >= 0x2600 && cp <= 0x27BF) is_emoji = true;   // Misc Symbols
        if (cp >= 0x2300 && cp <= 0x23FF) is_emoji = true;   // Misc Technical
        if (cp >= 0x2B50 && cp <= 0x2B55) is_emoji = true;   // Stars
        if (cp >= 0x3297 && cp <= 0x3299) is_emoji = true;   // CJK enclosed
        if (cp == 0x200D) is_emoji = true;                    // ZWJ
        if (cp >= 0xFE00 && cp <= 0xFE0F) is_emoji = true;   // Variation Sel
        if (cp == 0x20E3) is_emoji = true;                    // Keycap

        if (!is_emoji) {
            result.append(text, i, len);
        }

        i += len;
    }

    return result;
}

std::string TextPreprocessor::normalize_whitespace(const std::string& text) {
    std::string result;
    result.reserve(text.size());
    bool prev_space = false;

    for (char c : text) {
        if (c == '\n' || c == '\r' || c == '\t') c = ' ';
        if (c == ' ' && prev_space) continue;
        result += c;
        prev_space = (c == ' ');
    }

    // trim
    while (!result.empty() && result.back() == ' ') result.pop_back();
    if (!result.empty() && result.front() == ' ') result.erase(0, 1);

    return result;
}

std::string TextPreprocessor::truncate_utf8(const std::string& text, size_t max_chars) {
    size_t char_count = 0;
    size_t byte_pos = 0;

    while (byte_pos < text.size() && char_count < max_chars) {
        uint8_t b = static_cast<uint8_t>(text[byte_pos]);
        if (b < 0x80) byte_pos += 1;
        else if (b < 0xE0) byte_pos += 2;
        else if (b < 0xF0) byte_pos += 3;
        else byte_pos += 4;
        ++char_count;
    }

    if (byte_pos >= text.size()) return text;
    return text.substr(0, byte_pos) + "、以下省略";
}

} // namespace tts_bot
