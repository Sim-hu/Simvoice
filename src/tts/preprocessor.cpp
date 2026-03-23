#include "tts/preprocessor.hpp"
#include "db/database.hpp"

#include <regex>

namespace tts_bot {

std::string TextPreprocessor::process(const std::string& text,
                                      const std::vector<DictEntry>& dict,
                                      size_t max_chars) {
    auto result = text;
    result = remove_code_blocks(result);
    result = remove_spoilers(result);
    result = remove_urls(result);
    result = convert_mentions(result);
    result = convert_channel_mentions(result);
    result = convert_custom_emoji(result);
    result = strip_unicode_emoji(result);
    if (!dict.empty()) result = apply_dict(result, dict);
    result = convert_numbers(result);
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

std::string TextPreprocessor::remove_spoilers(const std::string& text) {
    static const std::regex spoiler_re(R"(\|\|[^|]*\|\|)");
    return std::regex_replace(text, spoiler_re, "");
}

std::string TextPreprocessor::remove_code_blocks(const std::string& text) {
    // マルチラインコードブロック
    static const std::regex block_re(R"(```[\s\S]*?```)");
    auto result = std::regex_replace(text, block_re, "");
    // インラインコード
    static const std::regex inline_re(R"(`[^`]+`)");
    return std::regex_replace(result, inline_re, "");
}

std::string TextPreprocessor::convert_numbers(const std::string& text) {
    // 連続する数字を漢数字表記に変換 (OpenJTalkが読めるように)
    // 例: "123" → "百二十三", "42" → "四十二"
    // 大きい数字はそのまま (OpenJTalkが処理する)
    // ここでは4桁以下の数字のみ日本語化
    static const char* digits[] = {
        "零", "一", "二", "三", "四", "五", "六", "七", "八", "九"};

    auto num_to_kanji = [](int n) -> std::string {
        if (n == 0) return "零";
        std::string result;
        if (n >= 1000) { result += digits[n / 1000]; result += "千"; n %= 1000; }
        if (n >= 100)  { result += digits[n / 100];  result += "百"; n %= 100; }
        if (n >= 10) {
            if (n / 10 > 1) result += digits[n / 10];
            result += "十";
            n %= 10;
        }
        if (n > 0) result += digits[n];
        return result;
    };

    static const std::regex num_re(R"(\d+)");
    std::string result;
    std::sregex_iterator it(text.begin(), text.end(), num_re);
    std::sregex_iterator end;
    size_t last = 0;

    for (; it != end; ++it) {
        result += text.substr(last, it->position() - last);
        auto num_str = it->str();
        if (num_str.size() <= 4) {
            int val = std::stoi(num_str);
            result += num_to_kanji(val);
        } else {
            result += num_str; // 5桁以上はそのまま
        }
        last = it->position() + it->length();
    }
    result += text.substr(last);
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
