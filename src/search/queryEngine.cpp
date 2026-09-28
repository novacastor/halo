#include "search/queryEngine.hpp"
#include "indexing/tokenizer.hpp"
#include "engine/log.hpp"

namespace Engine {
    QueryEngine::QueryEngine(Engine::Database &db) : db(db) {}

    bool QueryEngine::init() {
        LOG_INFO("Query engine initialized.");
        return true;
    }

    std::vector<MatchResult> QueryEngine::search_terms(const std::string &query_text) {
        auto query_tokens = Tokenizer::tokenize(query_text);
        if(query_tokens.empty()) {
            return {};
        }
        return db.execute_terms_search(query_tokens, 200);
    }

    std::vector<FileMatch> QueryEngine::search_filename(const std::string &file_name) {
        if(file_name.empty()) {
            return {};
        }

        return db.execute_filename_search(file_name);
    }
}
