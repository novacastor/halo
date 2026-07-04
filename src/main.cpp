#include "crawler.hpp"
#include "tokenizer.hpp"
#include "database.hpp"
#include <iostream>
#include <sqlite3.h>
#include <imgui.h>
#include <chrono>
using namespace std;

int main() {
    
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    
    auto start_time = chrono::high_resolution_clock::now();
    
    cout << "========================================" << "\n";
    cout << "SQLite Version: " << sqlite3_libversion() << "\n";

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    
    ImGui::DestroyContext();


    cout<<"crawler test starting"<<endl;
    string file_path = "/home/salik/.config/nvm";
    Engine::Database db("test.db");
    
    if(!db.init()) {
        cout << "db initializtion failed" << endl;
        return 0;
    }
    sqlite3 *db_handle = db.get_db_handle();

    auto [total_files_opened, total_content_size] =  run_crawler(file_path, db);

    db.print_db_size("test.db");

    auto end_time = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::seconds>(end_time - start_time).count();


    cout << "Execution Time: " << duration << " seconds.\n";
    cout << "Total Files opened: " << total_files_opened << endl;
    cout << "Total Data processed: " << total_content_size << " bytes " << endl;
    
    return 0;
}
