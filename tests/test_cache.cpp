#include <gtest/gtest.h>
#include "audio/cache.hpp"

using tts_bot::AudioCache;
using tts_bot::OpusFrames;

static OpusFrames make_frames(size_t n, size_t frame_size = 10) {
    OpusFrames f;
    for (size_t i = 0; i < n; ++i) {
        f.frames.emplace_back(frame_size, static_cast<uint8_t>(i));
        f.total_bytes += frame_size;
    }
    return f;
}

TEST(CacheTest, MissOnEmpty) {
    AudioCache cache(1024);
    auto result = cache.get(AudioCache::make_key("test", 0));
    EXPECT_FALSE(result.has_value());

    auto stats = cache.stats();
    EXPECT_EQ(stats.misses, 1);
    EXPECT_EQ(stats.hits, 0);
}

TEST(CacheTest, HitAfterPut) {
    AudioCache cache(1024 * 1024);
    auto frames = make_frames(5);
    auto key = AudioCache::make_key("hello", 0);

    cache.put(key, frames);
    auto result = cache.get(key);

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result->frames.size(), 5);

    auto stats = cache.stats();
    EXPECT_EQ(stats.hits, 1);
    EXPECT_EQ(stats.entries, 1);
}

TEST(CacheTest, EvictsOldEntries) {
    AudioCache cache(25); // 25 bytes
    auto f1 = make_frames(1, 10); // 10 bytes
    auto f2 = make_frames(1, 10);
    auto f3 = make_frames(1, 10);

    cache.put(1, f1);
    cache.put(2, f2);
    cache.put(3, f3); // evicts f1

    EXPECT_FALSE(cache.get(1).has_value());
    EXPECT_TRUE(cache.get(2).has_value());
    EXPECT_TRUE(cache.get(3).has_value());
}

TEST(CacheTest, LruOrderRespected) {
    AudioCache cache(25);
    auto f = make_frames(1, 10);

    cache.put(1, f);
    cache.put(2, f);
    cache.get(1); // touch key 1
    cache.put(3, f); // evicts key 2

    EXPECT_TRUE(cache.get(1).has_value());
    EXPECT_FALSE(cache.get(2).has_value());
    EXPECT_TRUE(cache.get(3).has_value());
}

TEST(CacheTest, DifferentSpeedPitchKeys) {
    auto k1 = AudioCache::make_key("hello", 0, 1.0f, 0.0f);
    auto k2 = AudioCache::make_key("hello", 0, 1.5f, 0.0f);
    auto k3 = AudioCache::make_key("hello", 0, 1.0f, 0.1f);
    EXPECT_NE(k1, k2);
    EXPECT_NE(k1, k3);
    EXPECT_NE(k2, k3);
}

TEST(CacheTest, RejectsOversizedEntry) {
    AudioCache cache(16);
    auto big = make_frames(10, 100); // 1000 bytes > 16
    cache.put(1, big);

    EXPECT_FALSE(cache.get(1).has_value());
    EXPECT_EQ(cache.stats().entries, 0);
}
