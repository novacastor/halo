#include "database/database.hpp"
#include <iostream>

using namespace std;

namespace Engine {
    void finalize(sqlite3_stmt*& stmt) {
        if (stmt) {
            sqlite3_finalize(stmt);
            stmt = nullptr;
        }
    }

    Database::Database(const string &db_path) {
        if(sqlite3_open(db_path.c_str(), &db_handle) != SQLITE_OK) {
            cerr << "SQL error: Can't open database" << sqlite3_errmsg(db_handle) << endl;

            if (db_handle) {
                sqlite3_close(db_handle);
                db_handle = nullptr;
            }
            return;
        }
        configure_database();
    }

    Database::~Database() {
        lock_guard<mutex> lock(db_mutex);

        finalize(select_token_stmt);
        finalize(insert_token_stmt);
        finalize(insert_token_row_stmt);

        finalize(select_doc_stmt);
        finalize(insert_doc_stmt);
        finalize(delete_doc_stmt);
        finalize(update_mtime_stmt);

        finalize(upsert_fs_stmt);
        finalize(delete_fs_stmt);
        finalize(delete_fs_dir_stmt);

        if(db_handle) sqlite3_close(db_handle);
    }

    bool Database::init() {
        lock_guard<mutex> lock(db_mutex);

        if(!db_handle) return false;
        if(!create_schema()) return false;
        if(!prepare_statements()) return false;
        
        load_existing_mtimes();
        return true;
    }

    sqlite3 *Database::get_db_handle() {
        return db_handle;
    }

    void Database::load_existing_mtimes() {
        sqlite3_stmt *stmt;
        if(sqlite3_prepare_v2(db_handle, "SELECT file_path, mtime FROM documents;", -1, &stmt, nullptr) != SQLITE_OK) {
            cerr << "Failed to prepare statement for mtime loading: " << sqlite3_errmsg(db_handle) << endl;
            return;
        }

        while(sqlite3_step(stmt) == SQLITE_ROW) {
            string path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            long long mtime = sqlite3_column_int64(stmt, 1);
        
            existing_mtimes[path] = mtime;
        }

        sqlite3_finalize(stmt);
    }

    void Database::begin_transaction() {
        lock_guard<mutex> lock(db_mutex);
        sqlite3_exec(db_handle, "BEGIN;", nullptr, nullptr, nullptr);
    }
    
    void Database::commit_transaction() {
        lock_guard<mutex> lock(db_mutex);
        sqlite3_exec(db_handle, "COMMIT;", nullptr, nullptr, nullptr);
    }

    void Database::begin_bulk_index() {
        drop_idx_tokens_table();
    }

    void Database::end_bulk_index() {
        {
            lock_guard<mutex> lock(db_mutex);
            token_cache.clear();
        }
        optimize_search_indexes();
    }

    void Database::drop_idx_tokens_table() {
        lock_guard<mutex> lock(db_mutex);
        sqlite3_exec(db_handle, "DROP INDEX IF EXISTS idx_tokens;", nullptr, nullptr, nullptr);
    }

    void Database::optimize_search_indexes() {
        lock_guard<mutex> lock(db_mutex); 

        sqlite3_exec(db_handle, "PRAGMA synchronous = OFF;", nullptr, nullptr, nullptr);
        sqlite3_exec(db_handle, "BEGIN;", nullptr, nullptr, nullptr);
        
        sqlite3_exec(db_handle, "CREATE INDEX IF NOT EXISTS idx_document_id ON inverted_index(document_id);", nullptr, nullptr, nullptr);
        sqlite3_exec(db_handle, "CREATE INDEX IF NOT EXISTS idx_tokens ON inverted_index(token_id);", nullptr, nullptr, nullptr);
        
        sqlite3_exec(db_handle, "COMMIT;", nullptr, nullptr, nullptr);
        sqlite3_exec(db_handle, "PRAGMA synchronous = NORMAL;", nullptr, nullptr, nullptr);
    }


    bool Database::prepare_statements() {
        return prepare_document_statements() && prepare_token_statements() && prepare_filesystem_statements();
    }

    void Database::configure_database() {
        sqlite3_exec(db_handle, "PRAGMA journal_mode = WAL;", nullptr, nullptr, nullptr);
        sqlite3_exec(db_handle, "PRAGMA synchronous = NORMAL;", nullptr, nullptr, nullptr);        

        sqlite3_exec(db_handle, "PRAGMA cache_size = -500000;", nullptr, nullptr, nullptr); 
        sqlite3_exec(db_handle, "PRAGMA mmap_size = 268435456;", nullptr, nullptr, nullptr);
        sqlite3_exec(db_handle, "PRAGMA temp_store = MEMORY;", nullptr, nullptr, nullptr);
    }

    bool Database::create_schema() {
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
            "CREATE TABLE IF NOT EXISTS filesystem_index ("
            "   id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "   file_path TEXT UNIQUE NOT NULL, "
            "   file_name TEXT NOT NULL, "
            "   file_ext TEXT NOT NULL"
            ");"
            "CREATE INDEX IF NOT EXISTS idx_document_id ON inverted_index(document_id);"
            "CREATE INDEX IF NOT EXISTS idx_tokens ON inverted_index(token_id);";

        char* error_msg = nullptr;

        if(sqlite3_exec(db_handle, schema_sql, nullptr, nullptr, &error_msg) != SQLITE_OK) {
            cerr << "SQL Error: Can't initialize database." << error_msg << endl;
            sqlite3_free(error_msg);
            return false;
        }

        return true;
    }
    
    bool Database::prepare_document_statements() {
        const char* insert_doc_sql = "INSERT INTO documents (file_path, mtime) VALUES (?, ?);"; 
        if (sqlite3_prepare_v2(db_handle, insert_doc_sql, -1, &insert_doc_stmt, nullptr) != SQLITE_OK) {
            cerr << "Failed to prepare doc statement: " << sqlite3_errmsg(db_handle) << endl;
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
            
        const char* update_mtime_sql = "UPDATE documents SET mtime = ? WHERE id = ?;";
        if(sqlite3_prepare_v2(db_handle, update_mtime_sql, -1, &update_mtime_stmt, nullptr) != SQLITE_OK) {
            cerr << "Failed to prepare update mtime statement: " << sqlite3_errmsg(db_handle) << endl;
            return false;
        }

        return true;
    }

    bool Database::prepare_token_statements() {
        const char* insert_token_sql = "INSERT INTO inverted_index (token_id, document_id, line_number) VALUES (?, ?, ?);";
        if (sqlite3_prepare_v2(db_handle, insert_token_sql, -1, &insert_token_stmt, nullptr) != SQLITE_OK) {
            cerr << "Failed to prepare insert token statement: " << sqlite3_errmsg(db_handle) << endl;
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

        return true;
    }

    bool Database::prepare_filesystem_statements() {
        const char* upsert_fs_sql = "INSERT OR REPLACE INTO filesystem_index (file_name, file_ext, file_path) VALUES (?, ?, ?);";
        if(sqlite3_prepare_v2(db_handle, upsert_fs_sql, -1, &upsert_fs_stmt, nullptr) != SQLITE_OK) {
            cerr << "Failed to prepare upsert filesystem statement: " << sqlite3_errmsg(db_handle) << endl;
            return false;
        } 

        const char* delete_fs_sql = "DELETE FROM filesystem_index WHERE file_path = ?;";
        if(sqlite3_prepare_v2(db_handle, delete_fs_sql, -1, &delete_fs_stmt, nullptr) != SQLITE_OK) {
            cerr << "Failed to prepare delete filesystem statement: " << sqlite3_errmsg(db_handle) << endl;
            return false;
        }

        const char* delete_fs_dir_sql = "DELETE FROM filesystem_index WHERE file_path = ? OR file_path LIKE ?;";
        if(sqlite3_prepare_v2(db_handle, delete_fs_dir_sql, -1, &delete_fs_dir_stmt, nullptr) != SQLITE_OK) {
            cerr << "Failed to prepare delete filesystem directory statement: " << sqlite3_errmsg(db_handle) << endl;
            return false;
        }

        return true;
    }
}