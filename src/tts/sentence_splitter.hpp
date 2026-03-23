#pragma once

#include <string>
#include <vector>

namespace tts_bot {

std::vector<std::string> split_sentences(const std::string& text,
                                          size_t min_length = 30);

} // namespace tts_bot
