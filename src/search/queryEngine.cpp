#include "search/queryEngine.hpp"
#include "indexing/tokenizer.hpp"
#include "engine/log.hpp"

namespace Engine {
    QueryEngine::QueryEngine(Engine::Database &db) : db(db) {}

    bool QueryEngine::init() {
        LOG_INFO("Query Engine successfully initalized. ");
        return true;
    }

    std::vector<MatchResult> QueryEngine::search_phrase(const std::string &query_phrase) {
        auto query_tokens = Tokenizer::tokenize(query_phrase);
        if(query_tokens.empty()) {
            return {};
        }
        return db.execute_phrase_search(query_tokens, 200);
    }

    std::vector<FileMatch> QueryEngine::search_filename(const std::string &file_name) {
        if(file_name.empty()) {
            return {};
        }

        return db.execute_filename_search(file_name);
    }
}