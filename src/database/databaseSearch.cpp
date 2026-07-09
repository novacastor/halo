#include "database/database.hpp"
#include <iostream>
using namespace std;

namespace Engine {
    static string escape_like(const string &input) {
        string escaped;
        escaped.reserve(input.size());
        for(char c : input) {
            if(c == '%' || c == '_' || c == '\\') escaped += '\\';
            escaped += c;
        }
        return escaped;
    }

    vector<MatchResult> Database::execute_phrase_search(const vector<TokenMatch>& query_tokens, int limit) {
        vector<MatchResult> result;
        if(query_tokens.empty()) return result;
        
        lock_guard<mutex> lock(db_mutex);
        string sql =
            "SELECT "
            "    d.file_path, "
            "    i.line_number, "
            "    (COUNT(*) * 100) + "
            "    CASE "
            "        WHEN d.file_path LIKE '%/.%' THEN -1000 "
            "        WHEN d.file_path LIKE '%/programming/%' COLLATE NOCASE THEN 50 "
            "        WHEN d.file_path LIKE '%/Projects/%' COLLATE NOCASE THEN 50 "
            "        WHEN d.file_path LIKE '%/src/%' COLLATE NOCASE THEN 50 "
            "        WHEN d.file_path LIKE '%/dotfiles/%' COLLATE NOCASE THEN 50 "
            "        WHEN d.file_path LIKE '%/nano/%' COLLATE NOCASE THEN 50 "
            "        WHEN d.file_path LIKE '%/Documents/%' COLLATE NOCASE THEN 50 "
            "        WHEN d.file_path LIKE '%/Downloads/%' COLLATE NOCASE THEN 50 "
            "        WHEN d.file_path LIKE '%/Desktop/%' COLLATE NOCASE THEN 50 "
            "        WHEN d.file_path LIKE '%/Pictures/%' COLLATE NOCASE THEN 50 "
            "        WHEN d.file_path LIKE '%/Videos/%' COLLATE NOCASE THEN 50 "
            "        WHEN d.file_path LIKE '%/Music/%' COLLATE NOCASE THEN 50 "
            "        WHEN d.file_path LIKE '%/Templates/%' COLLATE NOCASE THEN 50 "
            "        WHEN d.file_path LIKE '%/Public/%' COLLATE NOCASE THEN 50 "
            "        WHEN d.file_path LIKE '%/wallpaper/%' COLLATE NOCASE THEN 50 "
            "        ELSE 0 "
            "    END AS rank_score "
            "FROM inverted_index i "
            "JOIN tokens t ON i.token_id = t.id "
            "JOIN documents d ON i.document_id = d.id "
            "WHERE t.text IN (";

        for (size_t i = 0; i < query_tokens.size(); i++) {
            if (i > 0) sql += ", ";
            sql += "?";
        }

        sql +=
            ") "
            "GROUP BY i.document_id, i.line_number "
            "ORDER BY rank_score DESC "
            "LIMIT ?;";

        sqlite3_stmt *stmt = nullptr;
        if (sqlite3_prepare_v2(db_handle, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            cerr << "Search preparation error: " << sqlite3_errmsg(db_handle) << endl;
            return result;
        }

        for (size_t i = 0; i < query_tokens.size(); i++) {
            sqlite3_bind_text(stmt, i + 1, query_tokens[i].token.c_str(), -1, SQLITE_TRANSIENT);
        }
        
        sqlite3_bind_int(stmt, query_tokens.size() + 1, limit);

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            string file_path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            int line_number = sqlite3_column_int(stmt, 1);
            int matched_tokens = sqlite3_column_int(stmt, 2);

            result.push_back({
                file_path,
                line_number,
                matched_tokens
            });
        }

        sqlite3_finalize(stmt);
        return result;
    }

    vector<FileMatch> Database::execute_filename_search(const string &pattern) {
        vector<FileMatch> results;
        
        lock_guard<mutex> lock(db_mutex);

        const char *sql = 
            "SELECT file_path, file_name FROM filesystem_index "
            "WHERE file_name LIKE ? ESCAPE '\\' "
            "ORDER BY file_name ASC;";

        sqlite3_stmt *stmt = nullptr;
        if(sqlite3_prepare_v2(db_handle, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            cerr << "Failed to prepare file search statement: " << sqlite3_errmsg(db_handle) << endl;
            return results;
        }

        string like_pattern = "%" + escape_like(pattern) + "%";
        sqlite3_bind_text(stmt, 1, like_pattern.c_str(), -1, SQLITE_TRANSIENT);

        while(sqlite3_step(stmt) == SQLITE_ROW) {
            string path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            string name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            results.push_back({path, name});
        }

        sqlite3_finalize(stmt);
        return results;
    }
    
}