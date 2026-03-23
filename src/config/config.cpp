#include "config/config.hpp"

#include <cstdlib>
#include <fstream>
#include <stdexcept>
#include <string>

namespace tts_bot {

static void load_dotenv(const std::string& path) {
    std::ifstream ifs(path);
    if (!ifs) return;

    std::string line;
    while (std::getline(ifs, line)) {
        if (line.empty() || line[0] == '#') continue;
        auto pos = line.find('=');
        if (pos == std::string::npos) continue;

        std::string key = line.substr(0, pos);
        std::string val = line.substr(pos + 1);

        // 既存の環境変数を上書きしない
        setenv(key.c_str(), val.c_str(), 0);
    }
}

Config Config::from_env() {
    load_dotenv(".env");

    Config config;

    if (const char* path = std::getenv("DISCORD_TOKEN_FILE")) {
        std::ifstream ifs(path);
        if (!ifs) {
            throw std::runtime_error(
                std::string("Cannot open DISCORD_TOKEN_FILE: ") + path);
        }
        std::getline(ifs, config.discord_token);
    } else if (const char* token = std::getenv("DISCORD_TOKEN")) {
        config.discord_token = token;
    } else {
        throw std::runtime_error(
            "DISCORD_TOKEN or DISCORD_TOKEN_FILE must be set");
    }

    auto& t = config.discord_token;
    t.erase(0, t.find_first_not_of(" \t\n\r"));
    t.erase(t.find_last_not_of(" \t\n\r") + 1);

    if (t.empty()) {
        throw std::runtime_error("Discord token is empty");
    }

    if (const char* v = std::getenv("OPEN_JTALK_DICT_DIR"))
        config.open_jtalk_dict_dir = v;

    if (const char* v = std::getenv("MODEL_PATH"))
        config.model_path = v;

    if (const char* v = std::getenv("DEFAULT_STYLE_ID"))
        config.default_style_id = static_cast<uint32_t>(std::stoul(v));

    if (const char* v = std::getenv("CPU_NUM_THREADS"))
        config.cpu_num_threads = static_cast<uint16_t>(std::stoul(v));

    return config;
}

} // namespace tts_bot
