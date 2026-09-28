#include "crawler/crawler.hpp"
#include "test_support.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>

TEST(Crawler, CollectsFilesDirectoriesAndWhitelistedCodeFiles) {
    TestSupport::TemporaryDirectory root;
    root.write_file("src/main.cpp", "int main() {}");
    root.write_file("README.md", "project notes");
    root.write_file("image.png", "not source");
    root.write_file("build/generated.cpp", "ignored build output");

    Engine::Crawler crawler;
    const auto batch = crawler.process_filesystem_crawl(root.path().string());

    ASSERT_EQ(batch.all_files.size(), 3);
    ASSERT_EQ(batch.code_files.size(), 2);
    EXPECT_EQ(batch.all_directories.size(), 2);
    EXPECT_TRUE(std::find(batch.all_directories.begin(), batch.all_directories.end(),
                          root.path().string()) != batch.all_directories.end());

    const auto has_file = [&](const std::string& suffix) {
        return std::any_of(batch.all_files.begin(), batch.all_files.end(), [&](const auto& file) {
            return file.path.ends_with(suffix);
        });
    };
    EXPECT_TRUE(has_file("/src/main.cpp"));
    EXPECT_TRUE(has_file("/README.md"));
    EXPECT_TRUE(has_file("/image.png"));
    EXPECT_FALSE(has_file("/build/generated.cpp"));

    EXPECT_TRUE(std::any_of(batch.code_files.begin(), batch.code_files.end(), [](const auto& candidate) {
        return candidate.path.ends_with("/src/main.cpp") && candidate.mtime != 0;
    }));
    EXPECT_TRUE(std::any_of(batch.code_files.begin(), batch.code_files.end(), [](const auto& candidate) {
        return candidate.path.ends_with("/README.md");
    }));
}

TEST(Crawler, ExtensionWhitelistRecognizesSupportedAndUnsupportedExtensions) {
    Engine::Crawler crawler;

    EXPECT_TRUE(crawler.check_extension(".cpp"));
    EXPECT_TRUE(crawler.check_extension(".py"));
    EXPECT_FALSE(crawler.check_extension(".png"));
    EXPECT_FALSE(crawler.check_extension("CPP"));
}

TEST(Crawler, MissingRootRaisesFilesystemError) {
    TestSupport::TemporaryDirectory root;
    Engine::Crawler crawler;

    EXPECT_THROW(crawler.process_filesystem_crawl((root.path() / "missing").string()),
                 std::filesystem::filesystem_error);
}
