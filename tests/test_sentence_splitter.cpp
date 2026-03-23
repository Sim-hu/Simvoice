#include <gtest/gtest.h>
#include "tts/sentence_splitter.hpp"

using tts_bot::split_sentences;

TEST(SplitterTest, EmptyString) {
    auto r = split_sentences("");
    EXPECT_EQ(r.size(), 1);
}

TEST(SplitterTest, ShortTextNotSplit) {
    auto r = split_sentences("こんにちは");
    EXPECT_EQ(r.size(), 1);
    EXPECT_EQ(r[0], "こんにちは");
}

TEST(SplitterTest, SplitOnPeriod) {
    auto r = split_sentences("最初の文。二番目の文。三番目の文。", 1);
    EXPECT_EQ(r.size(), 3);
    EXPECT_EQ(r[0], "最初の文。");
    EXPECT_EQ(r[1], "二番目の文。");
    EXPECT_EQ(r[2], "三番目の文。");
}

TEST(SplitterTest, SplitOnQuestion) {
    auto r = split_sentences("元気ですか？はい！", 1);
    EXPECT_EQ(r.size(), 2);
}

TEST(SplitterTest, SplitOnNewline) {
    auto r = split_sentences("一行目\n二行目\n三行目", 1);
    EXPECT_EQ(r.size(), 3);
}

TEST(SplitterTest, TrailingText) {
    auto r = split_sentences("文。残り", 1);
    EXPECT_EQ(r.size(), 2);
    EXPECT_EQ(r[0], "文。");
    EXPECT_EQ(r[1], "残り");
}
