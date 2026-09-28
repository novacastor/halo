#include "database/database.hpp"
#include "database/databaseUtils.hpp"

#include <string>
#include <unordered_set>

namespace Engine {
    std::vector<MatchResult> Database::execute_terms_search(const std::vector<TokenMatch>& query_tokens, int limit) {
        std::vector<MatchResult> result;
        if(query_tokens.empty()) {
            LOG_ERROR("Query has no searchable terms; returning no matches.");
            return result;
        }
        std::lock_guard<std::mutex> lock(db_mutex);

        std::vector<std::string> query_terms;
        std::unordered_set<std::string> unique_terms;
        for (const auto& token : query_tokens) {
            if (unique_terms.insert(token.token).second) {
                query_terms.push_back(token.token);
            }
        }

        std::string sql =
            "SELECT "
            "    d.file_path, "
            "    i.line_number, "
            "    (COUNT(DISTINCT t.text) * 100) + "
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

        for (size_t i = 0; i < query_terms.size(); i++) {
            if (i > 0) sql += ", ";
            sql += "?";
        }

        sql +=
            ") "
            "GROUP BY i.document_id, i.line_number "
            "HAVING COUNT(DISTINCT t.text) = ? "
            "ORDER BY rank_score DESC "
            "LIMIT ?;";

        sqlite3_stmt *stmt = nullptr;
        if (sqlite3_prepare_v2(db_handle, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            LOG_ERROR(std::string("Query Search statement preparation failed ") + sqlite3_errmsg(db_handle));
            return result;
        }

        for (size_t i = 0; i < query_terms.size(); i++) {
            sqlite3_bind_text(stmt, i + 1, query_terms[i].c_str(), -1, SQLITE_TRANSIENT);
        }

        sqlite3_bind_int(stmt, query_terms.size() + 1, static_cast<int>(query_terms.size()));
        sqlite3_bind_int(stmt, query_terms.size() + 2, limit);

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            std::string file_path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
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

    std::vector<FileMatch> Database::execute_filename_search(const std::string &pattern) {
        std::vector<FileMatch> results;
        
        std::lock_guard<std::mutex> lock(db_mutex);

        const char *sql = 
            "SELECT file_path, file_name FROM filesystem_index "
            "WHERE file_name LIKE ? ESCAPE '\\' "
            "ORDER BY file_name ASC;";

        sqlite3_stmt *stmt = nullptr;
        if(sqlite3_prepare_v2(db_handle, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            LOG_ERROR(std::string("Filename search statement preparation failed ") + sqlite3_errmsg(db_handle));
            return results;
        }

        std::string like_pattern = "%" + escape_like_literal(pattern) + "%";
        sqlite3_bind_text(stmt, 1, like_pattern.c_str(), -1, SQLITE_TRANSIENT);

        while(sqlite3_step(stmt) == SQLITE_ROW) {
            std::string path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            std::string name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            results.push_back({path, name});
        }

        sqlite3_finalize(stmt);
        return results;
    }
    
}
