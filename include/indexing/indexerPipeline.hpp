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
#include <condition_variable>
#include <mutex>

namespace Engine {
    class IndexerPipeline {
    public:
        IndexerPipeline(Engine::Database &db) : db(db){
            db_thread = std::thread(&Engine::IndexerPipeline::database_writer_thread, this);
        }
        ~IndexerPipeline() {
            db_queue.push({"", {}, true, 0});
            if(db_thread.joinable()) db_thread.join();
        }

        void execute(const std::vector<Engine::CodeCandidate> &code_candidates);

        double get_read_time_s() const { return file_read_time_us.load() / 1'000'000.0; }
        double get_tokenize_time_s() const { return tokenize_time_us.load() / 1'000'000.0; }
        double get_db_time_s() const { return db_time_us.load() / 1'000'000.0; }
        long long get_files_indexed() const { return total_files_indexed.load(); }
        long long get_files_total() const { return total_files_to_index.load(); }
        void request_stop() { stop_requested.store(true); }
        void add_job_to_queue(const CodeCandidate &candidate);

    private:
        std::string open_file(const std::string &path);
        void batch_jobs(const std::vector<Engine::CodeCandidate> &code_candidates);
        void process_batch(const std::vector<Engine::CodeCandidate> &code_candidates);
        void wait_for_database();
        void mark_database_jobs_complete(std::size_t count);
        void database_writer_thread();
        void print_profile(std::chrono::steady_clock::time_point start_time);

        Database &db;
        WorkQueue<IndexJob> db_queue;
        std::thread db_thread;
        std::mutex pending_jobs_mutex;
        std::condition_variable pending_jobs_cv;
        std::size_t pending_database_jobs = 0;

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
