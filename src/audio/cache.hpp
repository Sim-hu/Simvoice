#pragma once

#include "audio/pipeline.hpp"

#include <cstddef>
#include <cstdint>
#include <list>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace tts_bot {

class AudioCache {
public:
    explicit AudioCache(size_t max_bytes);

    static size_t make_key(const std::string& text, uint32_t style_id,
                           float speed = 1.0f, float pitch = 0.0f);

    std::optional<OpusFrames> get(size_t key);
    void put(size_t key, const OpusFrames& frames);

    struct Stats {
        uint64_t hits = 0;
        uint64_t misses = 0;
        size_t entries = 0;
        size_t memory_bytes = 0;
    };
    Stats stats() const;

private:
    struct Entry {
        OpusFrames frames;
        std::list<size_t>::iterator lru_it;
    };

    void evict_if_needed(size_t needed);

    size_t max_bytes_;
    size_t current_bytes_ = 0;
    uint64_t hits_ = 0;
    uint64_t misses_ = 0;

    std::unordered_map<size_t, Entry> map_;
    std::list<size_t> lru_;
    mutable std::mutex mutex_;
};

} // namespace tts_bot
