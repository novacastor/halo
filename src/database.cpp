#include "database.hpp"
#include <iostream>
#include <algorithm>
#include <filesystem>
#include <iomanip>
#include <cctype>
#include <sqlite3.h>
using namespace std;

namespace Engine{
    Database::Database(const string &db_path) {
        if(sqlite3_open(db_path.c_str(), &db_handle) != SQLITE_OK) {
            cerr << "SQL error: Can't open database" << sqlite3_errmsg(db_handle) << endl;
            db_handle = nullptr;
        }

        sqlite3_exec(db_handle, "PRAGMA journal_mode = WAL;", nullptr, nullptr, nullptr);
        sqlite3_exec(db_handle, "PRAGMA synchronous = NORMAL;", nullptr, nullptr, nullptr);        

        sqlite3_exec(db_handle, "PRAGMA cache_size = -500000;", nullptr, nullptr, nullptr); 
        sqlite3_exec(db_handle, "PRAGMA mmap_size = 268435456;", nullptr, nullptr, nullptr);
        sqlite3_exec(db_handle, "PRAGMA temp_store = MEMORY;", nullptr, nullptr, nullptr);
    }

    Database::~Database() {
        if (select_token_stmt) sqlite3_finalize(select_token_stmt);
        if (insert_token_stmt) sqlite3_finalize(insert_token_stmt);
        if (insert_token_row_stmt) sqlite3_finalize(insert_token_row_stmt);

        if (select_doc_stmt) sqlite3_finalize(select_doc_stmt); 
        if (insert_doc_stmt) sqlite3_finalize(insert_doc_stmt);
        if (delete_doc_stmt) sqlite3_finalize(delete_doc_stmt);

        if (update_mtime_stmt) sqlite3_finalize(update_mtime_stmt);

        if(db_handle) sqlite3_close(db_handle);
    }

    bool Database::init() {
        if(!db_handle) return false;

        const char* schema_sql = 
            "PRAGMA foreign_keys = ON;"
            "CREATE TABLE IF NOT EXISTS documents ("
            "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "    file_path TEXT UNIQUE NOT NULL,"
            "    mtime INTEGER NOT NULL DEFAULT 0"
            ");"
            "CREATE TABLE IF NOT EXISTS tokens ("
            "   id INTEGER PRIMARY KEY,"
            "   text TEXT UNIQUE NOT NULL"
            ");"
            "CREATE TABLE IF NOT EXISTS inverted_index ("
            "    token_id INTEGER NOT NULL,"
            "    document_id INTEGER NOT NULL,"
            "    line_number INTEGER NOT NULL,"
            "    FOREIGN KEY(document_id) REFERENCES documents(id),"
            "    FOREIGN KEY(token_id) REFERENCES tokens(id)"
            ");"
            "CREATE INDEX IF NOT EXISTS idx_document_id ON inverted_index(document_id);"
            "CREATE INDEX IF NOT EXISTS idx_tokens ON inverted_index(token_id);";

        char* error_msg = nullptr;

        if(sqlite3_exec(db_handle, schema_sql, nullptr, nullptr, &error_msg) != SQLITE_OK) {
            cerr << "SQL Error: Can't initialize database." << error_msg << endl;
            sqlite3_free(error_msg);
            return false;
        }

        return Database::prepare_statements();
    }

    sqlite3 *Database::get_db_handle() {
        return db_handle;
    }

    int Database::insert_document(const string &file_path, long long mtime) {
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

    bool Database::prepare_statements() {

        const char* doc_sql = "INSERT INTO documents (file_path, mtime) VALUES (?, ?);"; 
        if (sqlite3_prepare_v2(db_handle, doc_sql, -1, &insert_doc_stmt, nullptr) != SQLITE_OK) {
            cerr << "Failed to prepare doc statement: " << sqlite3_errmsg(db_handle) << endl;
            return false;
        }
    
        const char* token_sql = "INSERT INTO inverted_index (token_id, document_id, line_number) VALUES (?, ?, ?);";
        if (sqlite3_prepare_v2(db_handle, token_sql, -1, &insert_token_stmt, nullptr) != SQLITE_OK) {
            cerr << "Failed to prepare insert token statement: " << sqlite3_errmsg(db_handle) << endl;
            return false;
        }
    
        const char *select_doc_sql = "SELECT id FROM documents WHERE file_path = ?;";
        if(sqlite3_prepare_v2(db_handle, select_doc_sql, -1, &select_doc_stmt, nullptr) != SQLITE_OK) {
            cerr << "Failed to prepare select document statement: " << sqlite3_errmsg(db_handle) << endl;
            return false;
        }
    
        const char* delete_doc_sql = "DELETE FROM inverted_index WHERE document_id = ?;";
        if(sqlite3_prepare_v2(db_handle, delete_doc_sql, -1, &delete_doc_stmt, nullptr) != SQLITE_OK) {
            cerr << "Failed to prepare delete document statement: " << sqlite3_errmsg(db_handle) << endl;
            return false;
        }
    
        const char* select_token_sql = "SELECT id FROM tokens WHERE text = ?;";
        if(sqlite3_prepare_v2(db_handle, select_token_sql, -1, &select_token_stmt, nullptr) != SQLITE_OK) {
            cerr << "Failed to prepare select token statement: " << sqlite3_errmsg(db_handle) << endl;
            return false;
        }
    
        const char* insert_token_row_sql = "INSERT INTO tokens (text) VALUES (?);";
        if(sqlite3_prepare_v2(db_handle, insert_token_row_sql, -1, &insert_token_row_stmt, nullptr) != SQLITE_OK) {
            cerr << "Failed to prepare insert token row statement: " << sqlite3_errmsg(db_handle) << endl;
            return false;
        }
    
        const char* update_mtime_sql = "UPDATE documents SET mtime = ? WHERE id = ?;";
        if(sqlite3_prepare_v2(db_handle, update_mtime_sql, -1, &update_mtime_stmt, nullptr) != SQLITE_OK) {
            cerr << "Failed to prepare update mtime statement: " << sqlite3_errmsg(db_handle) << endl;
            return false;
        }
                
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

    void Database::print_inverted_index() {
        if(!db_handle) return;

        const char* sql = 
            "SELECT t.text, d.file_path, i.line_number "
            "FROM inverted_index i "
            "JOIN documents d ON i.document_id = d.id "
            "JOIN tokens t ON i.token_id = t.id "
            "ORDER BY t.text ASC, i.line_number ASC;";

        sqlite3_stmt* stmt;

        if (sqlite3_prepare_v2(db_handle, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            cerr << "Failed to execute print query: " << sqlite3_errmsg(db_handle) << endl;
            return;
        }

        cout << endl << "=================== INVERTED INDEX DATABASE ===================" << endl;
        cout << "TOKEN               | LINE | FILE PATH" << endl;
        cout << "---------------------------------------------------------------" << endl;

        while(sqlite3_step(stmt) == SQLITE_ROW) {
            const unsigned char* token = sqlite3_column_text(stmt, 0);
            const unsigned char* path  = sqlite3_column_text(stmt, 1);
            int line                   = sqlite3_column_int(stmt, 2);

            printf("%-19s | %-4d | %s\n", token, line, path);
        }
        cout << "===============================================================" << endl << endl;
        sqlite3_finalize(stmt);
    }

    void Database::print_db_size(string db_path) const {
        try {
            if (filesystem::exists(db_path)) {
                uintmax_t file_size_bytes = filesystem::file_size(db_path);
                double file_size_kb = static_cast<double>(file_size_bytes) / 1024.0;
                double file_size_mb = file_size_kb / 1024.0;

                cout << "\n========================================\n";
                cout << "DATABASE STORAGE METRICS:\n";
                cout << "----------------------------------------\n";
                cout << "Database File: " << db_path << "\n";
                
                if (file_size_mb >= 1.0) {
                    cout << "Disk Footprint: " << fixed << setprecision(2) << file_size_mb << " MB\n";
                } else {
                    cout << "Disk Footprint: " << fixed << setprecision(2) << file_size_kb << " KB (" << file_size_bytes << " bytes)\n";
                }
                cout << "========================================\n";
            } else {
                cerr << "Warning: 'test.db' not found on disk to compute size.\n";
            }
        } catch (const filesystem::filesystem_error& e) {
            cerr << "Filesystem Error checking DB size: " << e.what() << "\n";
        }
    }
}
