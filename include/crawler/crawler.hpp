#pragma once
#include <unordered_set>
#include <unordered_map>
#include <string>
#include <vector>
#include "engine/types.hpp"

struct StringHash {
    using is_transparent = void;
    size_t operator()(std::string_view sv) const { return std::hash<std::string_view>{}(sv); }
};

namespace Engine {
    
    class Crawler {
        public:
        CrawlBatch run_crawler(const std::string &target_path);
        CrawlBatch process_filesystem_crawl(const std::string &target_path);
        
        private:
        const std::unordered_set<std::string, StringHash, std::equal_to<>> EXTENSION_WHITELIST = {
            ".cpp", ".hpp", ".h", ".c", ".cc", ".cxx",
            ".py", ".sh", ".bash", ".lua",
            ".js", ".jsx", ".ts", ".tsx", ".html", ".css",
            ".json", ".yaml", ".yml", ".toml", ".xml", ".ini",
            ".md", ".txt"
        };
        
        const std::unordered_set<std::string, StringHash, std::equal_to<>> FOLDER_BLACKLIST = {
            ".git", ".svn", ".hg", ".vscode", ".idea",
            "build", "cmake-build-debug", "cmake-build-release", "cmake-build-relwithdebinfo", "cmake-build-minsizerel", "out", "dist", "target",
            "node_modules", ".npm", ".pnpm-store", ".yarn",
            "__pycache__", ".pytest_cache", ".mypy_cache", ".ruff_cache", ".tox", ".venv", "venv", "env",
            ".cargo", ".rustup", ".gradle", ".m2", ".cache", ".docker", ".vagrant", ".local", ".Trash", ".sass-cache"
        };
        
        const std::unordered_set<std::string, StringHash, std::equal_to<>> GLOBAL_FOLDER_BLACKLIST = { 
            ".cache", ".Trash", ".local/share" 
        };

    };
}
