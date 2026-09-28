#include "indexing/tokenizer.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

TEST(Tokenizer, LowercasesSplitsAndTracksLines) {
    const auto tokens = Engine::Tokenizer::tokenize(
        "Hello, WORLD!\nquick brown fox\nint x = 7; _private");

    const std::vector<Engine::TokenMatch> expected = {
        {"hello", 1}, {"world", 1}, {"quick", 2},
            {"brown", 2}, {"fox", 2}, {"_private", 3}
    };

    ASSERT_EQ(tokens.size(), expected.size());
    for (std::size_t i = 0; i < expected.size(); ++i) {
        EXPECT_EQ(tokens[i].token, expected[i].token);
        EXPECT_EQ(tokens[i].line_number, expected[i].line_number);
    }
}

TEST(Tokenizer, OmitsStopWordsAndShortTokens) {
    const auto tokens = Engine::Tokenizer::tokenize("int x = 7; return hello");

    ASSERT_EQ(tokens.size(), 1);
    EXPECT_EQ(tokens.front().token, "hello");
}

TEST(Tokenizer, WhitespaceProducesNoTokens) {
    EXPECT_TRUE(Engine::Tokenizer::tokenize(" \n\t ").empty());
}

TEST(Tokenizer, HandlesNonAsciiBytesWithoutPassingNegativeCharsToCtype) {
    const std::string text = std::string("ascii ") + static_cast<char>(0xE9) + " word";
    const auto tokens = Engine::Tokenizer::tokenize(text);

    ASSERT_EQ(tokens.size(), 2);
    EXPECT_EQ(tokens[0].token, "ascii");
    EXPECT_EQ(tokens[1].token, "word");
}
