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
        // Returns lines containing every distinct query term; term order is ignored.
        std::vector<MatchResult> search_terms(const std::string &query_text);
    private:
        Database &db;
    };
}
