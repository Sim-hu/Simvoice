#include "audio/cache.hpp"

#include <functional>

namespace tts_bot {

AudioCache::AudioCache(size_t max_bytes) : max_bytes_(max_bytes) {}

size_t AudioCache::make_key(const std::string& text, uint32_t style_id) {
    size_t h = std::hash<std::string>{}(text);
    h ^= std::hash<uint32_t>{}(style_id) + 0x9e3779b9 + (h << 6) + (h >> 2);
    return h;
}

std::optional<std::vector<int16_t>> AudioCache::get(size_t key) {
    std::lock_guard lock(mutex_);

    auto it = map_.find(key);
    if (it == map_.end()) {
        ++misses_;
        return std::nullopt;
    }

    ++hits_;
    lru_.erase(it->second.lru_it);
    lru_.push_front(key);
    it->second.lru_it = lru_.begin();

    return it->second.pcm;
}

void AudioCache::put(size_t key, const std::vector<int16_t>& pcm) {
    std::lock_guard lock(mutex_);

    if (map_.contains(key)) return;

    size_t entry_bytes = pcm.size() * sizeof(int16_t);
    if (entry_bytes > max_bytes_) return;

    evict_if_needed(entry_bytes);

    lru_.push_front(key);
    map_[key] = {pcm, lru_.begin()};
    current_bytes_ += entry_bytes;
}

void AudioCache::evict_if_needed(size_t needed) {
    while (current_bytes_ + needed > max_bytes_ && !lru_.empty()) {
        size_t old_key = lru_.back();
        lru_.pop_back();

        auto it = map_.find(old_key);
        if (it != map_.end()) {
            current_bytes_ -= it->second.pcm.size() * sizeof(int16_t);
            map_.erase(it);
        }
    }
}

AudioCache::Stats AudioCache::stats() const {
    std::lock_guard lock(mutex_);
    return {hits_, misses_, map_.size(), current_bytes_};
}

} // namespace tts_bot
