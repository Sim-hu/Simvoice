#include "tts/synthesizer.hpp"
#include "audio/pipeline.hpp"

#include <spdlog/spdlog.h>

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
            auto cache_key =
                AudioCache::make_key(req->text, req->style_id);

            // キャッシュ確認
            auto cached = cache_.get(cache_key);
            if (cached) {
                spdlog::debug("Cache hit for guild {}",
                              static_cast<uint64_t>(req->guild_id));
                if (req->on_complete) req->on_complete(*cached);
                continue;
            }

            // TTS 合成
            auto wav = engine_.tts(req->text, req->style_id);
            auto pcm = extract_pcm_from_wav(wav.data(), wav.size());
            auto stereo = resample_to_48k_stereo(pcm);

            // キャッシュに保存
            cache_.put(cache_key, stereo);

            // コールバックで送信
            if (req->on_complete) req->on_complete(stereo);

            spdlog::debug("TTS synthesized: {} samples for guild {}",
                          stereo.size(),
                          static_cast<uint64_t>(req->guild_id));
        } catch (const std::exception& e) {
            spdlog::error("Worker TTS error: {}", e.what());
        }
    }
}

AudioCache::Stats SynthesizerPool::cache_stats() const {
    return cache_.stats();
}

} // namespace tts_bot
