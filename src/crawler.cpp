#include "crawler.hpp"
#include "pipeline.hpp"
#include <iostream>
#include <filesystem>
#include <unordered_set>
#include <chrono>

using namespace std;
using Clock = chrono::steady_clock;

namespace fs = std::filesystem;

const unordered_set<string> EXTENSION_WHITELIST = {
    ".cpp", ".hpp", ".h", ".c", ".cc", ".cxx",
    ".py", ".sh", ".bash", ".lua",
    ".js", ".jsx", ".ts", ".tsx", ".html", ".css",
    ".json", ".yaml", ".yml", ".toml", ".xml", ".ini",
    ".md", ".txt", "dotfiles" 
};

const unordered_set<string> FOLDER_BLACKLIST = {
    ".git", ".svn", ".hg", ".vscode", ".idea",
    "build", "cmake-build-debug", "cmake-build-release", "cmake-build-relwithdebinfo", "cmake-build-minsizerel", "out", "dist", "target",
    "node_modules", ".npm", ".pnpm-store", ".yarn",
    "__pycache__", ".pytest_cache", ".mypy_cache", ".ruff_cache", ".tox", ".venv", "venv", "env",
    ".cargo", ".rustup", ".gradle", ".m2", ".cache", ".docker", ".vagrant", ".local", ".Trash", ".sass-cache"
};

const unordered_set<string> GLOBAL_FOLDER_BLACKLIST = { 
    ".cache", ".Trash", ".local/share" 
};


void commit_filesystem_index(vector<Engine::FSEntry> &files, Engine::Database &db) {
    if(files.empty()) return;

    sqlite3_exec(db.get_db_handle(), "BEGIN;", nullptr, nullptr, nullptr);

    for(const auto &file: files) {
        db.insert_file(file.name, file.ext, file.path);
    }

    sqlite3_exec(db.get_db_handle(), "COMMIT;", nullptr, nullptr, nullptr);
}

Engine::CrawlBatch Engine::Crawler::process_filesystem_crawl(const string &target_path) {
    Engine::CrawlBatch batch;

    batch.all_files.reserve(10000);
    batch.code_files.reserve(1000);

    fs::path root_path(target_path);

    string path, name, ext;
    
    for(auto it = fs::recursive_directory_iterator(root_path, fs::directory_options::skip_permission_denied); it != fs::end(it); ++it) {
        const auto &entry = *it;
        
        if(entry.is_directory()) {
            string folder_name = entry.path().filename().string();
            if(FOLDER_BLACKLIST.find(folder_name) != FOLDER_BLACKLIST.end() || GLOBAL_FOLDER_BLACKLIST.find(folder_name) != FOLDER_BLACKLIST.end()) {
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
        long long mtime = chrono::duration_cast<chrono::seconds>(fs::file_time_type::clock::to_sys(ftime).time_since_epoch()).count();
        
        batch.code_files.push_back({path, mtime});
    }
    return batch;
}

void Engine::Crawler::run_crawler(const string &target_path, Engine::Database &db) {
    db.load_existing_mtimes(this->existing_mtimes);

    Engine::CrawlBatch batch = process_filesystem_crawl(target_path);
    db.commit_filesystem_index(batch.all_files);

    Engine::IndexerPipeline pipeline;
    bool database_muteated = pipeline.execute(batch.code_files, db, existing_mtimes);
    
    if(database_muteated) {
        cout << endl << "Optimizing search indexes..." << endl;
        
        auto t_start = Clock::now();
        db.optimize_search_indexes();
        auto t_end = Clock::now();
        
        cout << "Index build: " << chrono::duration_cast<chrono::milliseconds>(t_end - t_start).count() / 1000.0 << " s\n";
    }else{
        cout << "\nSearch indexes up to date. Skipping optimization pass.\n";
    }
}

/*
cmake --build build
./build/search_engine
*/