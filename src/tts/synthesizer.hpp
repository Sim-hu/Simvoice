#pragma once

#include "audio/cache.hpp"
#include "guild/queue.hpp"
#include "tts/voicevox.hpp"
#include "tts/warmup.hpp"

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
    AudioCache& cache_ref() { return cache_; }
    FrequencyTracker& freq_ref() { return freq_tracker_; }

    struct SynthStats {
        uint64_t total_synths = 0;
        double total_ms = 0;
        double min_ms = 1e9;
        double max_ms = 0;
        double p50_approx = 0;
    };
    SynthStats synth_stats() const;

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

    mutable std::mutex stats_mutex_;
    std::vector<double> synth_times_ms_;
    double min_ms_ = 1e9;
    double max_ms_ = 0;

    FrequencyTracker freq_tracker_;
};

} // namespace tts_bot
