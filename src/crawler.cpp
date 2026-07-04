#include "crawler.hpp"
#include "tokenizer.hpp"
#include "threadpool.hpp"
#include "concurrentQueue.hpp"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_set>
#include <chrono>
#include <atomic>
#include <mutex>

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

unordered_map<string, long long> existing_mtimes;

atomic<long long> total_files_processed = 0;
atomic<long long> total_content_size = 0;
atomic<long long> file_read_time_us{0};
atomic<long long> tokenize_time_us{0};
atomic<long long> db_time_us{0};

WorkQueue db_queue;

struct FileEntry {
    string path;
    long long mtime;
};


void print_progress() { 
    cout << fixed << setprecision(2);
    
    if (total_content_size >= 1024LL * 1024 * 1024) {
        cout << "\r[Indexer] Total Files Processed: " << total_files_processed
        << " | Total Content Size: "
        << total_content_size / (1024.0 * 1024 * 1024)
        << " GB " << flush;
    }
    else if (total_content_size >= 1024LL * 1024) {
        cout << "\r[Indexer] Total Files Processed: " << total_files_processed
        << " | Total Content Size: "
        << total_content_size / (1024.0 * 1024)
        << " MB " << flush;
    }
    else if (total_content_size >= 1024LL) {
                cout << "\r[Indexer] Total Files Processed: " << total_files_processed << " | Total Content Size: " << total_content_size / 1024.0
                << " KB " << flush;
            }
    else {
        cout << "\r[Indexer] Total Files Processed: " << total_files_processed << " | Total Content Size: " 
        << total_content_size << " B " << flush;
    }
    
}

void load_existing_mtimes(Engine::Database &db) {
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db.get_db_handle(), "SELECT file_path, mtime FROM documents;", -1, &stmt, nullptr);

    while(sqlite3_step(stmt) == SQLITE_ROW) {
        string path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        long long mtime = sqlite3_column_int64(stmt, 1);
    
        existing_mtimes[path] = mtime;
    }

    sqlite3_finalize(stmt);
}

string open_file(const string &path) {
    // cerr << "[opening] " << path << endl;
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

void process_batch(const vector<FileEntry> &paths, Engine::Database &db) {
    for(const FileEntry &entry: paths) {

        auto it = existing_mtimes.find(entry.path);
        if(it != existing_mtimes.end() && it->second == entry.mtime) {
            continue;
        } 
        
        auto t1 = Clock::now();
        string file_contents = open_file(entry.path);
        auto t2 = Clock::now();
        
        // cout << "\r[Indexer] files processed: " << total_files_processed << " " << flush;
        
        file_read_time_us += chrono::duration_cast<chrono::microseconds>(t2 - t1).count();
        if(file_contents.empty()) continue;
        
        auto t3 = Clock::now();
        auto tokens = Engine::Tokenizer::tokenize(file_contents);
        auto t4 = Clock::now();
        
        tokenize_time_us += chrono::duration_cast<chrono::microseconds>(t4 - t3).count();
        
        db_queue.push({entry.path, move(tokens), false, entry.mtime});
    }
}

void database_writer_thread(Engine::Database &db) {
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

vector<FileEntry> get_all_paths(const string &target_path) {
    fs::path root_path(target_path);
    vector<FileEntry> all_paths;
    
    for(auto it = fs::recursive_directory_iterator(root_path, fs::directory_options::skip_permission_denied); it != fs::end(it); ++it) {
        const auto &entry = *it;
        
        if(entry.is_directory()) {
            string folder_name = entry.path().filename().string();
            if(FOLDER_BLACKLIST.find(folder_name) != FOLDER_BLACKLIST.end()) {
                it.disable_recursion_pending();
            }
            continue;
        }
        
        if(!entry.is_regular_file()) continue;
        
        string ext = entry.path().extension().string();
        if(EXTENSION_WHITELIST.find(ext) == EXTENSION_WHITELIST.end()) continue;
        
        auto ftime = entry.last_write_time();
        
        long long mtime = chrono::duration_cast<chrono::seconds>(fs::file_time_type::clock::to_sys(ftime).time_since_epoch()).count();
        
        all_paths.push_back({fs::absolute(entry.path()).string(), mtime});
    }

    return all_paths;
}

void batch_jobs(vector<FileEntry> &all_paths, Engine::Database &db) {
    size_t num_threads = thread::hardware_concurrency();
    ThreadPool pool(num_threads);
    thread db_thread(database_writer_thread, std::ref(db));
    
    size_t batch_size = (all_paths.size() + num_threads - 1)  / num_threads;
    if(batch_size == 0) batch_size = 1;
    
    sqlite3_exec(db.get_db_handle(), "DROP INDEX IF EXISTS idx_tokens;", nullptr, nullptr, nullptr);
    // sqlite3_exec(db.get_db_handle(), "DROP INDEX IF EXISTS idx_document_id;", nullptr, nullptr, nullptr);
    
    for(size_t i = 0; i < all_paths.size(); i += batch_size) {
        auto start_it = all_paths.begin() + i;
        auto end_it = all_paths.begin() + min(i + batch_size, all_paths.size());
        vector<FileEntry> batch(start_it, end_it);
        pool.enqueue([batch = move(batch), &db] {
            process_batch(batch, db);
        });
    }
    cout << endl << "waiting for pool to finish" << endl;
    pool.wait();
    
    db_queue.push({"", {}, true, 0});
    db_thread.join();
}

void optimize_search_indexes(Engine::Database &db) {
    sqlite3_exec(db.get_db_handle(), "PRAGMA synchronous = OFF;", nullptr, nullptr, nullptr);
    sqlite3_exec(db.get_db_handle(), "BEGIN;", nullptr, nullptr, nullptr);
    
    sqlite3_exec(db.get_db_handle(), "CREATE INDEX IF NOT EXISTS idx_document_id ON inverted_index(document_id);", nullptr, nullptr, nullptr);
    sqlite3_exec(db.get_db_handle(), "CREATE INDEX IF NOT EXISTS idx_tokens ON inverted_index(token_id);", nullptr, nullptr, nullptr);
    
    sqlite3_exec(db.get_db_handle(), "COMMIT;", nullptr, nullptr, nullptr);
    sqlite3_exec(db.get_db_handle(), "PRAGMA synchronous = NORMAL;", nullptr, nullptr, nullptr);
}

pair<int, int> run_crawler(const string &target_path, Engine::Database &db) {
    load_existing_mtimes(db);

    vector<FileEntry> all_paths = get_all_paths(target_path);
    batch_jobs(all_paths, db);
    
    cout << endl << "Optimizing search indexes..." << endl;
    
    auto t_start = Clock::now();
    optimize_search_indexes(db);
    auto t_end = Clock::now();
    
    cout << "Index build: " << chrono::duration_cast<chrono::milliseconds>(t_end - t_start).count() / 1000.0 << " s\n";
    
    cout << "\n\n===== PROFILE =====\n";
    cout << "File Read : " << file_read_time_us.load() / 1'000'000.0 << " s\n";
    cout << "Tokenize  : " << tokenize_time_us.load() / 1'000'000.0 << " s\n";
    cout << "Database  : " << db_time_us.load() / 1'000'000.0 << " s\n";
    
    return {total_files_processed, total_content_size};
}

/*
cmake --build build
./build/search_engine
*/