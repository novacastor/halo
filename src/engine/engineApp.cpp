#include "engine/engineApp.hpp"

namespace Engine {
    App::App() : db(config.database), query_engine(db) {}

    App::~App() {
        if(index_worker.joinable()) {
            index_worker.join();
        }
    }

    bool App::init() {
        if(!db.init()) {
            std::cout << "db initialization failed" << std::endl;
            return false;
        }
        if(!query_engine.init()) {
            std::cout << "query_engine initalization failed" << std::endl;
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
            CrawlBatch batch = crawl();
            sync_filesystem(batch);
            build_index(batch);

            indexing_active.store(false);        
        });

        // auto t0 = Clock::now();
        // auto t1 = Clock::now();
        // auto t2 = Clock::now();
        // auto t3 = Clock::now();
        
        // std::cout << "Crawl:            " << std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count() << " ms\n";
        // std::cout << "Filesystem sync:  " << std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count() << " ms\n";
        // std::cout << "Index build:      " << std::chrono::duration_cast<std::chrono::milliseconds>(t3 - t2).count() << " ms\n";
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
        
        std::cout << "Execution Time: " << duration << " seconds.\n";
    }
}