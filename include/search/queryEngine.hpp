#pragma once
#include "database/database.hpp"
#include "database/databaseDebug.hpp"
#include "engine/types.hpp"
#include <string>
#include <vector>

namespace Engine {
    class QueryEngine {
    public:
        QueryEngine(Engine::Database &db);
        ~QueryEngine();

        bool init();
        void run();
    private:
        std::vector<MatchResult> search_token(const std::string &query_token);
        std::vector<FileMatch> search_filename(const std::string &file_name);
        std::vector<MatchResult> search_phrase(const std::string &query_phrase);
        Database &db;
        sqlite3_stmt *search_phrase_stmt = nullptr;
        sqlite3_stmt *search_filename_stmt = nullptr;
    };
}