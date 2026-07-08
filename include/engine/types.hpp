#pragma once

#include <string>
#include <vector>

namespace Engine {
    struct FSEntry {
        std::string name;
        std::string path;
        std::string ext;
    };

    struct CodeCandidate {
        std::string path;
        long long mtime;
    };

    struct CrawlBatch {
        std::vector<FSEntry> all_files;
        std::vector<CodeCandidate> code_files;
    };

    struct TokenMatch {
        std::string token;
        size_t line_number;
    };

    struct IndexJob {
        std::string path;
        std::vector<TokenMatch> tokens;
        bool is_poison_pill = false;
        long long mtime;
    };

    struct MatchResult {
        std::string file_path;
        int line_number;
        int score;
    };

    struct MatchCandidate {
        int document_id;
        int line_number;
        int score;
    };

    struct MatchKey {
        int document_id;
        int line_number;
        int score;

        bool operator==(const MatchKey &other) const {
            return document_id == other.document_id &&
                line_number == other.line_number;
        }
    };

    struct MatchKeyHash {
        size_t operator()(const MatchKey &k) const {
            return (static_cast<size_t>(k.document_id) << 32)
                ^ static_cast<size_t>(k.line_number);
        }
    };

    struct FileMatch {
        std::string file_path;
        std::string file_name;
    };
}