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
        build_index(batch);
        start_console();
        print_statistics(start_time);
    }
    
    CrawlBatch App::crawl() {
        return crawler.run_crawler(file_path);
    }
    
    void App::sync_filesystem(const CrawlBatch &batch) {
        db.commit_filesystem_index(batch.all_files);
    }
    
    void App::build_index(const CrawlBatch &batch) {       
        pipeline.execute(batch.code_files, db); 
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