#include "tts/synthesizer.hpp"
#include "tts/interface.hpp"
#include "audio/pipeline.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>

namespace tts_bot {

SynthesizerPool::SynthesizerPool(VoicevoxEngine& engine, size_t num_workers,
                                 size_t cache_bytes)
    : engine_(engine), cache_(cache_bytes) {
    for (size_t i = 0; i < num_workers; ++i) {
        workers_.emplace_back(&SynthesizerPool::worker_loop, this);
    }
    spdlog::info("SynthesizerPool: {} workers, {}MB cache", num_workers,
                 cache_bytes / (1024 * 1024));
}

SynthesizerPool::~SynthesizerPool() { stop(); }

void SynthesizerPool::stop() {
    if (!running_.exchange(false)) return;
    notify_cv_.notify_all();
    for (auto& w : workers_) {
        if (w.joinable()) w.join();
    }
}

void SynthesizerPool::submit(TTSRequest req) {
    auto gid = static_cast<uint64_t>(req.guild_id);

    {
        std::unique_lock lock(guilds_mutex_);
        auto it = guilds_.find(gid);
        if (it == guilds_.end()) {
            guilds_[gid] = std::make_unique<GuildQueue>();
            guild_order_.push_back(gid);
            it = guilds_.find(gid);
        }
        it->second->push(std::move(req));
    }

    ++pending_;
    notify_cv_.notify_one();
}

std::optional<TTSRequest> SynthesizerPool::take_work() {
    std::unique_lock lock(guilds_mutex_);

    if (guild_order_.empty()) return std::nullopt;

    // ラウンドロビンで各ギルドを巡回
    size_t start = rr_index_;
    do {
        auto gid = guild_order_[rr_index_];
        rr_index_ = (rr_index_ + 1) % guild_order_.size();

        auto it = guilds_.find(gid);
        if (it != guilds_.end()) {
            TTSRequest req;
            if (it->second->try_pop(req)) {
                return req;
            }
        }
    } while (rr_index_ != start);

    return std::nullopt;
}

void SynthesizerPool::worker_loop() {
    while (running_) {
        // 仕事があるまで待機
        {
            std::unique_lock lock(notify_mutex_);
            notify_cv_.wait(lock, [this] {
                return !running_ || pending_.load() > 0;
            });
        }

        if (!running_) break;

        auto req = take_work();
        if (!req) continue;

        --pending_;

        try {
            auto cache_key = AudioCache::make_key(
                req->text, req->style_id, req->speed_scale, req->pitch_scale);

            // キャッシュ確認
            auto t_start = std::chrono::steady_clock::now();
            auto cached = cache_.get(cache_key);
            if (cached) {
                auto t_end = std::chrono::steady_clock::now();
                double hit_ms = std::chrono::duration<double, std::milli>(t_end - t_start).count();
                spdlog::info("Cache hit: {:.2f}ms \"{}\" (guild {})",
                             hit_ms, req->text.substr(0, 20),
                             static_cast<uint64_t>(req->guild_id));
                if (req->on_complete) req->on_complete(*cached);
                continue;
            }

            freq_tracker_.record(req->text, req->style_id);

            // TTS 合成 (計測)
            auto t0 = std::chrono::steady_clock::now();
            SynthParams params{req->speed_scale, req->pitch_scale};
            auto stereo = engine_.synthesize(req->text, req->style_id, params);
            auto t1 = std::chrono::steady_clock::now();
            double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

            {
                std::lock_guard lock(stats_mutex_);
                synth_times_ms_.push_back(ms);
                if (ms < min_ms_) min_ms_ = ms;
                if (ms > max_ms_) max_ms_ = ms;
            }

            cache_.put(cache_key, stereo);

            if (req->on_complete) req->on_complete(stereo);

            spdlog::info("TTS: {:.0f}ms \"{}\" (guild {})",
                         ms, req->text.substr(0, 20),
                         static_cast<uint64_t>(req->guild_id));
        } catch (const std::exception& e) {
            spdlog::error("Worker TTS error: {}", e.what());
        }
    }
}

AudioCache::Stats SynthesizerPool::cache_stats() const {
    return cache_.stats();
}

SynthesizerPool::SynthStats SynthesizerPool::synth_stats() const {
    std::lock_guard lock(stats_mutex_);
    SynthStats s;
    s.total_synths = synth_times_ms_.size();
    if (s.total_synths == 0) return s;

    s.min_ms = min_ms_;
    s.max_ms = max_ms_;
    for (auto v : synth_times_ms_) s.total_ms += v;

    auto sorted = synth_times_ms_;
    std::sort(sorted.begin(), sorted.end());
    s.p50_approx = sorted[sorted.size() / 2];
    return s;
}

} // namespace tts_bot
