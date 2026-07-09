#include "crawler/crawler.hpp"
#include "engine/log.hpp"
#include <filesystem>
#include <chrono>

namespace fs = std::filesystem;

Engine::CrawlBatch Engine::Crawler::process_filesystem_crawl(const std::string &target_path) {
    Engine::CrawlBatch batch;

    batch.all_files.reserve(10000);
    batch.code_files.reserve(1000);

    fs::path root_path(target_path);

    std::string path, name, ext;
    
    for(auto it = fs::recursive_directory_iterator(root_path, fs::directory_options::skip_permission_denied); it != fs::end(it); ++it) {
        const auto &entry = *it;
        
        if(entry.is_directory()) {
            const auto& native_name = entry.path().filename().native();
            std::string_view folder_name(native_name.c_str(), native_name.length());
            
            if(FOLDER_BLACKLIST.find(folder_name) != FOLDER_BLACKLIST.end() || GLOBAL_FOLDER_BLACKLIST.find(folder_name) != GLOBAL_FOLDER_BLACKLIST.end()) {
                it.disable_recursion_pending();
                continue;
            }
        }
        
        if(!entry.is_regular_file()) continue;
        
        path = entry.path().string();
        name = entry.path().filename().string();
        ext = entry.path().extension().string();

        batch.all_files.push_back({name, path, ext});

        if(EXTENSION_WHITELIST.find(ext) == EXTENSION_WHITELIST.end()) continue;
        
        auto ftime = entry.last_write_time();
        long long mtime = std::chrono::duration_cast<std::chrono::seconds>(fs::file_time_type::clock::to_sys(ftime).time_since_epoch()).count();
        
        batch.code_files.push_back({path, mtime});
    }
    return batch;
}

Engine::CrawlBatch Engine::Crawler::run_crawler(const std::string &target_path) {
    Engine::CrawlBatch batch = process_filesystem_crawl(target_path);
    LOG_INFO("File System Crawl Complete. ");
    return batch;
}

/*
cmake --build build
./build/search_engine
*/