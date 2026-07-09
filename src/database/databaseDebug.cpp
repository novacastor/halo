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
            LOG_ERROR(std::string("Failed to prepare pring query statement: ") + sqlite3_errmsg(db_handle));
            return;
        }
    
        LOG_INFO("=================== INVERTED INDEX DATABASE ===================");
        LOG_INFO("TOKEN               | LINE | FILE PATH");
        LOG_INFO("---------------------------------------------------------------");

        while(sqlite3_step(stmt) == SQLITE_ROW) {
            const unsigned char* token = sqlite3_column_text(stmt, 0);
            const unsigned char* path  = sqlite3_column_text(stmt, 1);
            int line                   = sqlite3_column_int(stmt, 2);

            std::ostringstream line_out;
            line_out << std::left << std::setw(19) << reinterpret_cast<const char*>(token)
                    << " | " << std::setw(4) << line << " | "
                    << reinterpret_cast<const char*>(path);
            LOG_INFO(line_out.str());
        }
        LOG_INFO("===============================================================");
        sqlite3_finalize(stmt);
    }

    void print_db_size(const string &db_path) {
        try {
            if (filesystem::exists(db_path)) {
                uintmax_t file_size_bytes = filesystem::file_size(db_path);
                double file_size_kb = static_cast<double>(file_size_bytes) / 1024.0;
                double file_size_mb = file_size_kb / 1024.0;

                LOG_INFO("========================================");
                LOG_INFO("DATABASE STORAGE METRICS:");
                LOG_INFO("----------------------------------------");
                LOG_INFO("Database File: " + db_path);
                
                std::ostringstream line_out;
                if (file_size_mb >= 1.0) {
                    line_out << "Disk Footprint: " << fixed << setprecision(2) << file_size_mb << "MB";
                    LOG_INFO(line_out.str());
                } else {
                    line_out << "Disk Footprint: " << fixed << setprecision(2) << file_size_kb << "KB (" << file_size_bytes << "bytes)";
                    LOG_INFO(line_out.str());
                }
                LOG_INFO("========================================");
            } else {
                LOG_ERROR("Database file not found: " + db_path);
            }
        } catch (const filesystem::filesystem_error& e) {
            LOG_ERROR("Filesystem Error checking DB size: " + std::string(e.what()));
        }
    }
}