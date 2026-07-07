#include "search/queryEngine.hpp"
#include "indexing/tokenizer.hpp"
#include <iostream>
#include <unordered_map>
#include <algorithm>
#include <iostream>

using namespace std;

static string escape_like(const string &input) {
    string escaped;
    escaped.reserve(input.size());
    for(char c : input) {
        if(c == '%' || c == '_' || c == '\\') escaped += '\\';
        escaped += c;
    }
    return escaped;
}

namespace Engine {
    QueryEngine::QueryEngine(Engine::Database &db) : db(db) {}
    QueryEngine::~QueryEngine() {
        if(search_phrase_stmt) sqlite3_finalize(search_phrase_stmt);
        if(search_filename_stmt) sqlite3_finalize(search_filename_stmt);
    }

    bool QueryEngine::init() {
        const char* sql = 
            "SELECT d.file_path, i.line_number "
            "FROM inverted_index i "
            "JOIN tokens t ON i.token_id = t.id "
            "JOIN documents d ON i.document_id = d.id "
            "WHERE t.text = ?;";
        
        if(sqlite3_prepare_v2(db.get_db_handle(), sql, -1, &search_phrase_stmt, nullptr) != SQLITE_OK) {
            cerr << "Failed to prepare search statement " << sqlite3_errmsg(db.get_db_handle()) << endl;
            return false;
        }

        const char *filename_sql = 
        "SELECT file_path, file_name FROM filesystem_index "
        "WHERE file_name LIKE ? ESCAPE '\\' "
        "ORDER BY file_name ASC;";

        if(sqlite3_prepare_v2(db.get_db_handle(), filename_sql, -1, &search_filename_stmt, nullptr) != SQLITE_OK) {
            cerr << "Failed to prepare file search statement " << sqlite3_errmsg(db.get_db_handle()) << endl;
            return false;
        }
        return true;
    }

    void QueryEngine::run() {
        cout << "System ready. Enter search terms (or type 'exit'):" << endl;
        int mode = 0;
        while(mode == 0) {
            cout << "Enter 1 to search filenames and 2 to search inside files: ";
            string input;
            if(!(cin >> input) || input == "exit") return;

            if(input == "1") {
                mode = 1;
            } else if(input == "2") {
                mode = 2;
            }
        }

        cin.ignore(numeric_limits<streamsize>::max(), '\n');

        while(mode) {
            cout << "Search ❯ ";
            string user_query;
            if(!getline(cin, user_query) || user_query == "exit") break;

            if(mode == 1) {
                auto matches = search_filename(user_query);
                cout << "Found " << matches.size() << " occurrences:\n";
                for(size_t i = 0; i < min(matches.size(), size_t(15)); ++i) {
                    cout << "  -> Match: " << matches[i].file_path << " Name: " << matches[i].file_name << " \n";
                }
            } else {
                auto matches = search_phrase(user_query);
                cout << "Found " << matches.size() << " occurrences:\n";
                for(size_t i = 0; i < min(matches.size(), size_t(15)); ++i) {
                    cout << "  -> Match: " << matches[i].file_path
                        << " [Line: " << matches[i].line_number
                        << "] (Score: " << matches[i].score << ")\n";
                }
            }
        }
    }

    vector<MatchResult> QueryEngine::search_token(const string &query_token) {
        vector<MatchResult> results;
        if(!search_phrase_stmt) return results;

        sqlite3_reset(search_phrase_stmt);
        sqlite3_bind_text(search_phrase_stmt, 1, query_token.c_str(), -1, SQLITE_STATIC);

        while(sqlite3_step(search_phrase_stmt) == SQLITE_ROW) {
            string path = reinterpret_cast<const char*>(sqlite3_column_text(search_phrase_stmt, 0));
            int line = sqlite3_column_int(search_phrase_stmt, 1);

            results.push_back({path, line, 1});
        }

        return results;
    }

    vector<MatchResult> QueryEngine::search_phrase(const string &query_phrase) {
        vector<MatchResult> match_results;
        if(!search_phrase_stmt) return match_results;

        auto search_token_matches = Tokenizer::tokenize(query_phrase);
        if(search_token_matches.empty()) {
            return match_results;
        }

        unordered_map<string, MatchResult> scoring_map;

        for(const auto &match: search_token_matches) {
            auto results = search_token(match.token);

            for(const auto &item: results) {
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

        sort(ranked_results.begin(), ranked_results.end(), [](const MatchResult &a, const MatchResult &b) {
            if(a.score != b.score) return a.score > b.score;
            return a.file_path < b.file_path;
        });

        return ranked_results;
    }

    vector<FileMatch> QueryEngine::search_filename(const string &file_name) {
        vector<FileMatch> results;
        if(!search_filename_stmt) return results;

        sqlite3_reset(search_filename_stmt);
        string pattern = "%" + escape_like(file_name) + "%";
        sqlite3_bind_text(search_filename_stmt, 1, pattern.c_str(), -1, SQLITE_TRANSIENT);

        while(sqlite3_step(search_filename_stmt) == SQLITE_ROW) {
            string path = reinterpret_cast<const char*>(sqlite3_column_text(search_filename_stmt, 0));
            string name = reinterpret_cast<const char*>(sqlite3_column_text(search_filename_stmt, 1));
            results.push_back({path, name});
        }

        return results;
    }
}