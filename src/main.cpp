#include "crawler.hpp"
#include <iostream>
#include <sqlite3.h>
#include <imgui.h>
using namespace std;

int main() {
    std::cout << "========================================" << "\n";
    std::cout << "SQLite Version: " << sqlite3_libversion() << "\n";

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    std::cout << "Dear ImGui Version: " << IMGUI_VERSION << "\n";
    
    std::cout << "========================================" << "\n";
    std::cout << "Day 1 Setup Successful! Ready to build." << "\n";
    
    ImGui::DestroyContext();

    cout<<"crawler test starting"<<endl;
    string file_path = "/home";
    run_crawler(file_path);
    return 0;
}
