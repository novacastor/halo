#include "engine/engineApp.hpp"

int main() {
    
    // ios::sync_with_stdio(false);
    // cin.tie(nullptr);
    
    
    std::cout << "========================================" << "\n";
    std::cout << "SQLite Version: " << sqlite3_libversion() << "\n";

    // IMGUI_CHECKVERSION();
    // ImGui::CreateContext();
    
    // ImGui::DestroyContext();
    
    Engine::App app;
    if(!app.init()) {
        return EXIT_FAILURE;
    }

    app.run();
    return 0;
}
