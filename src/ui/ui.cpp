#include "ui/ui.hpp"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <GLFW/glfw3.h>
#include <filesystem>
#include <iostream>
#include <cstdlib>
#include <unistd.h>
#include <sys/types.h>

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
        // IMPORTANT: platform selection has to happen via glfwInitHint(), *before*
        // glfwInit() runs. The previous code called glfwWindowHint(GLFW_PLATFORM, ...)
        // after glfwInit() had already picked a backend — that call was a silent
        // no-op, so the app was very likely running under XWayland the whole time
        // despite the comment. XWayland fakes HiDPI scaling by rendering at 1x and
        // stretching, which is exactly what reads as jagged/pixelated text on a
        // scaled Hyprland output.
        //
        // glfwPlatformSupported() can be (and must be) called before glfwInit().
        // If the GLFW build doesn't have the Wayland backend compiled in at all
        // (common with vcpkg's default glfw3 port, which usually only enables
        // X11 unless you opt in), forcing GLFW_PLATFORM_WAYLAND makes glfwInit()
        // fail outright rather than silently falling back — so check first.
        if (glfwPlatformSupported(GLFW_PLATFORM_WAYLAND)) {
            glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_WAYLAND);
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
            std::cerr << "CRITICAL: Failed to initialize GLFW\n";
            return false;
        }

        glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
        if (!window) {
            std::cerr << "CRITICAL: Failed to create GLFW window context\n";
            glfwTerminate();
            return false;
        }
        glfwMakeContextCurrent(window);
        glfwSwapInterval(1);

        glfwSetWindowUserPointer(window, this);
        glfwSetWindowContentScaleCallback(window, &UI::on_content_scale_changed);

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
        font_cfg.OversampleH = 3;
        font_cfg.OversampleV = 3;
        font_cfg.PixelSnapH = true;
        // Deliberately no RasterizerMultiply here: pushing anti-aliased glyph
        // alpha past 1.0 to fake a bolder weight distorts the AA edge and is a
        // common cause of text looking crunchy/jagged at small sizes. Better to
        // just use a font whose regular weight already reads cleanly.

        const float base_pt = 15.0f;             // logical point size
        const float px_size = base_pt * scale;   // baked at the real display scale

        // Inter/Noto read much closer to the macOS system font than DejaVu Sans.
        // For the best result: `sudo pacman -S ttf-inter` (or `ttf-inter-git` / AUR).
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

        if (!loaded) {
            std::cerr << "WARNING: Couldn't find Inter, Noto Sans, or DejaVu Sans on disk. "
                          "Falling back to ImGui's built-in bitmap font — install one with, "
                          "e.g., `sudo pacman -S ttf-inter` for a much crisper result.\n";
            io.Fonts->AddFontDefault();
        }
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

        // Minimalist layout structural tuning — all absolute values derived from
        // the current display scale, so this can be called again on a DPI change
        // without drifting.
        style.WindowPadding     = ImVec2(18.0f * scale, 18.0f * scale);
        style.FramePadding      = ImVec2(12.0f * scale, 9.0f * scale);
        style.ItemSpacing       = ImVec2(10.0f * scale, 10.0f * scale);
        style.ItemInnerSpacing  = ImVec2(8.0f * scale, 6.0f * scale);
        style.ScrollbarSize     = 12.0f * scale;
        style.WindowRounding    = 14.0f * scale;
        style.ChildRounding     = 10.0f * scale;
        style.FrameRounding     = 9.0f * scale;
        style.PopupRounding     = 9.0f * scale;
        style.ScrollbarRounding = 12.0f * scale;
        style.GrabRounding      = 6.0f * scale;
        style.WindowBorderSize  = 0.0f;
        style.ChildBorderSize   = 1.0f;
        style.FrameBorderSize   = 1.0f;

        // --- Creamy Tactile macOS Color Palette ---
        ImVec4 container_white  = ImVec4(0.98f, 0.98f, 0.98f, 1.00f); // #FAF9F8 Clean Off-White
        ImVec4 divider_gray     = ImVec4(0.88f, 0.88f, 0.86f, 1.00f); // #E0E0DC Tactile Borders
        ImVec4 charcoal_text    = ImVec4(0.16f, 0.17f, 0.18f, 1.00f); // #292A2E High Contrast Soft Text
        ImVec4 gray_muted_text  = ImVec4(0.50f, 0.51f, 0.53f, 1.00f); // #808287 Subdued Metrics

        ImVec4 interactive_bg   = ImVec4(0.91f, 0.91f, 0.89f, 1.00f); // #E8E8E3 Light Neutral Component
        ImVec4 interact_hover   = ImVec4(0.85f, 0.85f, 0.83f, 1.00f); // #D9D9D4 Medium Tint
        ImVec4 interact_active  = ImVec4(0.77f, 0.78f, 0.75f, 1.00f); // #C4C7BF Darker Press State

        ImVec4 slate_accent     = ImVec4(0.36f, 0.42f, 0.49f, 1.00f); // #5C6B7D
        ImVec4 slate_hover      = ImVec4(0.43f, 0.50f, 0.58f, 1.00f); // #6E8094

        style.Colors[ImGuiCol_WindowBg]             = container_white;
        style.Colors[ImGuiCol_ChildBg]              = container_white;
        style.Colors[ImGuiCol_PopupBg]              = container_white;
        style.Colors[ImGuiCol_Border]               = divider_gray;
        style.Colors[ImGuiCol_Separator]            = divider_gray;

        style.Colors[ImGuiCol_Text]                 = charcoal_text;
        style.Colors[ImGuiCol_TextDisabled]         = gray_muted_text;

        style.Colors[ImGuiCol_FrameBg]              = interactive_bg;
        style.Colors[ImGuiCol_FrameBgHovered]       = interact_hover;
        style.Colors[ImGuiCol_FrameBgActive]        = interact_active;

        style.Colors[ImGuiCol_Button]               = interactive_bg;
        style.Colors[ImGuiCol_ButtonHovered]        = interact_hover;
        style.Colors[ImGuiCol_ButtonActive]         = interact_active;

        style.Colors[ImGuiCol_Header]               = interactive_bg;
        style.Colors[ImGuiCol_HeaderHovered]        = interact_hover;
        style.Colors[ImGuiCol_HeaderActive]         = interact_active;

        style.Colors[ImGuiCol_PlotHistogram]        = slate_accent;
        style.Colors[ImGuiCol_PlotHistogramHovered] = slate_hover;
        style.Colors[ImGuiCol_CheckMark]            = slate_accent;
        style.Colors[ImGuiCol_SliderGrab]           = slate_accent;
        style.Colors[ImGuiCol_SliderGrabActive]     = slate_hover;

        style.Colors[ImGuiCol_ScrollbarBg]          = ImVec4(0.0f, 0.0f, 0.0f, 0.00f);
        style.Colors[ImGuiCol_ScrollbarGrab]        = interact_hover;
        style.Colors[ImGuiCol_ScrollbarGrabHovered]  = interact_active;
        style.Colors[ImGuiCol_ScrollbarGrabActive]   = slate_accent;
    }

    void UI::run() {
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();

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

            glClearColor(0.95f, 0.95f, 0.94f, 1.0f);
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
        ImGui::Separator();
        ImGui::Spacing();
        draw_search_region();

        ImGui::End();
    }

    void UI::draw_header_region() {
        const bool indexing = app.is_indexing();

        // Small status dot, menu-bar style: amber while indexing, soft green when idle.
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
            ImGui::BeginDisabled();
            ImGui::Button("Syncing...", ImVec2(100.0f * ui_scale, 0));
            ImGui::EndDisabled();
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
        ImVec4 selected_bg   = ImVec4(0.98f, 0.98f, 0.98f, 1.0f);
        ImVec4 selected_text = ImVec4(0.16f, 0.17f, 0.18f, 1.0f);
        ImVec4 idle_bg       = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
        ImVec4 idle_hover    = ImVec4(0.0f, 0.0f, 0.0f, 0.05f);
        ImVec4 idle_active   = ImVec4(0.0f, 0.0f, 0.0f, 0.08f);
        ImVec4 idle_text     = ImVec4(0.50f, 0.51f, 0.53f, 1.0f);

        const float seg_w = 88.0f * ui_scale;

        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, ImGui::GetStyle().FrameRounding * 0.75f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.91f, 0.91f, 0.89f, 1.0f));
        ImGui::BeginChild("ModeSwitch", ImVec2(seg_w * 2.0f + 6.0f * ui_scale, ImGui::GetFrameHeight() + 6.0f * ui_scale),
                           ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar);
        ImGui::Dummy(ImVec2(0.0f, 2.0f * ui_scale));
        ImGui::SameLine(3.0f * ui_scale, 0.0f);

        for (int i = 0; i < 2; ++i) {
            const char* label = (i == 0) ? "Content" : "Filenames";
            const bool selected = (mode_toggle == i);

            ImGui::PushID(i);
            ImGui::PushStyleColor(ImGuiCol_Button, selected ? selected_bg : idle_bg);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, selected ? selected_bg : idle_hover);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, selected ? selected_bg : idle_active);
            ImGui::PushStyleColor(ImGuiCol_Text, selected ? selected_text : idle_text);

            if (ImGui::Button(label, ImVec2(seg_w, ImGui::GetFrameHeight()))) {
                if (mode_toggle != i) {
                    mode_toggle = i;
                    content_results.clear();
                    filename_results.clear();
                    search_buffer[0] = '\0';
                }
            }

            ImGui::PopStyleColor(4);
            ImGui::PopID();

            if (i == 0) ImGui::SameLine(0.0f, 2.0f * ui_scale);
        }

        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
    }

    void UI::draw_search_region() {
        const bool indexing = app.is_indexing();
        if (indexing) ImGui::BeginDisabled();

        draw_mode_switch();
        ImGui::SameLine();

        ImGui::PushItemWidth(-1.0f);
        bool changed = ImGui::InputTextWithHint(
            "##SearchBox", "Search tokens or filenames...", search_buffer, sizeof(search_buffer));
        ImGui::PopItemWidth();

        if (changed) {
            std::string query(search_buffer);
            if (query.length() >= 2) {
                if (mode_toggle == 0) {
                    content_results = app.get_query_engine().search_phrase(query);
                } else {
                    filename_results = app.get_query_engine().search_filename(query);
                }
            } else {
                content_results.clear();
                filename_results.clear();
            }
        }

        ImGui::Spacing();

        if (ImGui::BeginChild("ScrollingResultsRegion", ImVec2(0, 0), ImGuiChildFlags_Borders, ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
            ImU32 alt_row = ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.0f, 0.0f, 0.025f));
            int row = 0;

            auto draw_alt_row_bg = [&]() {
                if (row % 2 == 1) {
                    ImVec2 rmin = ImGui::GetCursorScreenPos();
                    ImVec2 rmax = ImVec2(rmin.x + ImGui::GetContentRegionAvail().x,
                                          rmin.y + ImGui::GetTextLineHeightWithSpacing());
                    ImGui::GetWindowDrawList()->AddRectFilled(rmin, rmax, alt_row);
                }
            };

            if (mode_toggle == 0) {
                for (const auto& match : content_results) {
                    draw_alt_row_bg();
                    std::string label = match.file_path + "  [Line " + std::to_string(match.line_number) + "]";
                    if (ImGui::Selectable(label.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick)) {
                        if (ImGui::IsMouseDoubleClicked(0)) {
                            open_in_editor(match.file_path, match.line_number);
                        }
                    }
                    ++row;
                }
            } else {
                for (const auto& match : filename_results) {
                    draw_alt_row_bg();
                    std::string label = match.file_path + " (" + match.file_name + ")";
                    if (ImGui::Selectable(label.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick)) {
                        if (ImGui::IsMouseDoubleClicked(0)) {
                            open_in_editor(match.file_path, 1);
                        }
                    }
                    ++row;
                }
            }
            ImGui::EndChild();
        }

        if (indexing) ImGui::EndDisabled();
    }

    void UI::open_in_editor(const std::string& file_path, int line_number) {
        pid_t pid = fork();
        
        if (pid == -1) {
            std::cerr << "Failed to fork process for editor launch.\n";
            return;
        } 
        
        if (pid == 0) {
            std::string line_arg = std::to_string(line_number);
            
            execlp("kate", "kate", "--line", line_arg.c_str(), file_path.c_str(), nullptr);
            std::cerr << "Critical: execlp failed to launch editor.\n";
            exit(EXIT_FAILURE); 
        }
    }
}