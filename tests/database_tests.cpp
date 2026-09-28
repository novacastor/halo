#include "database/database.hpp"
#include "test_support.hpp"

#include <gtest/gtest.h>

TEST(Database, InitializationFailsWhenDatabaseParentDoesNotExist) {
    TestSupport::TemporaryDirectory root;
    Engine::Database db((root.path() / "missing" / "index.db").string());

    EXPECT_FALSE(db.init());
}

TEST(Database, FilesystemIndexSupportsCommitDeleteAndDirectoryBoundaries) {
    Engine::Database db(":memory:");
    ASSERT_TRUE(db.init());

    db.commit_filesystem_index({
        {"alpha.cpp", "/root/project/alpha.cpp", ".cpp"},
        {"beta.cpp", "/root/project/src/beta.cpp", ".cpp"},
        {"project2.cpp", "/root/project2/project2.cpp", ".cpp"}
    });
    EXPECT_EQ(db.execute_filename_search(".cpp").size(), 3);

    ASSERT_TRUE(db.delete_file("/root/project/alpha.cpp"));
    EXPECT_EQ(db.execute_filename_search("alpha").size(), 0);

    ASSERT_TRUE(db.delete_directory("/root/project"));
    EXPECT_EQ(db.execute_filename_search("beta").size(), 0);
    EXPECT_EQ(db.execute_filename_search("project2").size(), 1);

    db.commit_filesystem_index({});
    EXPECT_EQ(db.execute_filename_search("project2").size(), 1);
}

TEST(Database, DirectoryCanBeAddedAndDeletedFromFilesystemIndex) {
    Engine::Database db(":memory:");
    ASSERT_TRUE(db.init());
    ASSERT_TRUE(db.add_directory("/root/project/new-folder"));

    const auto directories = db.execute_filename_search("");
    ASSERT_EQ(directories.size(), 1);
    EXPECT_EQ(directories[0].file_path, "/root/project/new-folder");

    ASSERT_TRUE(db.delete_directory("/root/project/new-folder"));
    EXPECT_TRUE(db.execute_filename_search("").empty());
    EXPECT_FALSE(db.delete_directory(""));
}

TEST(Database, ReindexingDocumentReplacesOldTokensAndUpdatesMtime) {
    Engine::Database db(":memory:");
    ASSERT_TRUE(db.init());

    const int first_id = db.upsert_document("/root/project/file.cpp", 10);
    ASSERT_GT(first_id, 0);
    ASSERT_TRUE(db.insert_tokens(first_id, {{"oldtoken", 1}}));
    EXPECT_TRUE(db.file_is_up_to_date("/root/project/file.cpp", 10));

    const int second_id = db.upsert_document("/root/project/file.cpp", 20);
    EXPECT_EQ(second_id, first_id);
    ASSERT_TRUE(db.insert_tokens(second_id, {{"newtoken", 3}}));

    EXPECT_TRUE(db.execute_terms_search({{"oldtoken", 1}}).empty());
    const auto new_matches = db.execute_terms_search({{"newtoken", 1}});
    ASSERT_EQ(new_matches.size(), 1);
    EXPECT_EQ(new_matches[0].line_number, 3);

    sqlite3_stmt* statement = nullptr;
    ASSERT_EQ(sqlite3_prepare_v2(db.get_db_handle(),
        "SELECT mtime FROM documents WHERE file_path = ?", -1, &statement, nullptr), SQLITE_OK);
    sqlite3_bind_text(statement, 1, "/root/project/file.cpp", -1, SQLITE_STATIC);
    ASSERT_EQ(sqlite3_step(statement), SQLITE_ROW);
    EXPECT_EQ(sqlite3_column_int64(statement, 0), 20);
    sqlite3_finalize(statement);
}

TEST(Database, BulkIndexRestoresSearchIndexesAndPersistsDocuments) {
    TestSupport::TemporaryDirectory root;
    const auto database_path = root.path() / "persistent.db";
    {
        Engine::Database db(database_path.string());
        ASSERT_TRUE(db.init());
        db.begin_bulk_index();
        const int id = db.upsert_document("/root/bulk.cpp", 77);
        ASSERT_GT(id, 0);
        ASSERT_TRUE(db.insert_tokens(id, {{"bulkindextoken", 5}}));
        db.end_bulk_index();
        EXPECT_EQ(db.execute_terms_search({{"bulkindextoken", 1}}).size(), 1);
    }

    Engine::Database reopened(database_path.string());
    ASSERT_TRUE(reopened.init());
    EXPECT_TRUE(reopened.file_is_up_to_date("/root/bulk.cpp", 77));
    EXPECT_EQ(reopened.execute_terms_search({{"bulkindextoken", 1}}).size(), 1);
}

TEST(Database, DirectoryDeletionRemovesDocumentsButNotSiblingPrefixes) {
    Engine::Database db(":memory:");
    ASSERT_TRUE(db.init());

    const int project_id = db.upsert_document("/root/project/file.cpp", 1);
    const int sibling_id = db.upsert_document("/root/project-old/file.cpp", 1);
    ASSERT_TRUE(db.insert_tokens(project_id, {{"projecttoken", 1}}));
    ASSERT_TRUE(db.insert_tokens(sibling_id, {{"siblingtoken", 1}}));

    ASSERT_TRUE(db.delete_documents_under_directory("/root/project"));
    EXPECT_TRUE(db.execute_terms_search({{"projecttoken", 1}}).empty());
    EXPECT_EQ(db.execute_terms_search({{"siblingtoken", 1}}).size(), 1);
    EXPECT_FALSE(db.file_is_up_to_date("/root/project/file.cpp", 1));
    EXPECT_TRUE(db.file_is_up_to_date("/root/project-old/file.cpp", 1));
}

TEST(Database, DirectoryDeletionEscapesLikeWildcardsInPaths) {
    Engine::Database db(":memory:");
    ASSERT_TRUE(db.init());
    const int literal_id = db.upsert_document("/root/proj_ect/file.cpp", 1);
    const int sibling_id = db.upsert_document("/root/projXect/file.cpp", 1);
    ASSERT_GT(literal_id, 0);
    ASSERT_GT(sibling_id, 0);
    ASSERT_TRUE(db.insert_tokens(literal_id, {{"literalpathword", 1}}));
    ASSERT_TRUE(db.insert_tokens(sibling_id, {{"siblingpathword", 1}}));
    ASSERT_TRUE(db.insert_file("file.cpp", ".cpp", "/root/proj_ect/file.cpp"));
    ASSERT_TRUE(db.insert_file("file.cpp", ".cpp", "/root/projXect/file.cpp"));

    ASSERT_TRUE(db.delete_documents_under_directory("/root/proj_ect"));
    ASSERT_TRUE(db.delete_directory("/root/proj_ect"));
    EXPECT_TRUE(db.execute_terms_search({{"literalpathword", 1}}).empty());
    EXPECT_EQ(db.execute_terms_search({{"siblingpathword", 1}}).size(), 1);
    EXPECT_EQ(db.execute_filename_search("file.cpp").size(), 1);
    EXPECT_FALSE(db.delete_documents_under_directory(""));
}
