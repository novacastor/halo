#include "crawler.hpp"
#include "tokenizer.hpp"
#include "database.hpp"
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


string open_file(const string &path) {
    ifstream file(path);
    if(!file.is_open()) {
        cerr<<"Can't open file (Doesn't exist or Permission denied)\n";
        return "";
    }
    
    stringstream buffer;
    buffer<<file.rdbuf();
    string contents = buffer.str();
    
    file.close();
    
    return contents;
}
void run_crawler (const string &target_path) {
    auto start_time = chrono::high_resolution_clock::now();

    Engine::Database db("test.db");

    if(!db.init()) {
        cout << "db initializtion failed" << endl;
        return;
    }
    
    fs::path root_path(target_path);
    int total_files_opened = 0, total_content_size = 0;
    
    cout<<"Tokens Parsed Start: ";
    for(auto it = fs::recursive_directory_iterator(root_path, fs::directory_options::skip_permission_denied); it != fs::end(it); ++it) {
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

        string file_contents = open_file(absolute_path);

        if(!file_contents.empty()) {
            auto tokens = Engine::Tokenizer::tokenize(file_contents);
            
            int document_id = db.insert_document(absolute_path);
            if(document_id != -1) {
                db.insert_tokens(document_id, tokens);
            }
        }

        const char* sql = 
            "SELECT i.token, d.file_path, i.line_number "
            "FROM inverted_index i "
            "JOIN documents d ON i.document_id = d.id "
            "ORDER BY i.token ASC, i.line_number ASC;";

        sqlite3_stmt* stmt;

        total_content_size += file_contents.size();
        total_files_opened++;
    }

    db.print_inverted_index();

    auto end_time = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::microseconds>(end_time - start_time).count();

    cout<<"Tokens Parsed End: ";

    cout << "Execution Time: " << duration << " microseconds.\n";
    cout << "Total Files opened: " << total_files_opened << endl;
    cout << "Total Data processed: " << total_content_size << " bytes " << endl;
}

/*
cmake --build build
./build/search_engine
*/