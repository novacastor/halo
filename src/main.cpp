#include "crawler.hpp"
#include "tokenizer.hpp"
#include <iostream>
#include <sqlite3.h>
#include <imgui.h>
using namespace std;

int main() {
    
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    
    std::cout << "========================================" << "\n";
    std::cout << "SQLite Version: " << sqlite3_libversion() << "\n";

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    std::cout << "Dear ImGui Version: " << IMGUI_VERSION << "\n";
    
    std::cout << "========================================" << "\n";
    std::cout << "Day 1 Setup Successful! Ready to build." << "\n";
    
    ImGui::DestroyContext();

    cout<<"crawler test starting"<<endl;
    string file_path = "/home/salik/programming/cpp/dsa/codeforces/contests";
    run_crawler(file_path);

    
    return 0;
}
