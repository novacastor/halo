#include "engine/engineApp.hpp"
#include "test_support.hpp"

#include <gtest/gtest.h>

TEST(EngineApp, InitializesAndBuildsSearchIndexForConfiguredRoot) {
    TestSupport::TemporaryDirectory root;
    root.write_file("src/app.cpp", "EngineApplicationSearchToken");
    root.write_file("notes.txt", "plain text file");
    root.write_file(".config/project/preferences.cpp", "ConfigRepositorySearchToken");
    root.write_file(".config/project/settings.json", "ConfigRepositoryIgnoredToken");
    const auto database_path = root.path() / "search.db";

    Engine::App app({database_path.string(), root.path().string()});
    ASSERT_TRUE(app.init());
    app.build_search_index();
    ASSERT_TRUE(TestSupport::wait_until([&] {
        return !app.is_indexing() &&
               !app.get_query_engine().search_terms("EngineApplicationSearchToken").empty();
    }));

    const auto matches = app.get_query_engine().search_terms("EngineApplicationSearchToken");
    ASSERT_EQ(matches.size(), 1);
    EXPECT_EQ(matches[0].file_path, (root.path() / "src/app.cpp").string());
    EXPECT_EQ(app.get_query_engine().search_filename("notes.txt").size(), 1);
    const auto config_repo_matches = app.get_query_engine().search_terms("ConfigRepositorySearchToken");
    ASSERT_EQ(config_repo_matches.size(), 1);
    EXPECT_EQ(config_repo_matches[0].file_path,
              (root.path() / ".config/project/preferences.cpp").string());
    EXPECT_TRUE(app.get_query_engine().search_terms("ConfigRepositoryIgnoredToken").empty());
    EXPECT_TRUE(app.get_query_engine().search_filename("settings.json").empty());

    root.write_file("root_level.cpp", "RootLevelWatcherToken");
    ASSERT_TRUE(TestSupport::wait_until([&] {
        return !app.get_query_engine().search_terms("RootLevelWatcherToken").empty();
    }));
}
