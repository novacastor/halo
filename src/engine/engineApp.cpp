#include "engine/engineApp.hpp"

namespace Engine {
    App::App() : db(db_name), query_engine(db) {}

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

    void App::run() {
        auto start_time = Clock::now();
        
        CrawlBatch batch = crawl();
        sync_filesystem(batch);
        bool rebuild_required = build_index(batch);
        optimize_indexes(rebuild_required);
        start_console();
        print_statistics(start_time);
    }
    
    CrawlBatch App::crawl() {
        return crawler.run_crawler(file_path);
    }
    
    void App::sync_filesystem(const CrawlBatch &batch) {
        db.commit_filesystem_index(batch.all_files);
    }
    
    bool App::build_index(const CrawlBatch &batch) {       
        return pipeline.execute(batch.code_files, db); 
    }
    
    void App::optimize_indexes(bool rebuild_required) {
        if(rebuild_required) {
            auto t_start = Clock::now();
            db.optimize_search_indexes();
            auto t_end = Clock::now();
            
            std::cout << "Index build: " << std::chrono::duration_cast<std::chrono::milliseconds>(t_end - t_start).count() / 1000.0 << " s\n";
        } else {
            std::cout << "\nSearch indexes up to date. Skipping optimization pass.\n";
        }
    }
    
    void App::start_console() {
        query_engine.run();
    }
    
    void App::print_statistics(Clock::time_point start_time) {
        print_db_size(db_name);
        auto end_time = Clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time).count();
        
        std::cout << "Execution Time: " << duration << " seconds.\n";
    }
}