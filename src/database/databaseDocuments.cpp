#include "database/database.hpp"

namespace Engine {
    int Database::upsert_document(const std::string &file_path, long long mtime) {
        std::lock_guard<std::mutex> lock(db_mutex);

        if(!db_handle) {
            LOG_ERROR("Database handle missing, can't insert document.");
            return -1;
        }

        int existing_id = -1;

        sqlite3_reset(select_document_id_stmt);
        sqlite3_bind_text(select_document_id_stmt, 1, file_path.c_str(), -1, SQLITE_STATIC);
        if(sqlite3_step(select_document_id_stmt) == SQLITE_ROW) {
            existing_id = sqlite3_column_int(select_document_id_stmt, 0);
        }

        if (existing_id != -1) {
            sqlite3_reset(delete_document_tokens_stmt);
            sqlite3_bind_int(delete_document_tokens_stmt, 1, existing_id);
            sqlite3_step(delete_document_tokens_stmt);

            sqlite3_reset(update_document_mtime_stmt);
            sqlite3_bind_int64(update_document_mtime_stmt, 1, mtime);
            sqlite3_bind_int(update_document_mtime_stmt, 2, existing_id);
            sqlite3_step(update_document_mtime_stmt);
            
            return existing_id;
        }

        sqlite3_reset(insert_document_stmt);
        sqlite3_bind_text(insert_document_stmt, 1, file_path.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int64(insert_document_stmt, 2, mtime);

        if (sqlite3_step(insert_document_stmt) == SQLITE_DONE) {
            existing_mtimes[file_path] = mtime;
            return static_cast<int>(sqlite3_last_insert_rowid(db_handle));
        }
        return -1;
    }
    
    bool Database::insert_tokens(int document_id, const std::vector<TokenMatch> &tokens) {
        std::lock_guard<std::mutex> lock(db_mutex);

        if(!db_handle || tokens.empty()) {
            if(!db_handle) {
                LOG_ERROR("Database handle missing can't insert tokens.");
            }
            return false;
        }

        for(const auto &match: tokens) {
            int token_id = get_or_create_token_id(match.token);
            if(token_id == -1) continue;

            sqlite3_reset(insert_token_stmt);

            sqlite3_bind_int(insert_token_stmt, 1, token_id);
            sqlite3_bind_int(insert_token_stmt, 2, document_id);
            sqlite3_bind_int(insert_token_stmt, 3, match.line_number);

            sqlite3_step(insert_token_stmt);
        }
        sqlite3_reset(insert_token_stmt);
        return true;
    }

    bool Database::delete_documents_under_directory(const std::string &dir_path) {
        if(!db_handle) {
            LOG_ERROR("Database handle missing, can't delete documents under directory.");
            return false;
        }

        std::string wildcard_path = dir_path;
        if(wildcard_path.back() != '/') {
            wildcard_path += '/';
        }
        wildcard_path += '%';

        sqlite3_reset(delete_inverted_dir_stmt);
        sqlite3_bind_text(delete_inverted_dir_stmt, 1, dir_path.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(delete_inverted_dir_stmt, 2, wildcard_path.c_str(), -1, SQLITE_STATIC);
        if(sqlite3_step(delete_inverted_dir_stmt) != SQLITE_DONE) {
            LOG_ERROR(std::string("Failed to delete inverted index rows under directory: ") + sqlite3_errmsg(db_handle));
            return false;
        }

        sqlite3_reset(delete_documents_dir_stmt);
        sqlite3_bind_text(delete_documents_dir_stmt, 1, dir_path.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(delete_documents_dir_stmt, 2, wildcard_path.c_str(), -1, SQLITE_STATIC);
        if(sqlite3_step(delete_documents_dir_stmt) != SQLITE_DONE) {
            LOG_ERROR(std::string("Failed to delete documents under directory: ") + sqlite3_errmsg(db_handle));
            return false;
        }
        
        for(auto it = existing_mtimes.begin(); it != existing_mtimes.end();) {
            if(it->first.compare(0, dir_path.size(), dir_path) == 0) {
                it = existing_mtimes.erase(it);
            } else {
                ++it;
            }
        }
        
        return true;
    }

    bool Database::delete_document(const std::string &file_path) {
        std::lock_guard<std::mutex> lock(db_mutex);

        if(!db_handle) {
            LOG_ERROR("Database handle missing, can't delete document.");
            return false;
        }

        int existing_id = -1;

        sqlite3_reset(select_document_id_stmt);
        sqlite3_bind_text(select_document_id_stmt, 1, file_path.c_str(), -1, SQLITE_STATIC);
        if(sqlite3_step(select_document_id_stmt) == SQLITE_ROW) {
            existing_id = sqlite3_column_int(select_document_id_stmt, 0);
        }

        if(existing_id == -1) {
            return true;
        }

        sqlite3_reset(delete_document_tokens_stmt);
        sqlite3_bind_int(delete_document_tokens_stmt, 1, existing_id);
        sqlite3_step(delete_document_tokens_stmt);

        sqlite3_reset(delete_document_stmt);
        sqlite3_bind_int(delete_document_stmt, 1, existing_id);
        if(sqlite3_step(delete_document_stmt) != SQLITE_DONE) {
            LOG_ERROR("Can't delete file: " + file_path);
            return false;
        }

        existing_mtimes.erase(file_path);

        return true;
    }

    int Database::get_or_create_token_id(const std::string &token) {
        auto it = token_cache.find(token);
        if(it != token_cache.end()) return it->second;

        sqlite3_reset(select_token_stmt);
        sqlite3_bind_text(select_token_stmt, 1, token.c_str(), -1, SQLITE_STATIC);

        if(sqlite3_step(select_token_stmt) == SQLITE_ROW) {
            int id = sqlite3_column_int(select_token_stmt, 0);
            token_cache[token] = id;
            return id;
        }

        sqlite3_reset(insert_token_row_stmt);
        sqlite3_bind_text(insert_token_row_stmt, 1, token.c_str(), -1, SQLITE_STATIC);

        if(sqlite3_step(insert_token_row_stmt) != SQLITE_DONE) {
            LOG_ERROR(std::string("Failed to insert token ") + sqlite3_errmsg(db_handle));
            return -1;
        }

        int new_id = static_cast<int>(sqlite3_last_insert_rowid(db_handle));
        token_cache[token] = new_id;
        return new_id;
    }
}
