#pragma once
#include "database.hpp"
#include <string>
#include <vector>

using namespace std;

namespace Engine {
    struct MatchResult {
        string file_path;
        int line_number;
        int score;
    };

    class QueryEngine {
    public:
        QueryEngine(Engine::Database &db);
        ~QueryEngine();

        bool init();
        vector<MatchResult> searchToken(const string &query_token);
        vector<MatchResult> searchPhrase(const string &query_phrase);

    private:
        sqlite3 *db_handle;
        sqlite3_stmt *search_stmt = nullptr;
    };
}