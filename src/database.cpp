#include "database.hpp"
#include <iostream>
#include <algorithm>
#include <cctype>
using namespace std;

namespace Engine{
    Database::Database(const string &db_path) {
        if(sqlite3_open(db_path.c_str(), &db_handle) != SQLITE_OK) {
            cerr << "SQL error: Can't open database" << sqlite3_errmsg(db_handle) << endl;
            db_handle = nullptr;
        }
    }

    Database::~Database() {
        if(db_handle) sqlite3_close(db_handle);
    }

    bool Database::init() {
        if(!db_handle) return false;

        const char* schema_sql = 
            "PRAGMA foreign_keys = ON;"
            "CREATE TABLE IF NOT EXISTS documents ("
            "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "    file_path TEXT UNIQUE NOT NULL"
            ");"
            "CREATE TABLE IF NOT EXISTS inverted_index ("
            "    token TEXT NOT NULL,"
            "    document_id INTEGER NOT NULL,"
            "    line_number INTEGER NOT NULL,"
            "    FOREIGN KEY(document_id) REFERENCES documents(id)"
            ");"
            "CREATE INDEX IF NOT EXISTS idx_tokens ON inverted_index(token);";

        char* error_msg = nullptr;
        int exit = sqlite3_exec(db_handle, schema_sql, nullptr, nullptr, &error_msg);

        if(exit != SQLITE_OK) {
            cerr << "SQL Error: Can't initialize database." << error_msg << endl;
            sqlite3_free(error_msg);
            return false;
        }

        return true;
    }

    int Database::insert_document(const string &file_path) {
        if(!db_handle) return -1;

        const char* sql = "INSERT OR IGNORE INTO documents (file_path) VALUES (?);";
        sqlite3_stmt* stmt;

        if(sqlite3_prepare_v2(db_handle, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            return -1;
        }

        sqlite3_bind_text(stmt, 1, file_path.c_str(), -1, SQLITE_TRANSIENT);

        int exit = sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        const char* query_sql = "SELECT id FROM documents WHERE file_path = ?;";

        if(sqlite3_prepare_v2(db_handle, query_sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, file_path.c_str(), -1, SQLITE_TRANSIENT);
            if(sqlite3_step(stmt) == SQLITE_ROW) {
                int id = sqlite3_column_int(stmt, 0);
                sqlite3_finalize(stmt);
                return id;
            }
            sqlite3_finalize(stmt);
        }
        return -1;
    }

    bool Database::insert_tokens(int document_id, const vector<TokenMatch> &tokens) {
        if(!db_handle || tokens.empty()) return false;

        sqlite3_exec(db_handle, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);

        const char *sql = "INSERT INTO inverted_index (token, document_id, line_number) VALUES (?, ?, ?);";
        sqlite3_stmt* stmt;

        if(sqlite3_prepare_v2(db_handle, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            sqlite3_exec(db_handle, "ROLLBACK;", nullptr, nullptr, nullptr);
            return false;
        }

        for(const auto &match: tokens) {
            string token(match.token);
            transform(token.begin(), token.end(), token.begin(), [](unsigned char c) {return tolower(c);});

            sqlite3_bind_text(stmt, 1, token.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(stmt, 2, document_id);
            sqlite3_bind_int(stmt, 3, match.line_number);

            sqlite3_step(stmt);
            sqlite3_reset(stmt);
        }

        sqlite3_finalize(stmt);

        sqlite3_exec(db_handle, "COMMIT;", nullptr, nullptr, nullptr);
        return true;
    }

    void Database::print_inverted_index() {
        if(!db_handle) return;

        const char* sql = 
            "SELECT i.token, d.file_path, i.line_number "
            "FROM inverted_index i "
            "JOIN documents d ON i.document_id = d.id "
            "ORDER BY i.token ASC, i.line_number ASC;";

        sqlite3_stmt* stmt;

        if (sqlite3_prepare_v2(db_handle, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            cerr << "Failed to execute print query: " << sqlite3_errmsg(db_handle) << endl;
            return;
        }

        cout << "=================== INVERTED INDEX DATABASE ===================" << endl;
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
}