#include "crawler.hpp"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <chrono>
#include <unordered_set>

namespace fs = std::filesystem;
using namespace std;

const unordered_set<string> EXTENSION_WHITELIST = {
    ".cpp", ".hpp", ".h", ".c", ".cc", ".cxx",
    ".py", ".sh", ".bash", ".lua",
    ".js", ".jsx", ".ts", ".tsx", ".html", ".css",
    ".json", ".yaml", ".yml", ".toml", ".xml", ".ini",
    ".md", ".txt", "dotfiles" 
};

const unordered_set<string> FOLDER_BLACKLIST = {
    ".git", "build", ".vscode", "node_modules"
};

int open_file(const string &path) {
    ifstream file(path);
    if(!file.is_open()) {
        cerr<<"Can't open file (Doesn't exist or Permission denied)\n";
        return 0;
    }

    stringstream buffer;
    buffer<<file.rdbuf();
    string contents = buffer.str();

    file.close();

    // cout << "--- Memory Buffer Content Start ---\n";
    // cout <<contents;
    // cout << "--- Memory Buffer Content End ---\n";
    // cout << "Total bytes loaded: " << contents.size() << " bytes.\n";
    return contents.size();
}
void run_crawler (const string &target_path) {
    auto start_time = chrono::high_resolution_clock::now();
    
    fs::path root_path(target_path);
    int total_files_opened = 0, total_content_size = 0;

    for(auto it = fs::recursive_directory_iterator(
        root_path, fs::directory_options::skip_permission_denied
    ); 
    it != fs::end(it); ++it) {
        const auto &entry = *it;

        if(entry.is_directory()) {
            string folder_name = entry.path().filename().string();
            if(FOLDER_BLACKLIST.find(folder_name) != FOLDER_BLACKLIST.end()) {
                it.disable_recursion_pending();
            }
            continue;
        }

        if(!entry.is_regular_file()) continue;
        
        string ext = entry.path().extension().string();
        if(EXTENSION_WHITELIST.find(ext) == EXTENSION_WHITELIST.end()) continue;

        string absolute_path = fs::absolute(entry.path()).string();

        total_content_size += open_file(absolute_path);
        total_files_opened++;
    }

    auto end_time = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::microseconds>(end_time - start_time).count();

    cout << "Execution Time: " << duration << " microseconds.\n";
    cout << "Total Files opened: " << total_files_opened << endl;
    cout << "Total Data processed: " << total_content_size << " bytes " << endl;
}