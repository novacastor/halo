#pragma once 
#include "crawler/crawler.hpp"
#include "database/database.hpp"
#include "indexing/indexerPipeline.hpp"
#include "search/queryEngine.hpp"
#include "database/databaseDebug.hpp"
#include "engine/config.hpp"
#include <string>
#include <iostream>
#include <chrono>


using Clock = std::chrono::steady_clock;

namespace Engine {
    class App {
    public:
        App();
        ~App();
        bool init();
        void run();
        void build_search_index();
        
        bool is_indexing() const { return indexing_active.load(); }
        QueryEngine& get_query_engine() { return query_engine; }
        IndexerPipeline& get_pipeline() { return pipeline; }

        std::string get_target_path() const { return config.root_directory; }
        std::string get_db_name() const { return config.database; }
    private:
        CrawlBatch crawl();
        void sync_filesystem(const CrawlBatch &batch);
        void build_index(const CrawlBatch &batch);
        void start_console();
        void print_statistics(Clock::time_point start_time);

        std::atomic<bool> indexing_active{false};
        std::thread index_worker;
        
        Config config;
        Crawler crawler;
        Database db;    
        IndexerPipeline pipeline;
        QueryEngine query_engine;    
    };
}