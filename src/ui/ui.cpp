#include "ui/ui.hpp"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <GLFW/glfw3.h>
#include <filesystem>
#include <sys/types.h>
#include <spawn.h>
#include <sys/wait.h>
#include <cerrno>
#include <cstring>
#include <exception>
#include <thread>
#include <utility>
#include <algorithm>

extern char** environ;

extern std::atomic<bool> g_shutdown_requested;
namespace Engine {

    UI::UI(Engine::App& app) : app(app) {}

    UI::~UI() {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        if (window) {
            glfwDestroyWindow(window);
        }
        glfwTerminate();
    }

    bool UI::init(int width, int height, const std::string& title) {
        const char* wayland_display = std::getenv("WAYLAND_DISPLAY");

        if (wayland_display != nullptr && glfwPlatformSupported(GLFW_PLATFORM_WAYLAND)) {
            glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_WAYLAND);
            LOG_INFO("Configured Wayland Platform");
        }else if (glfwPlatformSupported(GLFW_PLATFORM_X11)) {
            glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
            LOG_INFO("Configured X11 Platform");
        } else {
            std::cerr << "NOTE: This GLFW build has no Wayland backend compiled in, "
                          "so it will fall back to X11/XWayland (blurry text on a "
                          "scaled Hyprland output). To fix at the root: add Wayland "
                          "support to the glfw3 vcpkg port (its Linux build needs "
                          "wayland-protocols/libxkbcommon/wayland-client/wayland-cursor "
                          "dev packages present at build time, e.g. via pacman) and "
                          "rebuild.\n";
        }

        if (!glfwInit()) {
            LOG_ERROR("CRITICAL: Failed to initialize GLFW");
            return false;
        }

        glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
        if (!window) {
            LOG_ERROR("CRITICAL: Failed to create GLFW window context");
            glfwTerminate();
            return false;
        }
        glfwMakeContextCurrent(window);
        glfwSwapInterval(1);

        glfwSetWindowUserPointer(window, this);
        glfwSetWindowFocusCallback(window, &UI::on_focus_changed);
        glfwSetWindowSizeLimits(window, 820, 560, GLFW_DONT_CARE, GLFW_DONT_CARE);
        window_focused = glfwGetWindowAttrib(window, GLFW_FOCUSED) == GLFW_TRUE;

        LOG_INFO("GLFW window context created successfully. ");
        // Wayland delivers the surface's content-scale asynchronously after the
        // window is mapped, so querying it immediately after creation can still
        // return a stale 1.0 on some compositors. Pumping events a couple of times
        // flushes that initial scale event before we bake the font atlas at the
        // wrong pixel size.
        glfwPollEvents();
        glfwPollEvents();

        // ImGui uses GLFW's logical window coordinates. The OpenGL backend
        // handles the framebuffer scale, so scaling widget metrics again here
        // makes layouts overflow on HiDPI Wayland monitors.
        ui_scale = 1.0f;

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui::StyleColorsLight();

        apply_custom_style(1.0f);
        load_font(1.0f);

        // Initialize backends — ImGui_ImplOpenGL3_Init() builds the font atlas
        // texture from whatever's in io.Fonts at this point.
        ImGui_ImplGlfw_InitForOpenGL(window, true);
        ImGui_ImplOpenGL3_Init("#version 330");
        glfwShowWindow(window);

        app.build_search_index();

        return true;
    }

    void UI::load_font(float scale) {
        ImGuiIO& io = ImGui::GetIO();
        static const char* candidates[] = {
            "/System/Library/Fonts/SFNS.ttf",
            "/usr/share/fonts/Inter/Inter-Regular.ttf",
            "/usr/share/fonts/inter/Inter-Regular.ttf",
            "/usr/share/fonts/TTF/Inter-Regular.ttf",
            "/usr/share/fonts/inter-font/Inter-Regular.ttf",
            "/usr/share/fonts/noto/NotoSans-Regular.ttf",
            "/usr/share/fonts/TTF/DejaVuSans.ttf",
        };

        const char* font_path = nullptr;
        for (const char* candidate : candidates) {
            if (std::filesystem::exists(candidate)) {
                font_path = candidate;
                break;
            }
        }

        if (font_path) {
            ImFontConfig body_config;
            body_config.OversampleH = 2;
            body_config.OversampleV = 2;
            body_font = io.Fonts->AddFontFromFileTTF(font_path, 20.0f * scale, &body_config);
            ImFontConfig display_config = body_config;
            display_font = io.Fonts->AddFontFromFileTTF(font_path, 42.0f * scale, &display_config);
        } else {
            ImFontConfig body_config;
            body_config.SizePixels = 20.0f * scale;
            body_font = io.Fonts->AddFontDefault(&body_config);
            ImFontConfig display_config;
            display_config.SizePixels = 42.0f * scale;
            display_font = io.Fonts->AddFontDefault(&display_config);
        }
        if (body_font) io.FontDefault = body_font;
    }

    void UI::on_focus_changed(GLFWwindow* window, int focused) {
        if (auto* self = static_cast<UI*>(glfwGetWindowUserPointer(window))) {
            self->window_focused = focused == GLFW_TRUE;
        }
    }

    void UI::apply_custom_style(float scale) {
        ImGuiStyle& style = ImGui::GetStyle();

        // Massive increases to padding for that "breathing room" UI feel
        style.WindowPadding     = ImVec2(28.0f * scale, 24.0f * scale);
        style.FramePadding      = ImVec2(13.0f * scale, 10.0f * scale);
        style.ItemSpacing       = ImVec2(10.0f * scale, 10.0f * scale);
        style.ItemInnerSpacing  = ImVec2(8.0f * scale, 6.0f * scale);
        
        // Heavy rounding for pill-shapes and soft edges
        style.WindowRounding    = 0.0f;
        style.ChildRounding     = 10.0f * scale;
        style.FrameRounding     = 8.0f * scale;
        style.PopupRounding     = 8.0f * scale;
        style.ScrollbarRounding = 8.0f * scale;
        style.GrabRounding      = 6.0f * scale;
        
        style.WindowBorderSize  = 0.0f;
        style.ChildBorderSize   = 0.0f;
        style.FrameBorderSize   = 0.0f;

        // Warm paper, soft graphite, and a quiet periwinkle accent.
        ImVec4 base_bg        = ImVec4(0.965f, 0.957f, 0.929f, 1.00f);
        ImVec4 elevated_bg    = ImVec4(1.000f, 0.997f, 0.980f, 1.00f);
        ImVec4 active_bg      = ImVec4(0.906f, 0.914f, 0.965f, 1.00f);
        ImVec4 primary_text   = ImVec4(0.165f, 0.169f, 0.196f, 1.00f);
        ImVec4 muted_text     = ImVec4(0.49f, 0.49f, 0.51f, 1.00f);
        ImVec4 accent_color   = ImVec4(0.36f, 0.40f, 0.76f, 1.00f);
        ImVec4 accent_hover   = ImVec4(0.43f, 0.47f, 0.82f, 1.00f);

        style.Colors[ImGuiCol_WindowBg]             = base_bg;
        style.Colors[ImGuiCol_ChildBg]              = ImVec4(0.935f, 0.925f, 0.895f, 1.0f);
        style.Colors[ImGuiCol_PopupBg]              = elevated_bg;
        style.Colors[ImGuiCol_Border]               = ImVec4(0.87f, 0.86f, 0.82f, 1.0f);
        style.Colors[ImGuiCol_Separator]            = ImVec4(0.87f, 0.86f, 0.82f, 1.0f);

        style.Colors[ImGuiCol_Text]                 = primary_text;
        style.Colors[ImGuiCol_TextDisabled]         = muted_text;

        style.Colors[ImGuiCol_FrameBg]              = elevated_bg;
        style.Colors[ImGuiCol_FrameBgHovered]       = active_bg;
        style.Colors[ImGuiCol_FrameBgActive]        = active_bg;

        style.Colors[ImGuiCol_Button]               = elevated_bg;
        style.Colors[ImGuiCol_ButtonHovered]        = active_bg;
        style.Colors[ImGuiCol_ButtonActive]         = accent_color;

        style.Colors[ImGuiCol_Header]               = accent_color; // For selectable items
        style.Colors[ImGuiCol_HeaderHovered]        = active_bg;
        style.Colors[ImGuiCol_HeaderActive]         = accent_color;

        style.Colors[ImGuiCol_PlotHistogram]        = accent_color;
        style.Colors[ImGuiCol_PlotHistogramHovered] = accent_hover;
        
        style.Colors[ImGuiCol_ScrollbarBg]          = ImVec4(0.0f, 0.0f, 0.0f, 0.00f);
        style.Colors[ImGuiCol_ScrollbarGrab]        = ImVec4(0.79f, 0.78f, 0.75f, 1.0f);
        style.Colors[ImGuiCol_ScrollbarGrabHovered] = active_bg;
        style.Colors[ImGuiCol_ScrollbarGrabActive]  = muted_text;
    }

    void UI::run() {

        while (!glfwWindowShouldClose(window) && !g_shutdown_requested.load()) {
            glfwPollEvents();
            if (!window_focused && has_presented_frame) {
                // Avoid swapping buffers for an occluded Wayland surface: some
                // compositors defer its frame callback until it becomes visible.
                glfwWaitEventsTimeout(0.25);
                continue;
            }
            if (!search_in_progress.load()) glfwWaitEventsTimeout(0.016);
            if (!window_focused && has_presented_frame) continue;

            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            render_frame();

            ImGui::Render();

            int display_w, display_h;
            glfwGetFramebufferSize(window, &display_w, &display_h);
            glViewport(0, 0, display_w, display_h);

            glClearColor(0.965f, 0.957f, 0.929f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            glfwSwapBuffers(window);
            has_presented_frame = true;
        }
    }

    void UI::render_frame() {
        const ImVec2 display = ImGui::GetIO().DisplaySize;
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(display);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 8.0f));
        ImGui::Begin("HaloViewport", nullptr,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus);

        constexpr float sidebar_width = 264.0f;
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.935f, 0.925f, 0.895f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20.0f, 24.0f));
        ImGui::BeginChild("Sidebar", ImVec2(sidebar_width, 0),
            ImGuiChildFlags_AlwaysUseWindowPadding);
        draw_header_region();
        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();

        ImGui::SameLine(0, 0);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.965f, 0.957f, 0.929f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(40.0f, 30.0f));
        ImGui::BeginChild("Workspace", ImVec2(0, 0),
            ImGuiChildFlags_AlwaysUseWindowPadding);
        const float workspace_width = ImGui::GetContentRegionAvail().x;
        const float content_width = std::min(workspace_width, 1024.0f);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (workspace_width - content_width) * 0.5f);
        ImGui::BeginChild("SearchContent", ImVec2(content_width, 0), ImGuiChildFlags_None);
        draw_search_region();
        ImGui::EndChild();
        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
        ImGui::End();
        ImGui::PopStyleVar(2);
    }

    void UI::draw_header_region() {
        const bool indexing = app.is_indexing();
        const ImVec2 p = ImGui::GetCursorScreenPos();
        ImDrawList* draw = ImGui::GetWindowDrawList();
        draw->AddRectFilled(p, ImVec2(p.x + 32.0f * ui_scale, p.y + 32.0f * ui_scale),
            IM_COL32(83, 91, 176, 255), 9.0f * ui_scale);
        draw->AddCircle(ImVec2(p.x + 16.0f * ui_scale, p.y + 16.0f * ui_scale),
            8.0f * ui_scale, IM_COL32(255, 255, 255, 235), 0, 2.0f * ui_scale);
        draw->AddCircleFilled(ImVec2(p.x + 19.0f * ui_scale, p.y + 13.0f * ui_scale),
            2.0f * ui_scale, IM_COL32(83, 91, 176, 255));
        ImGui::Dummy(ImVec2(32.0f, 32.0f));
        ImGui::SameLine(0, 10.0f);
        ImGui::Text("halo");
        ImGui::TextDisabled("LOCAL SEARCH");
        ImGui::Dummy(ImVec2(0, 24.0f));
        ImGui::TextDisabled("SEARCH IN");
        ImGui::Dummy(ImVec2(0, 5.0f));
        draw_mode_switch();
        ImGui::Dummy(ImVec2(0, 20.0f));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0, 14.0f));
        ImGui::TextDisabled("WORKSPACE");
        ImGui::Dummy(ImVec2(0, 5.0f));
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextUnformatted(app.get_target_path().c_str());
        ImGui::PopTextWrapPos();
        ImGui::Dummy(ImVec2(0, 10.0f));
        if (indexing) {
            const long long total = app.get_pipeline().get_files_total();
            const long long done = app.get_pipeline().get_files_indexed();
            const float fraction = total > 0 ? static_cast<float>(done) / static_cast<float>(total) : 0.0f;
            ImGui::ProgressBar(fraction, ImVec2(-1.0f, 7.0f));
            ImGui::TextDisabled("Indexing %lld / %lld", done, total);
        } else if (ImGui::Button("Refresh index", ImVec2(-1.0f, 36.0f))) {
            app.build_search_index();
        }
        ImGui::Dummy(ImVec2(0, 14.0f));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0, 8.0f));
        ImGui::TextColored(ImVec4(0.28f, 0.58f, 0.39f, 1.0f), "●");
        ImGui::SameLine(0, 6.0f * ui_scale);
        ImGui::TextDisabled("PRIVATE BY DESIGN");
        ImGui::TextDisabled("Your index stays on this device.");
    }

    void UI::draw_mode_switch() {
        const ImVec4 selected(0.88f, 0.89f, 0.95f, 1.0f);
        const ImVec4 selected_text(0.29f, 0.33f, 0.65f, 1.0f);
        for (int mode = 0; mode < 2; ++mode) {
            const bool active = mode_toggle == mode;
            if (active) {
                ImGui::PushStyleColor(ImGuiCol_Button, selected);
                ImGui::PushStyleColor(ImGuiCol_Text, selected_text);
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            }
            const char* label = mode == 0 ? "Code" : "File names";
            if (ImGui::Button(label, ImVec2(-1.0f, 40.0f * ui_scale))) mode_toggle = mode;
            ImGui::PopStyleColor(active ? 2 : 1);
        }
    }

    void UI::draw_search_region() {
        const bool indexing = app.is_indexing();
        ImGui::TextDisabled("HALO  /  WORKSPACE SEARCH");
        ImGui::SameLine(ImGui::GetWindowWidth() - 96.0f * ui_scale);
#if defined(__APPLE__)
        ImGui::TextDisabled("⌘ K  SEARCH");
#else
        ImGui::TextDisabled("CTRL K  SEARCH");
#endif
        ImGui::Dummy(ImVec2(0, 22.0f * ui_scale));
        if (display_font) ImGui::PushFont(display_font);
        ImGui::TextUnformatted("Find what you need.");
        if (display_font) ImGui::PopFont();
        ImGui::TextDisabled(mode_toggle == 0
            ? "Search across the words and symbols in your source files."
            : "Find a file by the name you remember.");
        ImGui::Dummy(ImVec2(0, 25.0f * ui_scale));

        if (indexing) ImGui::BeginDisabled();
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(16.0f * ui_scale, 15.0f * ui_scale));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(1.0f, 0.997f, 0.980f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.87f, 0.86f, 0.82f, 1.0f));
        ImGui::SetNextItemWidth(-1.0f);
        if ((ImGui::GetIO().KeyCtrl || ImGui::GetIO().KeySuper) && ImGui::IsKeyPressed(ImGuiKey_K))
            ImGui::SetKeyboardFocusHere();
        ImGui::InputTextWithHint("##SearchBox",
            mode_toggle == 0 ? "Search code, symbols, and phrases..." : "Search file names...",
            search_buffer, sizeof(search_buffer));
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
        if (indexing) ImGui::EndDisabled();
        ImGui::Dummy(ImVec2(0, 6.0f * ui_scale));
        ImGui::TextDisabled("SEARCHING IN  %s", app.get_target_path().c_str());

        const std::string query(search_buffer);
        if (query != last_search_query || mode_toggle != last_search_mode) {
            last_search_query = query;
            last_search_mode = mode_toggle;
            ++search_generation;
            content_results.clear();
            filename_results.clear();
            pending_search = query.size() >= 2;
            search_in_progress.store(pending_search);
        }

        if (search_future.valid() &&
            search_future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            try {
                SearchResponse response = search_future.get();
                if (response.generation == search_generation) {
                    if (response.mode == 0) {
                        content_results = std::get<std::vector<MatchResult>>(std::move(response.results));
                    } else {
                        filename_results = std::get<std::vector<FileMatch>>(std::move(response.results));
                    }
                    search_in_progress.store(false);
                }
            } catch (const std::exception& error) {
                LOG_ERROR(std::string("Search failed: ") + error.what());
                if (active_search_generation == search_generation) {
                    search_in_progress.store(false);
                }
            }
        }

        if (!search_future.valid() && pending_search) {
            const std::uint64_t generation = search_generation;
            const int mode = mode_toggle;
            const std::string requested_query = last_search_query;
            active_search_generation = generation;
            pending_search = false;
            try {
                search_future = std::async(std::launch::async, [this, generation, mode, requested_query]() {
                    if (mode == 0) {
                        return SearchResponse{generation, mode,
                            app.get_query_engine().search_terms(requested_query)};
                    }
                    return SearchResponse{generation, mode,
                        app.get_query_engine().search_filename(requested_query)};
                });
            } catch (const std::exception& error) {
                LOG_ERROR(std::string("Couldn't start search: ") + error.what());
                search_in_progress.store(false);
            }
        }

        ImGui::Dummy(ImVec2(0, 18.0f * ui_scale));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(1.0f, 0.997f, 0.980f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18.0f, 14.0f));
        ImGui::BeginChild("ResultsPanel", ImVec2(0, 0),
            ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding);
        const size_t count = mode_toggle == 0 ? content_results.size() : filename_results.size();
        ImGui::TextDisabled("%s", search_in_progress.load() ? "SEARCHING..." : "RESULTS");
        ImGui::SameLine(ImGui::GetWindowWidth() - 115.0f * ui_scale);
        if (!search_in_progress.load()) ImGui::TextDisabled("%zu FOUND", count);
        ImGui::Separator();

        if (indexing) {
            const long long total = app.get_pipeline().get_files_total();
            const long long done = app.get_pipeline().get_files_indexed();
            const float progress = total > 0 ? std::clamp(static_cast<float>(done) / static_cast<float>(total), 0.0f, 1.0f) : 0.0f;
            ImGui::TextWrapped("Building your local index. Search will be ready in a moment.");
            ImGui::ProgressBar(progress, ImVec2(-1.0f, 8.0f * ui_scale));
            ImGui::TextDisabled("%lld of %lld changed files indexed", done, total);
        } else if (last_search_query.size() < 2) {
            ImGui::Dummy(ImVec2(0, 24.0f * ui_scale));
            ImGui::TextColored(ImVec4(0.36f, 0.40f, 0.76f, 1.0f), "Your workspace, at your fingertips.");
            ImGui::TextDisabled("Content search matches every term on one line. File search looks at names.");
            ImGui::TextDisabled("Double-click a result to open it in your editor.");
        } else if (search_in_progress.load()) {
            ImGui::TextDisabled("Searching locally for \"%s\"...", last_search_query.c_str());
        } else if (count == 0) {
            ImGui::Dummy(ImVec2(0, 24.0f * ui_scale));
            ImGui::TextColored(ImVec4(0.36f, 0.40f, 0.76f, 1.0f), "No matches for this search.");
            ImGui::TextDisabled("Try fewer terms, a shorter filename, or sync the index.");
        } else {
            for (size_t i = 0; i < count; ++i) {
                const std::string& path = mode_toggle == 0 ? content_results[i].file_path : filename_results[i].file_path;
                const std::string title = mode_toggle == 0
                    ? std::filesystem::path(path).filename().string() : filename_results[i].file_name;
                const int line = mode_toggle == 0 ? content_results[i].line_number : 1;
                const std::string detail = mode_toggle == 0
                    ? "Line " + std::to_string(line) + "   ·   " + path : path;
                ImGui::PushID(static_cast<int>(i));
                const ImVec2 pos = ImGui::GetCursorScreenPos();
                const float height = 58.0f * ui_scale;
                ImGui::InvisibleButton("##result", ImVec2(-1.0f, height));
                const bool hovered = ImGui::IsItemHovered();
                const bool open = hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
                ImDrawList* draw = ImGui::GetWindowDrawList();
                if (hovered) draw->AddRectFilled(pos,
                    ImVec2(pos.x + ImGui::GetWindowWidth(), pos.y + height),
                    IM_COL32(246, 246, 251, 255), 5.0f * ui_scale);
                const float left = pos.x + 12.0f * ui_scale;
                draw->AddText(ImVec2(left, pos.y + 7.0f * ui_scale), IM_COL32(48, 51, 67, 255), title.c_str());
                draw->AddText(ImVec2(left, pos.y + 31.0f * ui_scale), IM_COL32(126, 127, 135, 255), detail.c_str());
                draw->AddLine(ImVec2(left, pos.y + height),
                    ImVec2(pos.x + ImGui::GetWindowWidth() - 12.0f * ui_scale, pos.y + height),
                    IM_COL32(235, 233, 227, 255));
                if (open) open_in_editor(path, line);
                ImGui::PopID();
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();

    }
    

    void UI::open_in_editor(const std::string& file_path, int line_number) {
        const std::string target = file_path + ":" + std::to_string(line_number);
        char* editor_arguments[] = {
            const_cast<char*>("code"),
            const_cast<char*>("--goto"),
            const_cast<char*>(target.c_str()),
            nullptr
        };

        pid_t child = -1;
        int spawn_error = posix_spawnp(&child, "code", nullptr, nullptr, editor_arguments, environ);
        if (spawn_error != 0) {
            char* opener_arguments[] = {
                const_cast<char*>("xdg-open"),
                const_cast<char*>(file_path.c_str()),
                nullptr
            };
            spawn_error = posix_spawnp(&child, "xdg-open", nullptr, nullptr, opener_arguments, environ);
        }

        if (spawn_error != 0) {
            LOG_ERROR(std::string("Couldn't launch an editor: ") + std::strerror(spawn_error));
            return;
        }

        std::thread([child] {
            int status = 0;
            while (waitpid(child, &status, 0) == -1 && errno == EINTR) {}
        }).detach();
    }
}
