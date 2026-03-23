#pragma once

#include "audio/cache.hpp"
#include "tts/voicevox.hpp"

#include <thread>
#include <vector>

namespace tts_bot {

class CacheWarmer {
public:
    CacheWarmer(VoicevoxEngine& engine, AudioCache& cache);
    ~CacheWarmer();

    void start(const std::vector<uint32_t>& style_ids);
    void stop();

private:
    void run(std::vector<uint32_t> style_ids);

    VoicevoxEngine& engine_;
    AudioCache& cache_;
    std::thread thread_;
    std::atomic<bool> running_{false};
};

} // namespace tts_bot
