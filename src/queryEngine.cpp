#include "queryEngine.hpp"
#include "tokenizer.hpp"
#include <iostream>
#include <unordered_map>
#include <algorithm>

using namespace std;

namespace Engine {
    QueryEngine::QueryEngine(Engine::Database &db) : db_handle(db.get_db_handle()) {}
    QueryEngine::~QueryEngine() {
        if(search_stmt) sqlite3_finalize(search_stmt);
    }

    bool QueryEngine::init() {
        if(!db_handle) return false;

        const char* sql = 
            "SELECT d.file_path, i.line_number "
            "FROM inverted_index i "
            "JOIN tokens t ON i.token_id = t.id "
            "JOIN documents d ON i.document_id = d.id "
            "WHERE t.text = ?;";
        
        if(sqlite3_prepare_v2(db_handle, sql, -1, &search_stmt, nullptr) != SQLITE_OK) {
            cerr << "Failed to prepare search statement " << endl;
            return false;
        }
        return true;
    }

    vector<MatchResult> QueryEngine::searchToken(const string &query_token) {
        vector<MatchResult> results;
        if(!search_stmt) return results;

        sqlite3_reset(search_stmt);
        sqlite3_bind_text(search_stmt, 1, query_token.c_str(), -1, SQLITE_STATIC);

        while(sqlite3_step(search_stmt) == SQLITE_ROW) {
            string path = reinterpret_cast<const char*>(sqlite3_column_text(search_stmt, 0));
            int line = sqlite3_column_int(search_stmt, 1);

            results.push_back({path, line, 1});
        }

        return results;
    }

    vector<MatchResult> QueryEngine::searchPhrase(const string &query_phrase) {
        vector<MatchResult> results;
        if(!search_stmt) return results;

        auto search_token_matches = Tokenizer::tokenize(query_phrase);
        if(search_token_matches.empty()) {
            return results;
        }

        unordered_map<string, MatchResult> scoring_map;

        for(const auto &match: search_token_matches) {
            auto match_results = searchToken(match.token);

            for(const auto &item: match_results) {
                string key = item.file_path + ":" + to_string(item.line_number);
                if(scoring_map.find(key) == scoring_map.end()) {
                    scoring_map[key] = item;
                }else{
                    scoring_map[key].score++;
                }
            }
        }

        vector<MatchResult> ranked_results;
        for(const auto &[key, match]: scoring_map) {
            ranked_results.push_back(match);
        }

        sort(ranked_results.begin(), ranked_results.end(), [&](const MatchResult &a, const MatchResult &b) {
            if(a.score != b.score) return a.score > b.score;
            return a.file_path < b.file_path;
        });

        return ranked_results;
    }
}