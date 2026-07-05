#include "crawler.hpp"
#include "tokenizer.hpp"
#include "database.hpp"
#include "queryEngine.hpp"
#include <iostream>
#include <sqlite3.h>
#include <imgui.h>
#include <chrono>
using namespace std;

void run_query_engine(Engine::Database &db) {
    cout << "\nInitializing Query Engine Subsystem..." << endl;
    Engine::QueryEngine query_engine(db);
    if(query_engine.init()) {
        cout << "System ready. Enter search terms (or type 'exit'):" << endl;
        while(true) {
            cout << "Search ❯ ";
            string user_query;
            if(!getline(cin, user_query) || user_query == "exit") break;

            auto matches = query_engine.searchPhrase(user_query);
            cout << "Found " << matches.size() << " occurrences:\n";
            for(size_t i = 0; i < min(matches.size(), size_t(15)); ++i) {
                cout << "  -> Match: " << matches[i].file_path 
                     << " [Line: " << matches[i].line_number 
                     << "] (Score: " << matches[i].score << ")\n";
            }
        }
    }
}
int main() {
    
    // ios::sync_with_stdio(false);
    // cin.tie(nullptr);
    
    auto start_time = chrono::high_resolution_clock::now();
    
    cout << "========================================" << "\n";
    cout << "SQLite Version: " << sqlite3_libversion() << "\n";

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    
    ImGui::DestroyContext();


    cout<<"crawler test starting"<<endl;
    string file_path = "/home/salik/programming";
    Engine::Database db("test.db");
    
    if(!db.init()) {
        cout << "db initializtion failed" << endl;
        return 0;
    }
    Engine::Crawler crawler;
    crawler.run_crawler(file_path, db);
    run_query_engine(db);

    db.print_db_size("test.db");

    auto end_time = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::seconds>(end_time - start_time).count();


    cout << "Execution Time: " << duration << " seconds.\n";
    
    return 0;
}
