#include "engine/engineApp.hpp"
#include "ui/ui.hpp"
#include "engine/log.hpp"
#include <csignal>

std::atomic<bool> g_shutdown_requested{false};
void handle_signal(int) { g_shutdown_requested.store(true); }

int main() {
    
    std::signal(SIGINT, handle_signal);
    std::signal(SIGTERM, handle_signal);
    
    Engine::Log::init(std::string(Engine::get_home_directory()) + "/programming/projects/search_engine/halo.log");
    LOG_INFO("Initializing Subsystems. ");
    
    Engine::App app;
    if (!app.init()) {
        LOG_ERROR("Failed to initialize Halo Core Subsystems. ");
        return EXIT_FAILURE;
    }

    Engine::UI ui(app);
    if (!ui.init(960, 600, "Halo Engine")) {
        LOG_ERROR("Critial: UI subsystem context instantiation failed. ");
        return EXIT_FAILURE;
    }


    ui.run();

    Engine::Log::shutdown();

    return EXIT_SUCCESS;
}
