#include "database/database.hpp"

namespace Engine {
    bool Database::insert_file(const std::string &name, const std::string &ext, const std::string &path) {
        std::lock_guard<std::mutex> lock(db_mutex);

        if(!upsert_fs_stmt) return false;

        sqlite3_reset(upsert_fs_stmt);

        sqlite3_bind_text(upsert_fs_stmt, 1, name.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(upsert_fs_stmt, 2, ext.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(upsert_fs_stmt, 3, path.c_str(), -1, SQLITE_STATIC);

        if(sqlite3_step(upsert_fs_stmt) != SQLITE_DONE) {
            LOG_ERROR(std::string("DB error can't upsert file ") + sqlite3_errmsg(db_handle));
            return false;
        }

        return true;
    }

    bool Database::delete_file(const std::string &path) {
        std::lock_guard<std::mutex> lock(db_mutex);

        if(!delete_fs_stmt) return false;

        sqlite3_reset(delete_fs_stmt);    
        sqlite3_bind_text(delete_fs_stmt, 1, path.c_str(), -1, SQLITE_STATIC);

        if(sqlite3_step(delete_fs_stmt) != SQLITE_DONE) {
            LOG_ERROR(std::string("DB error can't delete file ") + sqlite3_errmsg(db_handle));
            return false;
        }

        return true;
    }
    
    bool Database::delete_directory(const std::string &dir_path) {
        std::lock_guard<std::mutex> lock(db_mutex);
        
        if(!delete_fs_dir_stmt) return false;
        
        sqlite3_reset(delete_fs_dir_stmt);    
        
        std::string wildcard_path = dir_path;
        if(wildcard_path.back() != '/') {
            wildcard_path += '/';
        }
        wildcard_path += '%';
        
        sqlite3_bind_text(delete_fs_dir_stmt, 1, dir_path.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(delete_fs_dir_stmt, 2, wildcard_path.c_str(), -1, SQLITE_STATIC);
        
        if(sqlite3_step(delete_fs_dir_stmt) != SQLITE_DONE) {
            LOG_ERROR(std::string("DB error can't delete directory ") + sqlite3_errmsg(db_handle));
            return false;
        }
        
        return true;
    }

    bool Database::add_directory(const std::string &dir_path) {
        std::lock_guard<std::mutex> lock(db_mutex);

        if(!add_fs_dir_stmt) return false;

        sqlite3_reset(add_fs_dir_stmt);

        sqlite3_bind_text(add_fs_dir_stmt, 1, dir_path.c_str(), -1, SQLITE_STATIC);

        if(sqlite3_step(add_fs_dir_stmt) != SQLITE_DONE) {
            LOG_ERROR(std::string("DB error can't add directory: " + dir_path) + sqlite3_errmsg(db_handle));
            return false;
        }

        return true;
    }
    
    void Database::commit_filesystem_index(const std::vector<FSEntry> &files) {
        if(files.empty()) return;

        begin_transaction();

        for(const auto &file: files) {
            if(!insert_file(file.name, file.ext, file.path)) {
                commit_transaction();
                return;
            }
        }

        commit_transaction();

        LOG_INFO("Finished indexing filesystem. ");
    }

    bool Database::file_is_up_to_date(const std::string& path, long long mtime) const {
        auto it = existing_mtimes.find(path);
        if(it == existing_mtimes.end() || it->second != mtime) {
            return false;
        } 
        return true;
    }
}