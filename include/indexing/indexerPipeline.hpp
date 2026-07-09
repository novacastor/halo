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

        long long get_read_time_s() const { return file_read_time_us; }
        long long get_tokenize_time_s() const { return tokenize_time_us; }
        long long get_db_time_s() const { return db_time_us; }
        long long get_files_indexed() const { return total_files_indexed.load(); }
        long long get_files_total() const { return total_files_to_index.load(); }
        void request_stop() { stop_requested.store(true); }

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
        std::atomic<long long> total_files_indexed{0};
        std::atomic<long long> total_files_to_index{0};
        std::atomic<bool> stop_requested{false};
    };
}