#include <gtest/gtest.h>
#include "guild/queue.hpp"

#include <thread>
#include <atomic>

using tts_bot::GuildQueue;
using tts_bot::TTSRequest;

TEST(QueueTest, PushAndPop) {
    GuildQueue q;
    EXPECT_TRUE(q.empty());

    q.push({.text = "hello", .style_id = 0});

    EXPECT_EQ(q.size(), 1);
    EXPECT_FALSE(q.empty());

    TTSRequest req;
    EXPECT_TRUE(q.try_pop(req));
    EXPECT_EQ(req.text, "hello");
    EXPECT_TRUE(q.empty());
}

TEST(QueueTest, Fifo) {
    GuildQueue q;
    q.push({.text = "first"});
    q.push({.text = "second"});
    q.push({.text = "third"});

    TTSRequest req;
    q.try_pop(req); EXPECT_EQ(req.text, "first");
    q.try_pop(req); EXPECT_EQ(req.text, "second");
    q.try_pop(req); EXPECT_EQ(req.text, "third");
}

TEST(QueueTest, PopFromEmptyReturnsFalse) {
    GuildQueue q;
    TTSRequest req;
    EXPECT_FALSE(q.try_pop(req));
}

TEST(QueueTest, ThreadSafety) {
    GuildQueue q;
    std::atomic<int> produced{0};
    std::atomic<int> consumed{0};
    constexpr int N = 1000;

    auto producer = [&]() {
        for (int i = 0; i < N; ++i) {
            q.push({.text = std::to_string(i)});
            ++produced;
        }
    };

    auto consumer = [&]() {
        TTSRequest req;
        while (consumed < N * 2) {
            if (q.try_pop(req)) {
                ++consumed;
            } else {
                std::this_thread::yield();
            }
        }
    };

    std::thread p1(producer), p2(producer);
    std::thread c(consumer);

    p1.join();
    p2.join();
    c.join();

    EXPECT_EQ(produced.load(), N * 2);
    EXPECT_EQ(consumed.load(), N * 2);
    EXPECT_TRUE(q.empty());
}
