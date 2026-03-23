#pragma once

#include <cstddef>
#include <string>

namespace tts_bot {

class TextPreprocessor {
public:
    std::string process(const std::string& text, size_t max_chars = 100);

private:
    static std::string remove_urls(const std::string& text);
    static std::string convert_mentions(const std::string& text);
    static std::string convert_channel_mentions(const std::string& text);
    static std::string convert_custom_emoji(const std::string& text);
    static std::string strip_unicode_emoji(const std::string& text);
    static std::string normalize_whitespace(const std::string& text);
    static std::string truncate_utf8(const std::string& text, size_t max_chars);
};

} // namespace tts_bot
