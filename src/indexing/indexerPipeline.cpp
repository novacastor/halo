#include "indexing/indexerPipeline.hpp"
#include "indexing/tokenizer.hpp"
#include "indexing/threadpool.hpp"
#include "engine/log.hpp"
#include <filesystem>

using Clock = std::chrono::steady_clock;

void Engine::IndexerPipeline::print_profile(Clock::time_point t_start) {
    LOG_INFO("\n\n====== PROFILE ======\n");
    LOG_INFO("File Read : " + std::to_string(file_read_time_us.load() / 1'000'000.0) + " s");
    LOG_INFO("Tokenize  : " + std::to_string(tokenize_time_us.load() / 1'000'000.0) + " s");
    LOG_INFO("Database  : " + std::to_string(db_time_us.load() / 1'000'000.0) + " s");
    
    LOG_INFO("Total Files processed: " + std::to_string(total_files_processed));
    LOG_INFO("Total Data processed: " + std::to_string(total_content_size) + " bytes");
    auto t_end = Clock::now();
    LOG_INFO("Index build: " + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(t_end - t_start).count() / 1000.0) + " s");
}

void Engine::IndexerPipeline::execute(const std::vector<Engine::CodeCandidate> &code_candidates) {

    size_t file_change_count = 0;
    for(const auto &candidate: code_candidates) {
        if(!db.file_is_up_to_date(candidate.path, candidate.mtime)) file_change_count++;
    }

    total_files_to_index.store(file_change_count);
    total_files_indexed.store(0);
    
    bool rebuild_required = false;
    if(file_change_count >= 1000) rebuild_required = true;

    if(rebuild_required) db.begin_bulk_index();
    auto t_start = Clock::now();
    this->batch_jobs(code_candidates);
    if(rebuild_required) db.end_bulk_index();

    print_profile(t_start);
}

std::string Engine::IndexerPipeline::open_file(const std::string &path) {
    std::ifstream file(path);

    if(!file.is_open()) {
        LOG_ERROR("Can't open file " + path + " Doesn't exist or permission denied");
        return "";
    }
    
    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    std::string contents(size, ' ');
    file.seekg(0, std::ios::beg);
    file.read(&contents[0], size);
    
    file.close();
    
    total_files_processed++;
    total_content_size += contents.size();
    
    return contents;
}

void Engine::IndexerPipeline::batch_jobs(const std::vector<Engine::CodeCandidate> &code_candidates) {
    size_t num_threads = std::thread::hardware_concurrency();
    if(num_threads == 0) num_threads = 4;
    ThreadPool pool(num_threads);
    
    size_t batch_size = (code_candidates.size() + num_threads - 1)  / num_threads;
    if(batch_size == 0) batch_size = 1;
    
    
    for(size_t i = 0; i < code_candidates.size(); i += batch_size) {
        auto start_it = code_candidates.begin() + i;
        auto end_it = code_candidates.begin() + std::min(i + batch_size, code_candidates.size());
        std::vector<Engine::CodeCandidate> batch(start_it, end_it);
        pool.enqueue([batch = move(batch), this] {
            this->process_batch(batch);
        });
    }

    LOG_INFO("Waiting for File Indexing to Finish. ");
    pool.wait();
    
    LOG_INFO("File Indexing complete. ");
}

void Engine::IndexerPipeline::process_batch(const std::vector<Engine::CodeCandidate> &code_candidates) {
    for(const auto &candidate: code_candidates) {
        if(stop_requested.load()) {
            LOG_INFO("Stop Requested, exiting process_batch early. ");
            break;
        }
        add_job_to_queue(candidate);
    }
}

void Engine::IndexerPipeline::add_job_to_queue(const Engine::CodeCandidate &candidate) {
    if(db.file_is_up_to_date(candidate.path, candidate.mtime)) return;

    auto t1 = Clock::now();
    std::string file_contents = open_file(candidate.path);
    auto t2 = Clock::now();

    file_read_time_us += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
    if(file_contents.empty()) return;

    auto t3 = Clock::now();
    auto tokens = Engine::Tokenizer::tokenize(file_contents);
    auto t4 = Clock::now();

    tokenize_time_us += std::chrono::duration_cast<std::chrono::microseconds>(t4 - t3).count();

    db_queue.push({candidate.path, move(tokens), false, candidate.mtime});
}

void Engine::IndexerPipeline::database_writer_thread() {
    bool running = true;
    while(running) {
        IndexJob job = db_queue.pop();
        if(job.is_poison_pill) break;
        
        auto t_start = Clock::now();
        db.begin_transaction();
        
        int batch_count = 0;
        while(true) {
            if(stop_requested.load()){
                LOG_INFO("Stop Requested, flushing current transaction and exiting writer thread. ");
                break; 
            }

            int doc_id = db.upsert_document(job.path, job.mtime);
            if(doc_id != -1 && !job.tokens.empty()) {
                db.insert_tokens(doc_id, job.tokens);
                total_files_indexed++;
            }
            
            batch_count++;
            if(batch_count >= 200) break;
            
            if(!db_queue.try_pop(job)) break;
            
            if(job.is_poison_pill) {
                running = false;
                break;
            }
            
        }
        
        db.commit_transaction();
        auto t_end = Clock::now();
        db_time_us += std::chrono::duration_cast<std::chrono::microseconds>(t_end - t_start).count();
    }
}
