#pragma once

#include "audio/cache.hpp"
#include "guild/queue.hpp"
#include "tts/voicevox.hpp"

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <unordered_map>
#include <vector>

namespace tts_bot {

class SynthesizerPool {
public:
    SynthesizerPool(VoicevoxEngine& engine, size_t num_workers,
                    size_t cache_bytes);
    ~SynthesizerPool();

    SynthesizerPool(const SynthesizerPool&) = delete;
    SynthesizerPool& operator=(const SynthesizerPool&) = delete;

    void submit(TTSRequest req);
    void stop();

    AudioCache::Stats cache_stats() const;

private:
    void worker_loop();
    std::optional<TTSRequest> take_work();

    VoicevoxEngine& engine_;
    AudioCache cache_;

    // ギルド別キュー (ラウンドロビンで公平にスケジューリング)
    std::unordered_map<uint64_t, std::unique_ptr<GuildQueue>> guilds_;
    std::vector<uint64_t> guild_order_; // ラウンドロビン用
    std::shared_mutex guilds_mutex_;
    size_t rr_index_ = 0;

    std::mutex notify_mutex_;
    std::condition_variable notify_cv_;
    std::atomic<size_t> pending_{0};

    std::vector<std::thread> workers_;
    std::atomic<bool> running_{true};
};

} // namespace tts_bot
