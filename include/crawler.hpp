#pragma once
#include "database.hpp"
#include <string>
#include <thread>

namespace Engine {
    struct FSEntry {
        string name;
        string path;
        string ext;
    };

    struct CodeCandidate {
        string path;
        long long mtime;
    };

    struct CrawlBatch {
        vector<FSEntry> all_files;
        vector<CodeCandidate> code_files;
    };

    class Crawler {
    public:
        void run_crawler(const string &target_path, Database &db);
        CrawlBatch process_filesystem_crawl(const string &target_path);

    private:
        unordered_map<string, long long> existing_mtimes;
    };
}
