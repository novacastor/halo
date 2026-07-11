#pragma once
#include <string>
#include <vector>
#include <sqlite3.h>
#include <mutex>
#include <unordered_map>
#include "indexing/tokenizer.hpp"
#include "engine/types.hpp"
#include "engine/log.hpp"

namespace Engine {
    class Database {
    public:

        Database(const std::string &db_path);
        ~Database();

        Database(const Database&) = delete;
        Database& operator=(const Database&) = delete;

        bool init();
        bool prepare_statements();
        sqlite3 *get_db_handle();

        int upsert_document(const std::string &file_path, long long mtime);
        bool insert_tokens(int document_id, const std::vector<TokenMatch> &tokens);
        
        bool insert_file(const std::string &name, const std::string &ext, const std::string &path);
        bool delete_file(const std::string &path);
        bool delete_directory(const std::string &dir_path);
        bool add_directory(const std::string &dir_path);
        bool delete_document(const std::string &file_path);
        bool delete_documents_under_directory(const std::string &dir_path);
        
        void load_existing_mtimes();
        void commit_filesystem_index(const std::vector<FSEntry> &files);
        bool file_is_up_to_date(const std::string& path, long long mtime) const;
        
        void begin_transaction();
        void commit_transaction();

        void begin_bulk_index();
        void end_bulk_index();

        std::vector<MatchResult> execute_phrase_search(const std::vector<TokenMatch>& query_tokens, int limit = 50);
        std::vector<FileMatch> execute_filename_search(const std::string &pattern);
        
    private:
        sqlite3 *db_handle = nullptr;
        
        sqlite3_stmt *select_token_stmt = nullptr;
        sqlite3_stmt *insert_token_stmt = nullptr;
        sqlite3_stmt *insert_token_row_stmt = nullptr;
        
        sqlite3_stmt *select_document_id_stmt = nullptr;
        sqlite3_stmt *insert_document_stmt = nullptr;
        sqlite3_stmt *delete_document_tokens_stmt = nullptr;
        sqlite3_stmt *delete_document_stmt = nullptr;
        sqlite3_stmt *delete_inverted_dir_stmt = nullptr;
        sqlite3_stmt *delete_documents_dir_stmt = nullptr;
        sqlite3_stmt *update_document_mtime_stmt = nullptr;

        
        sqlite3_stmt *upsert_fs_stmt = nullptr;
        sqlite3_stmt *delete_fs_stmt = nullptr;
        sqlite3_stmt *delete_fs_dir_stmt = nullptr;
        sqlite3_stmt *add_fs_dir_stmt = nullptr;
        
        std::unordered_map<std::string, long long> existing_mtimes;
        std::unordered_map<std::string, int> token_cache;
        
        int get_or_create_token_id(const std::string &token);
        void configure_database();
        bool create_schema();
        bool prepare_document_statements();
        bool prepare_token_statements();
        bool prepare_filesystem_statements();
        void drop_idx_tokens_table();
        void optimize_search_indexes();
        
        std::mutex db_mutex;
    };
}