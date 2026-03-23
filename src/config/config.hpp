#pragma once

#include <cstdint>
#include <string>

namespace tts_bot {

struct Config {
    std::string discord_token;
    std::string open_jtalk_dict_dir;
    std::string model_path;
    uint32_t default_style_id = 0;
    uint16_t cpu_num_threads = 0;

    static Config from_env();
};

} // namespace tts_bot
