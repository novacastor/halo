#include "engine/engineApp.hpp"
#include "engine/log.hpp"

namespace Engine {
    App::App() : db(config.database), query_engine(db) {}

    App::~App() {
        pipeline.request_stop();
        if(index_worker.joinable()) {
            index_worker.join();
        }
    }

    bool App::init() {
        if(!db.init()) {
            LOG_ERROR("Database initialization Failed. ");
            return false;
        }
        if(!query_engine.init()) {
            LOG_ERROR("Query Engine initialization failed. ");
            return false;
        }
        if(!watcher.init()) {
            LOG_ERROR("File Watcher initialization failed");
            return false;
        }
        return true;
    }
    void App::build_search_index() {
        if(indexing_active.load()) return;

        indexing_active.store(true);
        
        if(index_worker.joinable()) {
            index_worker.join();
        }
        
        index_worker = std::thread([this]() {
            auto t0 = Clock::now();
            CrawlBatch batch = crawl();
            auto t1 = Clock::now();
            sync_filesystem(batch);
            auto t2 = Clock::now();
            build_index(batch);
            auto t3 = Clock::now();
            watcher.add_watchers(batch.all_directories);
            
            indexing_active.store(false);   
                       
            LOG_INFO("Crawl:            " + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count()) + " ms");
            LOG_INFO("Filesystem sync:  " + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count()) + " ms");
            LOG_INFO("Index build:      " + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(t3 - t2).count()) + " ms");
        });
    }
    void App::run() {
        auto start_time = Clock::now();
        build_search_index();
        print_statistics(start_time);
    }
    
    CrawlBatch App::crawl() {
        return crawler.run_crawler(config.root_directory);
    }
    
    void App::sync_filesystem(const CrawlBatch &batch) {
        db.commit_filesystem_index(batch.all_files);
    }
    
    void App::build_index(const CrawlBatch &batch) {       
        pipeline.execute(batch.code_files, db); 
    }

    void App::print_statistics(Clock::time_point start_time) {
        print_db_size(config.database);
        auto end_time = Clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time).count();
        
        LOG_INFO("Execution Time: " + std::to_string(duration) + " seconds");
    }
}