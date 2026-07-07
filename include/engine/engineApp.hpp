#pragma once 
#include "crawler/crawler.hpp"
#include "database/database.hpp"
#include "indexing/indexerPipeline.hpp"
#include "search/queryEngine.hpp"
#include <string>
#include <iostream>
#include <chrono>


using Clock = std::chrono::steady_clock;

namespace Engine {
    class App {
    public:
        App();
        bool init();
        void run();
    private:
        CrawlBatch crawl();
        void sync_filesystem(const CrawlBatch &batch);
        void build_index(const CrawlBatch &batch);
        void start_console();
        void print_statistics(Clock::time_point start_time);

        std::string db_name = "test.db";
        std::string file_path = "/home/salik/";

        Crawler crawler;
        Database db;    
        IndexerPipeline pipeline;
        QueryEngine query_engine;    
    };
}