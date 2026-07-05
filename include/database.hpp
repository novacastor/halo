#pragma once
#include <string>
#include <string_view>
#include <vector>
#include <sqlite3.h>
#include <mutex>
#include <unordered_map>
#include "tokenizer.hpp"
using namespace std;

namespace Engine {
    struct FSEntry;

    class Database {
    public:
        Database(const string &db_path);
        ~Database();

        Database(const Database&) = delete;
        Database& operator=(const Database&) = delete;

        bool init();
        bool prepare_statements();
        sqlite3 *get_db_handle();
        int insert_document(const string &file_path, long long mtime);
        bool insert_tokens(int document_id, const vector<TokenMatch> &tokens);
        bool insert_file(const string &name, const string &ext, const string &path);
        bool delete_file(const string &path);
        bool delete_directory(const string &dir_path);
        void print_inverted_index();
        void print_db_size(string db_path);
        void load_existing_mtimes(unordered_map<string, long long> &existing_mtimes);
        void optimize_search_indexes();
        void commit_filesystem_index(const vector<FSEntry> &files);

    private:
        sqlite3 *db_handle = nullptr;

        sqlite3_stmt *select_token_stmt = nullptr;
        sqlite3_stmt *insert_token_stmt = nullptr;
        sqlite3_stmt *insert_token_row_stmt = nullptr;

        sqlite3_stmt *select_doc_stmt = nullptr;
        sqlite3_stmt *insert_doc_stmt = nullptr;
        sqlite3_stmt *delete_doc_stmt = nullptr;

        sqlite3_stmt *upsert_fs_stmt = nullptr;
        sqlite3_stmt *delete_fs_stmt = nullptr;
        sqlite3_stmt *delete_fs_dir_stmt = nullptr;
        
        sqlite3_stmt* update_mtime_stmt = nullptr;

        unordered_map<string, int> token_cache;

        int get_or_create_token_id(const string &token);
        mutex db_mutex;
    };
}