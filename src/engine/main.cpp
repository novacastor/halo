#include "engine/engineApp.hpp"
#include "ui/ui.hpp"

int main() {
    
    std::cout << "========================================" << "\n";
    std::cout << "SQLite Version: " << sqlite3_libversion() << "\n";
    
    Engine::App app;
    if (!app.init()) {
        std::cerr << "Failed to initialize Halo Core Subsystems.\n";
        return EXIT_FAILURE;
    }

    Engine::UI ui(app);
    if (!ui.init(960, 600, "Halo Engine")) {
        std::cerr << "CRITICAL: UI subsystem context instantiation failed.\n";
        return EXIT_FAILURE;
    }

    ui.run();

    return EXIT_SUCCESS;
}
