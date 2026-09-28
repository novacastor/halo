#include "database/database.hpp"
#include "indexing/indexerPipeline.hpp"
#include "search/queryEngine.hpp"
#include "test_support.hpp"

#include <gtest/gtest.h>

TEST(IndexerPipeline, ExecuteIndexesSupportedFilesAndTracksCounts) {
    TestSupport::TemporaryDirectory root;
    const auto source = root.write_file("main.cpp", "UniqueIndexerSymbol appears here\nsecond line");
    Engine::Database db(":memory:");
    ASSERT_TRUE(db.init());

    {
        Engine::IndexerPipeline pipeline(db);
        pipeline.execute({{source.string(), 100}});

        EXPECT_EQ(pipeline.get_files_total(), 1);
        Engine::QueryEngine query(db);
        EXPECT_EQ(query.search_terms("UniqueIndexerSymbol").size(), 1);
        EXPECT_EQ(pipeline.get_files_indexed(), 1);
        EXPECT_GE(pipeline.get_read_time_s(), 0.0);
        EXPECT_GE(pipeline.get_tokenize_time_s(), 0.0);
        EXPECT_GE(pipeline.get_db_time_s(), 0.0);
    }
}

TEST(IndexerPipeline, SkipsUnchangedFilesAndEmptyOrMissingFiles) {
    TestSupport::TemporaryDirectory root;
    const auto source = root.write_file("stable.cpp", "StablePipelineToken");
    const auto empty = root.write_file("empty.cpp", "");
    const auto missing = root.path() / "missing.cpp";
    Engine::Database db(":memory:");
    ASSERT_TRUE(db.init());

    {
        Engine::IndexerPipeline pipeline(db);
        pipeline.execute({{source.string(), 101}});
        Engine::QueryEngine query(db);
        ASSERT_TRUE(TestSupport::wait_until([&] {
            return !query.search_terms("StablePipelineToken").empty();
        }));

        pipeline.execute({{source.string(), 101}, {empty.string(), 102}, {missing.string(), 103}});
        EXPECT_EQ(pipeline.get_files_total(), 2);
        EXPECT_EQ(pipeline.get_files_indexed(), 0);
        EXPECT_EQ(query.search_terms("StablePipelineToken").size(), 1);
        EXPECT_TRUE(query.search_terms("missing").empty());
    }
}

TEST(IndexerPipeline, ReindexingReplacesPriorContent) {
    TestSupport::TemporaryDirectory root;
    const auto source = root.write_file("change.cpp", "OriginalPipelineWord");
    Engine::Database db(":memory:");
    ASSERT_TRUE(db.init());
    Engine::QueryEngine query(db);

    {
        Engine::IndexerPipeline pipeline(db);
        pipeline.execute({{source.string(), 1}});
        ASSERT_TRUE(TestSupport::wait_until([&] {
            return !query.search_terms("OriginalPipelineWord").empty();
        }));

        root.write_file("change.cpp", "ReplacementPipelineWord");
        pipeline.execute({{source.string(), 2}});

        ASSERT_TRUE(TestSupport::wait_until([&] {
            return !query.search_terms("ReplacementPipelineWord").empty();
        }));
        EXPECT_TRUE(query.search_terms("OriginalPipelineWord").empty());
    }
}
