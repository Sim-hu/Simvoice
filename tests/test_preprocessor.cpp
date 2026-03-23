#include <gtest/gtest.h>
#include "tts/preprocessor.hpp"
#include "db/database.hpp"

using tts_bot::TextPreprocessor;

class PreprocessorTest : public ::testing::Test {
protected:
    TextPreprocessor pp;
};

TEST_F(PreprocessorTest, RemovesUrls) {
    auto result = pp.process("check https://example.com ok");
    EXPECT_EQ(result, "check URL省略 ok");
}

TEST_F(PreprocessorTest, RemovesMultipleUrls) {
    auto result = pp.process("a http://x.com b https://y.com c");
    EXPECT_EQ(result, "a URL省略 b URL省略 c");
}

TEST_F(PreprocessorTest, ConvertsMentions) {
    auto result = pp.process("hello <@123456> and <@!789>");
    EXPECT_EQ(result, "hello 誰か and 誰か");
}

TEST_F(PreprocessorTest, ConvertsChannelMentions) {
    auto result = pp.process("see <#123456>");
    EXPECT_EQ(result, "see どこか");
}

TEST_F(PreprocessorTest, RemovesCustomEmoji) {
    auto result = pp.process("nice <:pepehands:123456>");
    EXPECT_EQ(result, "nice");
}

TEST_F(PreprocessorTest, RemovesAnimatedEmoji) {
    auto result = pp.process("wow <a:partyblob:789>");
    EXPECT_EQ(result, "wow");
}

TEST_F(PreprocessorTest, RemovesEmojiShortcodes) {
    auto result = pp.process("hello :thumbsup: world");
    EXPECT_EQ(result, "hello world");
}

TEST_F(PreprocessorTest, NormalizesWhitespace) {
    auto result = pp.process("hello   \n\n  world");
    EXPECT_EQ(result, "hello world");
}

TEST_F(PreprocessorTest, TruncatesLongText) {
    std::string long_text(500, 'a');
    auto result = pp.process(long_text, {}, 10);
    EXPECT_TRUE(result.size() < long_text.size());
    EXPECT_TRUE(result.find("以下省略") != std::string::npos);
}

TEST_F(PreprocessorTest, EmptyAfterProcessing) {
    auto result = pp.process(":thumbsup:");
    EXPECT_TRUE(result.empty());
}

TEST_F(PreprocessorTest, ConvertsNumbers) {
    auto result = pp.process("test 42 end");
    EXPECT_EQ(result, "test 四十二 end");
}

TEST_F(PreprocessorTest, ConvertsLargeNumbers) {
    auto result = pp.process("price 1500");
    EXPECT_EQ(result, "price 一千五百");
}

TEST_F(PreprocessorTest, KeepsFiveDigitNumbers) {
    auto result = pp.process("id 12345");
    EXPECT_EQ(result, "id 12345");
}

TEST_F(PreprocessorTest, AppliesDict) {
    std::vector<tts_bot::DictEntry> dict = {
        {"w", "わら", 5},
        {"おk", "おーけー", 5},
    };
    auto result = pp.process("おk w", dict);
    EXPECT_EQ(result, "おーけー わら");
}
