#include <gtest/gtest.h>
#include "audio/cache.hpp"

using tts_bot::AudioCache;

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
    std::vector<int16_t> pcm = {1, 2, 3, 4};
    auto key = AudioCache::make_key("hello", 0);

    cache.put(key, pcm);
    auto result = cache.get(key);

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(*result, pcm);

    auto stats = cache.stats();
    EXPECT_EQ(stats.hits, 1);
    EXPECT_EQ(stats.entries, 1);
}

TEST(CacheTest, EvictsOldEntries) {
    AudioCache cache(32); // 32 bytes = 16 samples
    std::vector<int16_t> pcm1(8, 1); // 16 bytes
    std::vector<int16_t> pcm2(8, 2); // 16 bytes
    std::vector<int16_t> pcm3(8, 3); // 16 bytes

    cache.put(1, pcm1);
    cache.put(2, pcm2);
    cache.put(3, pcm3); // should evict pcm1

    EXPECT_FALSE(cache.get(1).has_value());
    EXPECT_TRUE(cache.get(2).has_value());
    EXPECT_TRUE(cache.get(3).has_value());
}

TEST(CacheTest, LruOrderRespected) {
    AudioCache cache(32);
    std::vector<int16_t> pcm(8, 0);

    cache.put(1, pcm);
    cache.put(2, pcm);
    cache.get(1); // touch key 1, making key 2 the LRU
    cache.put(3, pcm); // should evict key 2

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
    std::vector<int16_t> big(100, 0); // 200 bytes > 16
    cache.put(1, big);

    EXPECT_FALSE(cache.get(1).has_value());
    EXPECT_EQ(cache.stats().entries, 0);
}
