#pragma once
#include "indexing/workQueue.hpp"
#include "crawler/crawler.hpp"
#include "database/database.hpp"
#include "engine/types.hpp"
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <unordered_map>
#include <chrono>

namespace Engine {
    class IndexerPipeline {
    public:
        IndexerPipeline() = default;
        ~IndexerPipeline() = default;

        void execute(const std::vector<Engine::CodeCandidate> &code_candidates, Engine::Database &db);

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
        std::string open_file(const std::string &path);
        void batch_jobs(const std::vector<Engine::CodeCandidate> &code_candidates, Engine::Database &db);
        void process_batch(const std::vector<Engine::CodeCandidate> &code_candidates, Engine::Database &db);
        void database_writer_thread(Engine::Database &db);
        void print_profile(std::chrono::steady_clock::time_point start_time);

        WorkQueue<IndexJob> db_queue;

        std::atomic<long long> file_read_time_us{0};
        std::atomic<long long> tokenize_time_us{0};
        std::atomic<long long> db_time_us{0};
        std::atomic<long long> total_files_processed{0};
        std::atomic<long long> total_content_size{0};
    };
}