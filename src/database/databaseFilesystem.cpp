#include "database/database.hpp"
#include <iostream>

using namespace std;

namespace Engine {
    bool Database::insert_file(const string &name, const string &ext, const string &path) {
        lock_guard<mutex> lock(db_mutex);

        if(!upsert_fs_stmt) return false;

        sqlite3_reset(upsert_fs_stmt);

        sqlite3_bind_text(upsert_fs_stmt, 1, name.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(upsert_fs_stmt, 2, ext.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(upsert_fs_stmt, 3, path.c_str(), -1, SQLITE_STATIC);

        if(sqlite3_step(upsert_fs_stmt) != SQLITE_DONE) {
            cerr << " DB error (insert file): " << sqlite3_errmsg(db_handle) << endl;
            return false;
        }

        return true;
    }

    bool Database::delete_file(const string &path) {
        lock_guard<mutex> lock(db_mutex);

        if(!delete_fs_stmt) return false;

        sqlite3_reset(delete_fs_stmt);    
        sqlite3_bind_text(delete_fs_stmt, 1, path.c_str(), -1, SQLITE_STATIC);

        if(sqlite3_step(delete_fs_stmt) != SQLITE_DONE) {
            cerr << "Inotify DB error (delete file): " << sqlite3_errmsg(db_handle) << endl;
            return false;
        }

        return true;
    }
    
    bool Database::delete_directory(const string &dir_path) {
        lock_guard<mutex> lock(db_mutex);
        
        if(!delete_fs_dir_stmt) return false;
        
        sqlite3_reset(delete_fs_dir_stmt);    
        
        string wildcard_path = dir_path;
        if(wildcard_path.back() != '/') {
            wildcard_path += '/';
        }
        wildcard_path += '%';
        
        sqlite3_bind_text(delete_fs_dir_stmt, 1, dir_path.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(delete_fs_dir_stmt, 2, wildcard_path.c_str(), -1, SQLITE_STATIC);
        
        if(sqlite3_step(delete_fs_dir_stmt) != SQLITE_DONE) {
            cerr << "Inotify DB error (delete directory): " << sqlite3_errmsg(db_handle) << endl;
            return false;
        }
        
        return true;
    }
    
    void Database::commit_filesystem_index(const vector<FSEntry> &files) {
        if(files.empty()) return;

        begin_transaction();

        for(const auto &file: files) {
            if(!insert_file(file.name, file.ext, file.path)) {
                return;
            }
        }

        commit_transaction();
    }

    bool Database::file_is_up_to_date(const std::string& path, long long mtime) const {
        auto it = existing_mtimes.find(path);
        if(it == existing_mtimes.end() || it->second != mtime) {
            return false;
        } 
        return true;
    }
}