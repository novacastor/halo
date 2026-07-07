#include "database/databaseDebug.hpp"
#include <filesystem>
#include <iomanip>
#include <iostream>

using namespace std;

namespace Engine {
    void print_inverted_index(Database &db) {
        const auto db_handle = db.get_db_handle();
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

            cout << left << setw(19) << reinterpret_cast<const char*>(token) 
            << " | " << setw(4) << line << " | " 
            << reinterpret_cast<const char*>(path) << '\n';
        }
        cout << "===============================================================" << endl << endl;
        sqlite3_finalize(stmt);
    }

    void print_db_size(const string &db_path) {
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
                cerr << "Database file not found: " << db_path << '\n';
            }
        } catch (const filesystem::filesystem_error& e) {
            cerr << "Filesystem Error checking DB size: " << e.what() << "\n";
        }
    }
}