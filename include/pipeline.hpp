#pragma once
#include "concurrentQueue.hpp"
#include "crawler.hpp"
#include "database.hpp"
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <unordered_map>
using namespace std;

namespace Engine {
    class IndexerPipeline {
    public:
        IndexerPipeline() = default;
        ~IndexerPipeline() = default;

        bool execute(vector<Engine::CodeCandidate> &code_candidates, Engine::Database &db, const unordered_map<string, long long> &mtimes);

        double get_read_time_s() const { 
            return file_read_time_us.load() / 1'000'000.0; 
        }
        double get_tokenize_time_s() const { 
            return tokenize_time_us.load() / 1'000'000.0; 
        }
        double get_db_time_s() const { 
            return db_time_us.load() / 1'000'000.0; 
        }
    private:
        string open_file(const string &path);
        void batch_jobs(vector<Engine::CodeCandidate> &code_candidates, Engine::Database &db);
        void process_batch(const vector<Engine::CodeCandidate> &code_candidates, Engine::Database &db);
        void database_writer_thread(Engine::Database &db);

        WorkQueue db_queue;
        unordered_map<string, long long> local_mtimes;

        atomic<long long> file_read_time_us{0};
        atomic<long long> tokenize_time_us{0};
        atomic<long long> db_time_us{0};
        atomic<long long> total_files_processed{0};
        atomic<long long> total_content_size{0};
    };
}