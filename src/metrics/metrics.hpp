#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace tts_bot {

class SynthesizerPool;

class MetricsServer {
public:
    using GuildCountFn = std::function<size_t()>;

    MetricsServer(uint16_t port, SynthesizerPool& pool,
                  GuildCountFn guild_count_fn);
    ~MetricsServer();

    void start();
    void stop();
    void record_synth_ms(double ms);

private:
    void serve_loop();
    std::string render_metrics();

    uint16_t port_;
    SynthesizerPool& pool_;
    GuildCountFn guild_count_fn_;

    int listen_fd_ = -1;
    std::thread thread_;
    std::atomic<bool> running_{false};

    mutable std::mutex hist_mutex_;
    std::vector<double> buckets_{50, 100, 200, 500, 1000, 2000, 5000};
    std::vector<uint64_t> bucket_counts_;
    double hist_sum_ = 0;
    uint64_t hist_count_ = 0;
};

} // namespace tts_bot
