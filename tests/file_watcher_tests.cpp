#include "database/database.hpp"
#include "indexing/indexerPipeline.hpp"
#include "search/queryEngine.hpp"
#include "test_support.hpp"
#include "watcher/fileWatcher.hpp"

#include <gtest/gtest.h>

#include <filesystem>

TEST(FileWatcher, WatchesFileCreationAndDeletion) {
    TestSupport::TemporaryDirectory root;
    Engine::Database db(":memory:");
    ASSERT_TRUE(db.init());
    Engine::IndexerPipeline pipeline(db);
    Engine::FileWatcher watcher(db, pipeline);
    ASSERT_TRUE(watcher.init());
    ASSERT_TRUE(watcher.add_watchers({root.path().string()}));
    Engine::QueryEngine query(db);

    const auto watched_file = root.path() / "watched.cpp";
    {
        std::ofstream file(watched_file);
        file << "WatchedCreationToken";
    }

    ASSERT_TRUE(TestSupport::wait_until([&] {
        return !query.search_filename("watched.cpp").empty() &&
               !query.search_terms("WatchedCreationToken").empty();
    }));

    std::filesystem::remove(watched_file);
    ASSERT_TRUE(TestSupport::wait_until([&] {
        return query.search_filename("watched.cpp").empty() &&
               query.search_terms("WatchedCreationToken").empty();
    }));

    watcher.remove_watchers();
}

TEST(FileWatcher, ReportsFailureForDirectoryThatCannotBeWatched) {
    TestSupport::TemporaryDirectory root;
    Engine::Database db(":memory:");
    ASSERT_TRUE(db.init());
    Engine::IndexerPipeline pipeline(db);
    Engine::FileWatcher watcher(db, pipeline);
    ASSERT_TRUE(watcher.init());

    EXPECT_FALSE(watcher.add_watchers({(root.path() / "missing").string()}));
}
