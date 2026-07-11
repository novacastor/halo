#include "ui/ui.hpp"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <GLFW/glfw3.h>
#include <filesystem>
#include <cstdlib>
#include <unistd.h>
#include <sys/types.h>

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
        glfwSetWindowContentScaleCallback(window, &UI::on_content_scale_changed);

        LOG_INFO("GLFW window context created successfully. ");
        // Wayland delivers the surface's content-scale asynchronously after the
        // window is mapped, so querying it immediately after creation can still
        // return a stale 1.0 on some compositors. Pumping events a couple of times
        // flushes that initial scale event before we bake the font atlas at the
        // wrong pixel size.
        glfwPollEvents();
        glfwPollEvents();

        float xscale = 1.0f, yscale = 1.0f;
        glfwGetWindowContentScale(window, &xscale, &yscale);
        ui_scale = (xscale > 0.0f) ? xscale : 1.0f;

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui::StyleColorsLight();

        apply_custom_style(ui_scale);
        load_font(ui_scale);

        // Initialize backends — ImGui_ImplOpenGL3_Init() builds the font atlas
        // texture from whatever's in io.Fonts at this point.
        ImGui_ImplGlfw_InitForOpenGL(window, true);
        ImGui_ImplOpenGL3_Init("#version 330");

        app.build_search_index();

        return true;
    }

   void UI::load_font(float scale) {
        ImGuiIO& io = ImGui::GetIO();
        ImFontConfig font_cfg;
        font_cfg.FontDataOwnedByAtlas = true;
        font_cfg.OversampleH = 4; // Maximize crispness
        font_cfg.OversampleV = 4;
        font_cfg.PixelSnapH = true;

        // Increased from 15.0f to 18.0f for that premium, readable macOS feel
        const float base_pt = 18.0f;             
        const float px_size = base_pt * scale;   

        static const char* candidates[] = {
            "/usr/share/fonts/inter/Inter-Regular.ttf",
            "/usr/share/fonts/TTF/Inter-Regular.ttf",
            "/usr/share/fonts/inter-font/Inter-Regular.ttf",
            "/usr/share/fonts/noto/NotoSans-Regular.ttf",
            "/usr/share/fonts/TTF/NotoSans-Regular.ttf",
            "/usr/share/fonts/TTF/DejaVuSans.ttf",
        };

        ImFont* loaded = nullptr;
        for (const char* path : candidates) {
            if (std::filesystem::exists(path)) {
                loaded = io.Fonts->AddFontFromFileTTF(path, px_size, &font_cfg);
                if (loaded) break;
            }
        }
        if (!loaded) io.Fonts->AddFontDefault();
    }

    void UI::rebuild_fonts() {
        ImGuiIO& io = ImGui::GetIO();

        float xscale = 1.0f, yscale = 1.0f;
        glfwGetWindowContentScale(window, &xscale, &yscale);
        ui_scale = (xscale > 0.0f) ? xscale : 1.0f;

        io.Fonts->Clear();
        load_font(ui_scale);
        io.Fonts->Build();

        // Newer Dear ImGui (1.92+) tracks font textures itself via ImTextureData —
        // the OpenGL3 backend notices the atlas changed and re-uploads it on the
        // next frame automatically, so no manual create/destroy texture call is
        // needed (those functions no longer exist in the backend's public API).

        // apply_custom_style() writes absolute values (base * scale) rather than
        // an incremental ScaleAllSizes(), so calling it again here is safe and
        // won't compound if the scale changes more than once.
        apply_custom_style(ui_scale);
    }

    void UI::on_content_scale_changed(GLFWwindow* window, float /*xscale*/, float /*yscale*/) {
        if (auto* self = static_cast<UI*>(glfwGetWindowUserPointer(window))) {
            self->fonts_dirty = true;
        }
    }

    void UI::apply_custom_style(float scale) {
        ImGuiStyle& style = ImGui::GetStyle();

        // Massive increases to padding for that "breathing room" UI feel
        style.WindowPadding     = ImVec2(32.0f * scale, 32.0f * scale);
        style.FramePadding      = ImVec2(20.0f * scale, 14.0f * scale);
        style.ItemSpacing       = ImVec2(16.0f * scale, 16.0f * scale);
        style.ItemInnerSpacing  = ImVec2(12.0f * scale, 8.0f * scale);
        
        // Heavy rounding for pill-shapes and soft edges
        style.WindowRounding    = 16.0f * scale;
        style.ChildRounding     = 12.0f * scale;
        style.FrameRounding     = 12.0f * scale; // Pill-shaped search bar
        style.PopupRounding     = 12.0f * scale;
        style.ScrollbarRounding = 12.0f * scale;
        style.GrabRounding      = 8.0f * scale;
        
        style.WindowBorderSize  = 0.0f;
        style.ChildBorderSize   = 0.0f; // Remove ugly borders
        style.FrameBorderSize   = 0.0f;

        // --- Midnight Mac Color Palette ---
        ImVec4 base_bg        = ImVec4(0.98f, 0.98f, 0.98f, 1.00f); // Soft off-white base
        ImVec4 elevated_bg    = ImVec4(1.00f, 1.00f, 1.00f, 1.00f); // Pure white for inputs/cards
        ImVec4 active_bg      = ImVec4(0.93f, 0.93f, 0.93f, 1.00f); // Slight grey for hovers/tracks
        ImVec4 primary_text   = ImVec4(0.12f, 0.12f, 0.12f, 1.00f); // Crisp near-black
        ImVec4 muted_text     = ImVec4(0.55f, 0.55f, 0.55f, 1.00f); // Medium grey
        ImVec4 accent_color   = ImVec4(0.15f, 0.15f, 0.15f, 1.00f); // Monochrome dark charcoal accent
        ImVec4 accent_hover   = ImVec4(0.30f, 0.30f, 0.30f, 1.00f); // Lighter charcoal

        style.Colors[ImGuiCol_WindowBg]             = base_bg;
        style.Colors[ImGuiCol_ChildBg]              = base_bg;
        style.Colors[ImGuiCol_PopupBg]              = elevated_bg;
        style.Colors[ImGuiCol_Border]               = elevated_bg;
        style.Colors[ImGuiCol_Separator]            = elevated_bg;

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
        style.Colors[ImGuiCol_ScrollbarGrab]        = elevated_bg;
        style.Colors[ImGuiCol_ScrollbarGrabHovered] = active_bg;
        style.Colors[ImGuiCol_ScrollbarGrabActive]  = muted_text;
    }

    void UI::run() {

        while (!glfwWindowShouldClose(window) && !g_shutdown_requested.load()) {

            if (search_in_progress.load()) {
                glfwPollEvents();
            } else {
                glfwWaitEventsTimeout(0.016); 
            }

            if (fonts_dirty) {
                rebuild_fonts();
                fonts_dirty = false;
            }

            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            render_frame();

            ImGui::Render();

            int display_w, display_h;
            glfwGetFramebufferSize(window, &display_w, &display_h);
            glViewport(0, 0, display_w, display_h);

            glClearColor(0.98f, 0.98f, 0.98f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            glfwSwapBuffers(window);
        }
    }

    void UI::render_frame() {
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(static_cast<float>(display_w), static_cast<float>(display_h)));

        ImGui::Begin("HaloViewport", nullptr,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

        draw_header_region();
        ImGui::Spacing();

        ImVec2 p = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddLine(
            ImVec2(0, p.y), 
            ImVec2(display_w, p.y), 
            ImGui::ColorConvertFloat4ToU32(ImVec4(0.88f, 0.88f, 0.88f, 1.0f))
        );
        ImGui::Dummy(ImVec2(0, 16.0f * ui_scale));

        ImGui::Spacing();
        draw_search_region();

        ImGui::End();
    }

    void UI::draw_header_region() {
        const bool indexing = app.is_indexing();

        ImVec4 dot_color = indexing ? ImVec4(0.80f, 0.62f, 0.30f, 1.0f)
                                     : ImVec4(0.42f, 0.62f, 0.44f, 1.0f);
        float dot_r = 4.0f * ui_scale;
        ImVec2 p = ImGui::GetCursorScreenPos();
        float line_h = ImGui::GetTextLineHeight();
        ImGui::GetWindowDrawList()->AddCircleFilled(
            ImVec2(p.x + dot_r, p.y + line_h * 0.5f), dot_r,
            ImGui::ColorConvertFloat4ToU32(dot_color));
        ImGui::Dummy(ImVec2(dot_r * 2.0f + 8.0f * ui_scale, line_h));
        ImGui::SameLine(0.0f, 0.0f);

        ImGui::TextDisabled("%s  -  %s", app.get_target_path().c_str(), indexing ? "Indexing..." : "Ready");

        ImGui::SameLine(ImGui::GetWindowWidth() - 120.0f * ui_scale);
        if (indexing) {
            long long done  = app.get_pipeline().get_files_indexed();
            long long total = app.get_pipeline().get_files_total();
            float frac = total > 0 ? (float)done / (float)total : 0.0f;

            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.36f, 0.42f, 0.49f, 0.55f));
            ImGui::ProgressBar(frac, ImVec2(-1.0f, 3.0f * ui_scale), "");
            ImGui::PopStyleColor();
            ImGui::TextDisabled("%lld / %lld Files Indexed", done, total);
        } else {
            if (ImGui::Button("Sync Index", ImVec2(100.0f * ui_scale, 0))) {
                app.build_search_index();
            }
        }

        ImGui::Spacing();
        if (indexing) {
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.36f, 0.42f, 0.49f, 0.55f));
            ImGui::ProgressBar(-0.5f * (float)ImGui::GetTime(), ImVec2(-1.0f, 3.0f * ui_scale), "");
            ImGui::PopStyleColor();
        } else {
            ImGui::TextDisabled("Read %.3fs  \xc2\xb7  Tokenize %.3fs  \xc2\xb7  Database %.3fs",
                               app.get_pipeline().get_read_time_s(),
                               app.get_pipeline().get_tokenize_time_s(),
                               app.get_pipeline().get_db_time_s());
        }
    }

    void UI::draw_mode_switch() {
        // Animation State Tracker
        static float anim_t = (mode_toggle == 0) ? 0.0f : 1.0f;
        float target = (mode_toggle == 0) ? 0.0f : 1.0f;
        
        // Smooth linear interpolation (lerp) for the sliding animation
        anim_t = anim_t + (target - anim_t) * 0.20f; 

        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImVec2 p = ImGui::GetCursorScreenPos();
        
        float width = 240.0f * ui_scale;
        float height = ImGui::GetFrameHeight();
        float radius = height * 0.5f; // Perfect pill shape
        
        // Draw the background track
        draw_list->AddRectFilled(p, ImVec2(p.x + width, p.y + height), 
                                 ImGui::ColorConvertFloat4ToU32(ImVec4(0.93f, 0.93f, 0.93f, 1.0f)), radius);

        // Draw the sliding active pill
        float pill_width = width * 0.5f;
        float pill_x_offset = anim_t * pill_width;
        
        // Slight inner shadow/border effect on the pill
        ImVec2 pill_min(p.x + pill_x_offset + 2.0f, p.y + 2.0f);
        ImVec2 pill_max(p.x + pill_x_offset + pill_width - 2.0f, p.y + height - 2.0f);
        draw_list->AddRectFilled(pill_min, pill_max, ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 1.0f, 1.0f, 1.0f)), radius - 2.0f);
        
        // Invisible buttons to handle clicking
        ImGui::SetCursorScreenPos(p);
        if (ImGui::InvisibleButton("##btn_content", ImVec2(pill_width, height))) mode_toggle = 0;
        ImGui::SetCursorScreenPos(ImVec2(p.x + pill_width, p.y));
        if (ImGui::InvisibleButton("##btn_filename", ImVec2(pill_width, height))) mode_toggle = 1;

        // Draw Text over everything
       auto draw_label = [&](const char* text, float offset_x, bool active) {
            ImVec2 text_size = ImGui::CalcTextSize(text);
            ImVec2 text_pos = ImVec2(p.x + offset_x + (pill_width - text_size.x) * 0.5f, p.y + (height - text_size.y) * 0.5f);
            ImU32 text_col = ImGui::ColorConvertFloat4ToU32(active ? ImVec4(0.12f, 0.12f, 0.12f, 1.0f) : ImVec4(0.60f, 0.60f, 0.60f, 1.0f));
            draw_list->AddText(text_pos, text_col, text);
        };

        draw_label("Content", 0.0f, mode_toggle == 0);
        draw_label("Filenames", pill_width, mode_toggle == 1);
        
        // Advance layout cursor past the custom drawn element
        ImGui::SetCursorScreenPos(ImVec2(p.x + width + (16.0f * ui_scale), p.y));
    }

    void UI::draw_search_region() {
        const bool indexing = app.is_indexing();
        if (indexing) ImGui::BeginDisabled();

        draw_mode_switch();
        ImGui::SameLine();

        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        bool changed = ImGui::InputTextWithHint(
            "##SearchBox", "Search tokens or filenames...", search_buffer, sizeof(search_buffer));

        if (changed) {
            std::string query(search_buffer);
            if (query.length() >= 2) {
                search_in_progress = true;
                if (mode_toggle == 0) {
                    future_content_results = std::async(std::launch::async, [this, query]() {
                        return app.get_query_engine().search_phrase(query);
                    });
                } else {
                    future_filename_results = std::async(std::launch::async, [this, query]() {
                        return app.get_query_engine().search_filename(query);
                    });
                }
            } else {
                content_results.clear();
                filename_results.clear();
                search_in_progress = false;
            }
        }

        if(search_in_progress) {
            if(mode_toggle == 0 && future_content_results.valid()) {
                if (future_content_results.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                    content_results = future_content_results.get();
                    search_in_progress = false;
                }
            }else if(mode_toggle == 1 && future_filename_results.valid()) {
                if (future_filename_results.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                    filename_results = future_filename_results.get();
                    search_in_progress = false;
                }
            }
            ImGui::TextDisabled("Searching...");
            ImGui::Separator();
        }
        ImGui::Spacing();
        ImGui::Spacing();

        // Custom Spotlight-style result row drawer
        // Custom Spotlight-style result row drawer
        auto draw_spotlight_row = [&](const std::string& primary, const std::string& secondary, int id) -> bool {
            ImGui::PushID(id);
            
            ImVec2 pos = ImGui::GetCursorScreenPos();
            ImVec2 avail = ImGui::GetContentRegionAvail();
            float row_height = 48.0f * ui_scale; 
            
            // Invisible button to capture clicks and hover states
            ImGui::InvisibleButton("##row", ImVec2(avail.x, row_height));
            bool hovered = ImGui::IsItemHovered();
            
            // TRACK DOUBLE CLICK HERE
            bool double_clicked = hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
            
            // 1. Draw rounded hover background
            if (hovered) {
                ImGui::GetWindowDrawList()->AddRectFilled(
                    pos, ImVec2(pos.x + avail.x, pos.y + row_height),
                    ImGui::ColorConvertFloat4ToU32(ImVec4(0.92f, 0.94f, 0.96f, 1.0f)), 
                    8.0f * ui_scale
                );
            }
            
            // 2. Draw Primary Text (Filename / Content)
            ImVec2 text_pos = ImVec2(pos.x + 12.0f * ui_scale, pos.y + 6.0f * ui_scale);
            ImGui::GetWindowDrawList()->AddText(
                text_pos, 
                ImGui::ColorConvertFloat4ToU32(ImVec4(0.12f, 0.12f, 0.12f, 1.0f)), 
                primary.c_str()
            );
            
            // 3. Draw Secondary Text (Path / Line Num)
            text_pos.y += 20.0f * ui_scale; 
            ImGui::GetWindowDrawList()->AddText(
                text_pos, 
                ImGui::ColorConvertFloat4ToU32(ImVec4(0.55f, 0.55f, 0.58f, 1.0f)), 
                secondary.c_str()
            );
            
            // 4. Draw subtle bottom separator line
            ImGui::GetWindowDrawList()->AddLine(
                ImVec2(pos.x + 12.0f * ui_scale, pos.y + row_height), 
                ImVec2(pos.x + avail.x - 12.0f * ui_scale, pos.y + row_height),
                ImGui::ColorConvertFloat4ToU32(ImVec4(0.92f, 0.92f, 0.92f, 1.0f))
            );
            
            ImGui::PopID();
            return double_clicked; // RETURN THE DOUBLE CLICK STATE
        };

        if (ImGui::BeginChild("ScrollingResultsRegion", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
            
            // Animation state tracker for results fading in
            static float list_alpha = 0.0f;
            list_alpha = list_alpha + ((search_in_progress.load() ? 0.0f : 1.0f) - list_alpha) * 0.15f;
            
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, list_alpha);

            int row = 0;
            if (mode_toggle == 0) {
                for (const auto& match : content_results) {
                    std::string line_info = "Line " + std::to_string(match.line_number);
                    if (draw_spotlight_row(match.file_path, line_info, row)) {
                        open_in_editor(match.file_path, match.line_number);
                    }
                    ++row;
                }
            } else {
                for (const auto& match : filename_results) {
                    if (draw_spotlight_row(match.file_name, match.file_path, row)) {
                        open_in_editor(match.file_path, 1);
                    }
                    ++row;
                }
            }
            ImGui::PopStyleVar();
            ImGui::EndChild();
        }

        if (indexing) ImGui::EndDisabled();
    }
    

    void UI::open_in_editor(const std::string& file_path, int line_number) {
        pid_t pid = fork();
        
        if (pid == -1) {
            LOG_ERROR("Failed to fork process for editor launch");
            return;
        } 
        
        if (pid == 0) {
            std::string line_arg = std::to_string(line_number);
            
            execlp("xdg-open", "xdg-open", file_path.c_str(), nullptr);
            LOG_ERROR("exelp failed to launch xdg-open");
            exit(EXIT_FAILURE); 
        }
    }
}