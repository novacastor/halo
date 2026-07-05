#include "pipeline.hpp"
#include "tokenizer.hpp"
#include "threadpool.hpp"
#include <iostream>
#include <filesystem>

using namespace std;
using Clock = chrono::steady_clock;

bool Engine::IndexerPipeline::execute(vector<Engine::CodeCandidate> &code_candidates, Engine::Database &db,
    const unordered_map<string, long long> &mtimes) {

    long long initial_processed = total_files_processed.load();

    this->local_mtimes = mtimes;
    this->batch_jobs(code_candidates, db);

    cout << "\n\n===== PROFILE =====\n";
    cout << "File Read : " << file_read_time_us.load() / 1'000'000.0 << " s\n";
    cout << "Tokenize  : " << tokenize_time_us.load() / 1'000'000.0 << " s\n";
    cout << "Database  : " << db_time_us.load() / 1'000'000.0 << " s\n";

    cout << "Total Files processed: " << total_files_processed << endl;
    cout << "Total Data processed: " << total_content_size << " bytes " << endl;

    return total_files_processed.load() > initial_processed;
}

string Engine::IndexerPipeline::open_file(const string &path) {
    ifstream file(path);

    if(!file.is_open()) {
        cerr << endl << "File: " << path << endl;
        cerr << "Can't open file (Doesn't exist or Permission denied)" << endl;
        return "";
    }
    
    stringstream buffer;
    buffer<<file.rdbuf();
    string contents = buffer.str();
    
    file.close();
    
    total_files_processed++;
    total_content_size += contents.size();
    
    return contents;
}

void Engine::IndexerPipeline::batch_jobs(vector<Engine::CodeCandidate> &code_candidates, Engine::Database &db) {
    size_t num_threads = thread::hardware_concurrency();
    ThreadPool pool(num_threads);
    thread db_thread(&Engine::IndexerPipeline::database_writer_thread, this, std::ref(db));
    
    size_t batch_size = (code_candidates.size() + num_threads - 1)  / num_threads;
    if(batch_size == 0) batch_size = 1;
    
    sqlite3_exec(db.get_db_handle(), "DROP INDEX IF EXISTS idx_tokens;", nullptr, nullptr, nullptr);
    // sqlite3_exec(db.get_db_handle(), "DROP INDEX IF EXISTS idx_document_id;", nullptr, nullptr, nullptr);
    
    for(size_t i = 0; i < code_candidates.size(); i += batch_size) {
        auto start_it = code_candidates.begin() + i;
        auto end_it = code_candidates.begin() + min(i + batch_size, code_candidates.size());
        vector<Engine::CodeCandidate> batch(start_it, end_it);
        pool.enqueue([batch = move(batch), &db, this] {
            this->process_batch(batch, db);
        });
    }
    cout << endl << "waiting for pool to finish" << endl;
    pool.wait();
    
    db_queue.push({"", {}, true, 0});
    db_thread.join();
}

void Engine::IndexerPipeline::process_batch(const vector<Engine::CodeCandidate> &code_candidates, Engine::Database &db) {
    for(const auto &candidate: code_candidates) {

        auto it = local_mtimes.find(candidate.path);
        if(it != local_mtimes.end() && it->second == candidate.mtime) {
            continue;
        } 
        
        auto t1 = Clock::now();
        string file_contents = open_file(candidate.path);
        auto t2 = Clock::now();
        
        // cout << "\r[Indexer] files processed: " << total_files_processed << " " << flush;
        
        file_read_time_us += chrono::duration_cast<chrono::microseconds>(t2 - t1).count();
        if(file_contents.empty()) continue;
        
        auto t3 = Clock::now();
        auto tokens = Engine::Tokenizer::tokenize(file_contents);
        auto t4 = Clock::now();
        
        tokenize_time_us += chrono::duration_cast<chrono::microseconds>(t4 - t3).count();
        
        db_queue.push({candidate.path, move(tokens), false, candidate.mtime});
    }
}

void Engine::IndexerPipeline::database_writer_thread(Engine::Database &db) {
    sqlite3 *db_handle = db.get_db_handle();
    
    bool running = true;
    while(running) {
        IndexJob job = db_queue.pop();
        if(job.is_poison_pill) break;
        
        auto t_start = Clock::now();
        sqlite3_exec(db_handle, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);
        
        int batch_count = 0;
        while(true) {
            int doc_id = db.insert_document(job.path, job.mtime);
            if(doc_id != -1 && !job.tokens.empty()) {
                db.insert_tokens(doc_id, job.tokens);
            }
            
            batch_count++;
            if(batch_count >= 200) break;
            
            if(!db_queue.try_pop(job)) break;
            
            if(job.is_poison_pill) {
                running = false;
                break;
            }
            
        }
        
        sqlite3_exec(db_handle, "COMMIT;", nullptr, nullptr, nullptr);
        auto t_end = Clock::now();
        db_time_us += chrono::duration_cast<chrono::microseconds>(t_end - t_start).count();
    }
}
