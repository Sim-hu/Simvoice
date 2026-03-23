#pragma once

#include <cstdint>
#include <string>

namespace tts_bot {

struct Config {
    std::string discord_token;
    std::string open_jtalk_dict_dir;
    std::string model_path;
    std::string model_dir;  // 全 .vvm を一括ロード
    uint32_t default_style_id = 0;
    uint16_t cpu_num_threads = 0;

    // クラスタリング
    uint32_t shard_count = 0;    // 0 = 自動
    uint32_t cluster_id = 0;
    uint32_t max_clusters = 1;

    size_t cache_mb = 64;
    uint16_t metrics_port = 0; // 0 = 無効
    uint32_t synth_timeout_sec = 30;

    static Config from_env();
};

} // namespace tts_bot
