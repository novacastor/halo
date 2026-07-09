#pragma once

#include "engine/engineApp.hpp"
#include "engine/types.hpp"
#include <string>
#include <vector>
#include <future>
#include <atomic>
#include <chrono>

struct GLFWwindow;

namespace Engine {
    class UI {
    public:
        UI(Engine::App& app);
        ~UI();

        UI(const UI&) = delete;
        UI& operator=(const UI&) = delete;

        bool init(int width = 960, int height = 600, const std::string& title = "Halo Engine Console");
        void run();

    private:
        void apply_custom_style(float scale);
        void render_frame();

        void draw_header_region();
        void draw_search_region();
        void draw_mode_switch();

        // Font / DPI handling. On Wayland the content scale can change at runtime
        // (e.g. dragging the window to a monitor with a different scale factor in
        // Hyprland), so fonts need to be reloadable, not just baked once at startup.
        void load_font(float scale);
        void rebuild_fonts();
        static void on_content_scale_changed(GLFWwindow* window, float xscale, float yscale);

        void open_in_editor(const std::string& file_path, int line_number);

        Engine::App& app;
        GLFWwindow* window = nullptr;

        char search_buffer[1024] = "";
        int mode_toggle = 0;

        float ui_scale = 1.0f;
        bool fonts_dirty = false;

        std::vector<Engine::MatchResult> content_results;
        std::vector<Engine::FileMatch> filename_results;

        std::future<std::vector<Engine::MatchResult>> future_content_results;
        std::future<std::vector<Engine::FileMatch>> future_filename_results;
        std::atomic<bool> search_in_progress{false};
    };
}