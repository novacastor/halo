#include "crawler/crawler.hpp"
#include "engine/fileTime.hpp"
#include "engine/log.hpp"
#include <filesystem>

namespace fs = std::filesystem;

Engine::CrawlBatch Engine::Crawler::process_filesystem_crawl(const std::string &target_path) {
    Engine::CrawlBatch batch;

    batch.all_files.reserve(10000);
    batch.code_files.reserve(1000);
    batch.all_directories.reserve(1000);

    fs::path root_path = fs::absolute(target_path).lexically_normal();
    batch.all_directories.push_back(root_path.string());

    std::string path, name, ext;
    
    for(auto it = fs::recursive_directory_iterator(root_path, fs::directory_options::skip_permission_denied); it != fs::end(it); ++it) {
        const auto &entry = *it;
        
        if(entry.is_directory()) {
            const auto native_name = entry.path().filename().native();
            std::string_view folder_name(native_name);
            
            if(Engine::FOLDER_BLACKLIST.find(folder_name) != Engine::FOLDER_BLACKLIST.end() ||
            Engine::GLOBAL_FOLDER_BLACKLIST.find(folder_name) != Engine::GLOBAL_FOLDER_BLACKLIST.end()) 
            {
                it.disable_recursion_pending();
                continue;
            }else {
                path = entry.path().string();
                batch.all_directories.push_back(path);
            }
        }
        
        if(!entry.is_regular_file()) continue;
        
        path = entry.path().string();
        name = entry.path().filename().string();
        ext = entry.path().extension().string();

        if (!check_file(name, ext)) continue;

        batch.all_files.push_back({name, path, ext});
        
        long long mtime = Engine::file_time_to_unix_nanoseconds(entry.last_write_time());
        
        batch.code_files.push_back({path, mtime});
    }
    return batch;
}

Engine::CrawlBatch Engine::Crawler::run_crawler(const std::string &target_path) {
    Engine::CrawlBatch batch = process_filesystem_crawl(target_path);
    LOG_INFO("FileSystem Crawl Complete. ");
    return batch;
}
