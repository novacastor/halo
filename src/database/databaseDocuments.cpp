#include "database/database.hpp"
#include <iostream>

using namespace std;

namespace Engine {
    int Database::insert_document(const string &file_path, long long mtime) {
        lock_guard<mutex> lock(db_mutex);

        if(!db_handle) {
            cerr << "db_handle missing " << endl;
            return -1;
        }

        int existing_id = -1;

        sqlite3_reset(select_doc_stmt);
        sqlite3_bind_text(select_doc_stmt, 1, file_path.c_str(), -1, SQLITE_STATIC);
        if(sqlite3_step(select_doc_stmt) == SQLITE_ROW) {
            existing_id = sqlite3_column_int(select_doc_stmt, 0);
        }

        if (existing_id != -1) {
            sqlite3_reset(delete_doc_stmt);
            sqlite3_bind_int(delete_doc_stmt, 1, existing_id);
            sqlite3_step(delete_doc_stmt);

            sqlite3_reset(update_mtime_stmt);
            sqlite3_bind_int64(update_mtime_stmt, 1, mtime);
            sqlite3_bind_int(update_mtime_stmt, 2, existing_id);
            sqlite3_step(update_mtime_stmt);
            
            return existing_id;
        }

        sqlite3_reset(insert_doc_stmt);
        sqlite3_bind_text(insert_doc_stmt, 1, file_path.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int64(insert_doc_stmt, 2, mtime);

        if (sqlite3_step(insert_doc_stmt) == SQLITE_DONE) {
            return static_cast<int>(sqlite3_last_insert_rowid(db_handle));
        }
        return -1;
    }
    
    bool Database::insert_tokens(int document_id, const vector<TokenMatch> &tokens) {
        lock_guard<mutex> lock(db_mutex);

        if(!db_handle || tokens.empty()) {
            if(!db_handle) cerr << "db_handle missing" << endl;
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

    int Database::get_or_create_token_id(const string &token) {
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
            cerr << "Failed to insert token: " << sqlite3_errmsg(db_handle) << endl;
            return -1;
        }

        int new_id = static_cast<int>(sqlite3_last_insert_rowid(db_handle));
        token_cache[token] = new_id;
        return new_id;
    }
}
