#pragma once

#include "engine/engineApp.hpp"
#include "engine/types.hpp"
#include <string>
#include <vector>
#include <future>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <variant>

struct GLFWwindow;
struct ImFont;

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
        struct SearchResponse {
            std::uint64_t generation;
            int mode;
            std::variant<std::vector<MatchResult>, std::vector<FileMatch>> results;
        };

        void apply_custom_style(float scale);
        void render_frame();

        void draw_header_region();
        void draw_search_region();
        void draw_mode_switch();

        void load_font(float scale);
        static void on_focus_changed(GLFWwindow* window, int focused);

        void open_in_editor(const std::string& file_path, int line_number);

        Engine::App& app;
        GLFWwindow* window = nullptr;
        bool window_focused = true;
        bool has_presented_frame = false;

        char search_buffer[1024] = "";
        int mode_toggle = 0;

        float ui_scale = 1.0f;
        ImFont* body_font = nullptr;
        ImFont* display_font = nullptr;

        std::vector<Engine::MatchResult> content_results;
        std::vector<Engine::FileMatch> filename_results;

        std::future<SearchResponse> search_future;
        std::string last_search_query;
        int last_search_mode = -1;
        std::uint64_t search_generation = 0;
        std::uint64_t active_search_generation = 0;
        bool pending_search = false;
        std::atomic<bool> search_in_progress{false};
    };
}
