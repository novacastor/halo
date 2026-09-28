#include "engine/engineApp.hpp"
#include "ui/ui.hpp"
#include "engine/log.hpp"
#include <csignal>
#include <cstdlib>
#include <filesystem>

std::atomic<bool> g_shutdown_requested{false};
void handle_signal(int) { g_shutdown_requested.store(true); }

int main() {
    
    std::signal(SIGINT, handle_signal);
    std::signal(SIGTERM, handle_signal);
    
    std::filesystem::path state_directory;
    if (const char* xdg_state_home = std::getenv("XDG_STATE_HOME");
        xdg_state_home && *xdg_state_home) {
        state_directory = xdg_state_home;
    } else {
        state_directory = std::filesystem::path(Engine::get_home_directory()) / ".local" / "state";
    }
    const auto log_directory = state_directory / "halo";
    std::error_code filesystem_error;
    std::filesystem::create_directories(log_directory, filesystem_error);
    Engine::Log::init((log_directory / "halo.log").string());
    LOG_INFO("Initializing Subsystems. ");
    
    Engine::App app;
    if (!app.init()) {
        LOG_ERROR("Failed to initialize Halo Core Subsystems. ");
        return EXIT_FAILURE;
    }

    Engine::UI ui(app);
    if (!ui.init(1100, 720, "Halo Engine")) {
        LOG_ERROR("UI subsystem initialization failed.");
        return EXIT_FAILURE;
    }


    ui.run();

    Engine::Log::shutdown();

    return EXIT_SUCCESS;
}
