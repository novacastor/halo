#pragma once
#include "database/database.hpp"
#include "engine/types.hpp"
#include <string>
#include <vector>

namespace Engine {
    class QueryEngine {
    public:
        QueryEngine(Engine::Database &db);

        bool init();
        std::vector<FileMatch> search_filename(const std::string &file_name);
        std::vector<MatchResult> search_phrase(const std::string &query_phrase);
    private:
        Database &db;
    };
}