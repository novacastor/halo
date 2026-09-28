#include "database/database.hpp"
#include "search/queryEngine.hpp"

#include <gtest/gtest.h>

TEST(QueryEngine, SearchesAllTermsOnSameLineAndReturnsScore) {
    Engine::Database db(":memory:");
    ASSERT_TRUE(db.init());

    const int document_id = db.upsert_document("/workspace/src/alpha.cpp", 42);
    ASSERT_GT(document_id, 0);
    ASSERT_TRUE(db.insert_tokens(document_id, {{"alpha", 2}, {"searchable", 2}, {"beta", 4}}));

    Engine::QueryEngine query(db);
    const auto matches = query.search_terms("ALPHA searchable");

    ASSERT_EQ(matches.size(), 1);
    EXPECT_EQ(matches[0].file_path, "/workspace/src/alpha.cpp");
    EXPECT_EQ(matches[0].line_number, 2);
    EXPECT_EQ(matches[0].score, 250);
    EXPECT_TRUE(query.search_terms("alpha beta").empty());
    EXPECT_EQ(query.search_terms("searchable alpha searchable").size(), 1);
}

TEST(QueryEngine, EmptyAndFilteredTermQueriesReturnNoMatches) {
    Engine::Database db(":memory:");
    ASSERT_TRUE(db.init());
    Engine::QueryEngine query(db);

    EXPECT_TRUE(query.search_terms("").empty());
    EXPECT_TRUE(query.search_terms("int").empty());
}

TEST(QueryEngine, FilenameSearchTreatsPatternCharactersLiterally) {
    Engine::Database db(":memory:");
    ASSERT_TRUE(db.init());
    ASSERT_TRUE(db.insert_file("alpha_search.cpp", ".cpp", "/workspace/src/alpha_search.cpp"));
    ASSERT_TRUE(db.insert_file("alphaXsearch.cpp", ".cpp", "/workspace/src/alphaXsearch.cpp"));

    Engine::QueryEngine query(db);
    const auto matches = query.search_filename("alpha_");

    ASSERT_EQ(matches.size(), 1);
    EXPECT_EQ(matches[0].file_name, "alpha_search.cpp");
    EXPECT_TRUE(query.search_filename("").empty());
}

TEST(QueryEngine, DeletedDocumentsAreNoLongerSearchable) {
    Engine::Database db(":memory:");
    ASSERT_TRUE(db.init());
    const int document_id = db.upsert_document("/workspace/src/alpha.cpp", 42);
    ASSERT_GT(document_id, 0);
    ASSERT_TRUE(db.insert_tokens(document_id, {{"alpha", 1}}));

    Engine::QueryEngine query(db);
    ASSERT_FALSE(query.search_terms("alpha").empty());
    ASSERT_TRUE(db.delete_document("/workspace/src/alpha.cpp"));
    EXPECT_TRUE(query.search_terms("alpha").empty());
}
