#include "database/database.hpp"
#include "database/databaseDebug.hpp"
#include "engine/config.hpp"
#include "engine/log.hpp"
#include "test_support.hpp"

#include <gtest/gtest.h>

#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>

TEST(Config, SuppliesDatabaseAndHomeDirectoryDefaults) {
    const Engine::Config config;

    EXPECT_EQ(config.database, "search_engine.db");
    EXPECT_EQ(config.root_directory, Engine::get_home_directory());
}

TEST(Log, WritesMessagesToConfiguredFile) {
    TestSupport::TemporaryDirectory root;
    const auto log_path = root.path() / "engine.log";

    Engine::Log::init(log_path.string());
    Engine::Log::write(Engine::LogLevel::Info, "test log entry");
    Engine::Log::shutdown();

    std::ifstream file(log_path);
    const std::string contents((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    EXPECT_NE(contents.find("[INFO] test log entry"), std::string::npos);
}

TEST(DatabaseDebug, PrintsIndexedRowsAndDatabaseFileSize) {
    TestSupport::TemporaryDirectory root;
    Engine::Database db(":memory:");
    ASSERT_TRUE(db.init());
    const int id = db.upsert_document("/root/debug.cpp", 1);
    ASSERT_GT(id, 0);
    ASSERT_TRUE(db.insert_tokens(id, {{"debugtoken", 7}}));

    std::ostringstream captured;
    auto* old_buffer = std::cerr.rdbuf(captured.rdbuf());
    Engine::print_inverted_index(db);

    const auto db_file = root.path() / "disk.db";
    {
        std::ofstream file(db_file);
        file << "database bytes";
    }
    Engine::print_db_size(db_file.string());
    std::cerr.rdbuf(old_buffer);

    EXPECT_NE(captured.str().find("debugtoken"), std::string::npos);
    EXPECT_NE(captured.str().find("/root/debug.cpp"), std::string::npos);
    EXPECT_NE(captured.str().find("DATABASE STORAGE METRICS"), std::string::npos);
}
