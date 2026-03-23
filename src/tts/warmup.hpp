#pragma once

#include "audio/cache.hpp"
#include "tts/voicevox.hpp"

#include <thread>
#include <vector>

namespace tts_bot {

class Database;

class CacheWarmer {
public:
    CacheWarmer(VoicevoxEngine& engine, AudioCache& cache);
    ~CacheWarmer();

    void start(const std::vector<uint32_t>& style_ids);
    void start_dict(Database& db, const std::vector<uint32_t>& style_ids);
    void stop();

private:
    void run(std::vector<uint32_t> style_ids);
    void run_dict(Database* db, std::vector<uint32_t> style_ids);

    void warm_phrase(const std::string& text, uint32_t style_id,
                     size_t& done, size_t& errors);

    VoicevoxEngine& engine_;
    AudioCache& cache_;
    std::thread thread_;
    std::thread dict_thread_;
    std::atomic<bool> running_{false};
};

// 頻出テキストの追跡
class FrequencyTracker {
public:
    void record(const std::string& text, uint32_t style_id);
    std::vector<std::pair<std::string, uint32_t>> top(size_t n) const;

private:
    mutable std::mutex mutex_;
    std::unordered_map<size_t, uint64_t> counts_;
    // key -> (text, style_id) の逆引き
    std::unordered_map<size_t, std::pair<std::string, uint32_t>> entries_;
};

} // namespace tts_bot
