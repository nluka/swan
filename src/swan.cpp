#include <filesystem>

#pragma warning(push)
#pragma warning(disable: 4244)
#define STB_IMAGE_IMPLEMENTATION
#include "stbi_image.h"
#pragma warning(pop)

#define IMGUI_DEFINE_MATH_OPERATORS 1
#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"

#include "on_scope_exit.hpp"
#include "primitives.hpp"
#include "common.hpp"
#include "util.hpp"

#include "pinned_window.cpp"
#include "explorer_window.cpp"
#include "file_ops_window.cpp"
#include "debug_log_window.cpp"

#define GL_SILENCE_DEPRECATION
#include <glfw3.h> // Will drag system OpenGL headers

// https://github.com/ocornut/imgui/issues/707#issuecomment-917151020
void setup_imgui_styles()
{
    ImVec4 *colors = ImGui::GetStyle().Colors;
    colors[ImGuiCol_Text]                   = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    colors[ImGuiCol_TextDisabled]           = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    colors[ImGuiCol_WindowBg]               = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    colors[ImGuiCol_ChildBg]                = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_PopupBg]                = ImVec4(0.19f, 0.19f, 0.19f, 0.92f);
    colors[ImGuiCol_Border]                 = ImVec4(0.19f, 0.19f, 0.19f, 0.29f);
    colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.24f);
    colors[ImGuiCol_FrameBg]                = ImVec4(0.05f, 0.05f, 0.05f, 0.54f);
    colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.19f, 0.19f, 0.19f, 0.54f);
    colors[ImGuiCol_FrameBgActive]          = ImVec4(0.20f, 0.22f, 0.23f, 1.00f);
    colors[ImGuiCol_TitleBg]                = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    colors[ImGuiCol_TitleBgActive]          = ImVec4(0.06f, 0.06f, 0.06f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    colors[ImGuiCol_MenuBarBg]              = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.05f, 0.05f, 0.05f, 0.54f);
    colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.34f, 0.34f, 0.34f, 0.54f);
    colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.40f, 0.40f, 0.40f, 0.54f);
    colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.56f, 0.56f, 0.56f, 0.54f);
    colors[ImGuiCol_CheckMark]              = ImVec4(0.33f, 0.67f, 0.86f, 1.00f);
    colors[ImGuiCol_SliderGrab]             = ImVec4(0.34f, 0.34f, 0.34f, 0.54f);
    colors[ImGuiCol_SliderGrabActive]       = ImVec4(0.56f, 0.56f, 0.56f, 0.54f);
    colors[ImGuiCol_Button]                 = ImVec4(0.05f, 0.05f, 0.05f, 0.54f);
    colors[ImGuiCol_ButtonHovered]          = ImVec4(0.19f, 0.19f, 0.19f, 0.54f);
    colors[ImGuiCol_ButtonActive]           = ImVec4(0.20f, 0.22f, 0.23f, 1.00f);
    colors[ImGuiCol_Header]                 = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
    colors[ImGuiCol_HeaderHovered]          = ImVec4(0.00f, 0.00f, 0.00f, 0.36f);
    colors[ImGuiCol_HeaderActive]           = ImVec4(0.20f, 0.22f, 0.23f, 0.33f);
    colors[ImGuiCol_Separator]              = ImVec4(0.28f, 0.28f, 0.28f, 0.29f);
    colors[ImGuiCol_SeparatorHovered]       = ImVec4(0.44f, 0.44f, 0.44f, 0.29f);
    colors[ImGuiCol_SeparatorActive]        = ImVec4(0.40f, 0.44f, 0.47f, 1.00f);
    colors[ImGuiCol_ResizeGrip]             = ImVec4(0.28f, 0.28f, 0.28f, 0.29f);
    colors[ImGuiCol_ResizeGripHovered]      = ImVec4(0.44f, 0.44f, 0.44f, 0.29f);
    colors[ImGuiCol_ResizeGripActive]       = ImVec4(0.40f, 0.44f, 0.47f, 1.00f);
    colors[ImGuiCol_Tab]                    = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
    colors[ImGuiCol_TabHovered]             = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_TabActive]              = ImVec4(0.20f, 0.20f, 0.20f, 0.36f);
    colors[ImGuiCol_TabUnfocused]           = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
    colors[ImGuiCol_TabUnfocusedActive]     = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_DockingPreview]         = ImVec4(0.33f, 0.67f, 0.86f, 1.00f);
    colors[ImGuiCol_DockingEmptyBg]         = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotLines]              = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered]       = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotHistogram]          = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered]   = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
    colors[ImGuiCol_TableHeaderBg]          = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
    colors[ImGuiCol_TableBorderStrong]      = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
    colors[ImGuiCol_TableBorderLight]       = ImVec4(0.28f, 0.28f, 0.28f, 0.29f);
    colors[ImGuiCol_TableRowBg]             = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt]          = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);
    colors[ImGuiCol_TextSelectedBg]         = ImVec4(0.20f, 0.22f, 0.23f, 1.00f);
    colors[ImGuiCol_DragDropTarget]         = ImVec4(0.33f, 0.67f, 0.86f, 1.00f);
    colors[ImGuiCol_NavHighlight]           = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
    colors[ImGuiCol_NavWindowingHighlight]  = ImVec4(1.00f, 0.00f, 0.00f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg]      = ImVec4(1.00f, 0.00f, 0.00f, 0.20f);
    colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(1.00f, 0.00f, 0.00f, 0.35f);

    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowPadding                     = ImVec2(8.00f, 8.00f);
    style.FramePadding                      = ImVec2(5.00f, 2.00f);
    style.CellPadding                       = ImVec2(6.00f, 6.00f);
    style.ItemSpacing                       = ImVec2(6.00f, 6.00f);
    style.ItemInnerSpacing                  = ImVec2(6.00f, 6.00f);
    style.TouchExtraPadding                 = ImVec2(0.00f, 0.00f);
    style.IndentSpacing                     = 25;
    style.ScrollbarSize                     = 15;
    style.GrabMinSize                       = 10;
    style.WindowBorderSize                  = 1;
    style.ChildBorderSize                   = 1;
    style.PopupBorderSize                   = 1;
    style.FrameBorderSize                   = 1;
    style.TabBorderSize                     = 1;
    style.WindowRounding                    = 7;
    style.ChildRounding                     = 4;
    style.FrameRounding                     = 3;
    style.PopupRounding                     = 4;
    style.ScrollbarRounding                 = 9;
    style.GrabRounding                      = 3;
    style.LogSliderDeadzone                 = 4;
    style.TabRounding                       = 4;
}

static
void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

static
GLFWwindow *init_glfw_and_imgui()
{
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) {
        return nullptr;
    }

    // GL 3.0 + GLSL 130
    {
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
        //glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
        //glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only
    }

    GLFWwindow *window = nullptr;
    // Create window with graphics context
    {
        int screenWidth = GetSystemMetrics(SM_CXSCREEN);
        int screenHeight = GetSystemMetrics(SM_CYSCREEN);
        window = glfwCreateWindow(screenWidth, screenHeight, "swan", nullptr, nullptr);
        if (window == nullptr) {
            return nullptr;
        }
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // vsync
    glfwMaximizeWindow(window);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    io.ConfigDockingWithShift = true;
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::SetNextWindowSizeConstraints(io.DisplaySize, io.DisplaySize);
    ImGui::StyleColorsDark();

    // setup_imgui_styles();

    // Setup Platform/Renderer backends
    {
        char const *glsl_version = "#version 130";
        ImGui_ImplGlfw_InitForOpenGL(window, true);
        ImGui_ImplOpenGL3_Init(glsl_version);
    }

    {
        [[maybe_unused]] auto font = io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/consola.ttf", 15.0f);
        // IM_ASSERT(font != nullptr);

        io.Fonts->AddFontDefault();
    }

    return window;
}

static
void set_window_icon(GLFWwindow *window)
{
    GLFWimage icon;
    icon.pixels = nullptr;
    icon.width = 0;
    icon.height = 0;

    int icon_width, icon_height, icon_channels;
    u8 *icon_pixels = stbi_load("resource/swan.png", &icon_width, &icon_height, &icon_channels, STBI_rgb_alpha);

    auto cleanup_icon_pixels_routine = make_on_scope_exit([icon_pixels] {
        stbi_image_free(icon_pixels);
    });

    if (icon_pixels)
    {
        icon.pixels = icon_pixels;
        icon.width = icon_width;
        icon.height = icon_height;

        glfwSetWindowIcon(window, 1, &icon);
    }
}

static
bool init_windows_shell_com_garbage()
{
    // COM has to be one of the dumbest things I've ever seen...
    // what's wrong with just having some functions? Why on earth does this stuff need to be OO?

    // Initialize COM library
    HRESULT com_handle = CoInitialize(nullptr);
    if (FAILED(com_handle)) {
        debug_log("CoInitialize failed");
        return false;
    }

    // Create an instance of IShellLinkA
    com_handle = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkA, (LPVOID *)&s_shell_link);
    if (FAILED(com_handle)) {
        debug_log("CoCreateInstance failed");
        CoUninitialize();
        return false;
    }

    // Query IPersistFile interface from IShellLinkA
    com_handle = s_shell_link->QueryInterface(IID_IPersistFile, (LPVOID *)&s_persist_file_interface);
    if (FAILED(com_handle)) {
        debug_log("failed to query IPersistFile interface");
        s_persist_file_interface->Release();
        CoUninitialize();
        return false;
    }

    return true;
}

static
void cleanup_windows_shell_com_garbage()
{
    s_persist_file_interface->Release();
    s_shell_link->Release();
    CoUninitialize();
}

static
void render(GLFWwindow *window)
{
    ImGui::Render();

    int display_w, display_h;
    ImVec4 clear_color(0.45f, 0.55f, 0.60f, 1.00f);

    glfwGetFramebufferSize(window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(window);
}

// #pragma comment(linker, "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")
i32 main(i32, char**) try
{
    debug_log("initializing...");

    GLFWwindow *window = init_glfw_and_imgui();
    if (window == nullptr) {
        return 1;
    }

    if (!init_windows_shell_com_garbage()) {
        return 1;
    }

    auto cleanup_routine = make_on_scope_exit([window]() {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        glfwDestroyWindow(window);
        glfwTerminate();
        cleanup_windows_shell_com_garbage();
    });

    set_window_icon(window);

    [[maybe_unused]] auto &io = ImGui::GetIO();

    io.IniFilename = "data/swan_imgui.ini";

    explorer_options expl_opts = {};
    if (!expl_opts.load_from_disk()) {
        debug_log("explorer_options::load_from_disk failed, setting defaults");
        expl_opts.auto_refresh_interval_ms = 1000;
        expl_opts.adaptive_refresh_threshold = 1000;
        expl_opts.ref_mode = explorer_options::refresh_mode::adaptive;
        expl_opts.show_dotdot_dir = true;
    #if !defined(NDEBUG)
        expl_opts.show_debug_info = true;
        expl_opts.show_cwd_len = true;
    #endif
    }

    windows_options win_opts = {};
    if (!win_opts.load_from_disk()) {
        debug_log("windows_options::load_from_disk failed, setting defaults");
        win_opts.show_explorer_1 = true;
        win_opts.show_pinned = true;
    #if !defined(NDEBUG)
        win_opts.show_demo = true;
    #endif
    }

    {
        auto [success, num_pins_loaded] = load_pins_from_disk(expl_opts.dir_separator());
        if (!success) {
            debug_log("load_pins_from_disk failed");
        } else {
            debug_log("load_pins_from_disk success, loaded %zu pins", num_pins_loaded);
        }
    }

    std::array<explorer_window, 4> explorers = {};
    {
        char const *names[] = { "Explorer 1", "Explorer 2", "Explorer 3", "Explorer 4" };

        for (u64 i = 0; i < explorers.size(); ++i) {
            auto &expl = explorers[i];

            expl.name = names[i];
            expl.filter_error.reserve(1024);

            bool load_result = explorers[i].load_from_disk(expl_opts.dir_separator());
            debug_log("[Explorer %zu] load_from_disk: %d", i+1, load_result);

            if (!load_result) {
                std::string startup_path_stdstr = std::filesystem::current_path().string();

                path_t startup_path = {};
                path_append(startup_path, startup_path_stdstr.c_str());
                path_force_separator(startup_path, expl_opts.dir_separator());

                expl.cwd = startup_path;
                expl.cwd_last_frame = startup_path;
                expl.wd_history.push_back(startup_path);

                bool save_result = explorers[i].save_to_disk();
                debug_log("[Explorer %zu] save_to_disk: %d", i+1, save_result);
            }

            update_cwd_entries(full_refresh, &expl, expl.cwd.data(), expl_opts);
        }
    }

    seed_fast_rand((u64)current_time().time_since_epoch().count());

    debug_log("entering render loop...");

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::DockSpaceOverViewport(0, ImGuiDockNodeFlags_PassthruCentralNode);

        {
            ImGuiStyle &style = ImGui::GetStyle();
            f32 original_padding = style.FramePadding.y;

            style.FramePadding.y = 7.5f;

            if (ImGui::BeginMainMenuBar()) {
                if (ImGui::BeginMenu("[Windows]")) {
                    bool change_made = false;
                    static_assert((false | false) == false);
                    static_assert((false | true) == true);
                    static_assert((true | true) == true);

                    change_made |= ImGui::MenuItem("Pinned", nullptr, &win_opts.show_pinned);
                    change_made |= ImGui::MenuItem("File Operations", nullptr, &win_opts.show_file_operations);

                    change_made |= ImGui::MenuItem(explorers[0].name, nullptr, &win_opts.show_explorer_0);
                    change_made |= ImGui::MenuItem(explorers[1].name, nullptr, &win_opts.show_explorer_1);
                    change_made |= ImGui::MenuItem(explorers[2].name, nullptr, &win_opts.show_explorer_2);
                    change_made |= ImGui::MenuItem(explorers[3].name, nullptr, &win_opts.show_explorer_3);

                    change_made |= ImGui::MenuItem("Analytics", nullptr, &win_opts.show_analytics);

                #if !defined(NDEBUG)
                    change_made |= ImGui::MenuItem("Debug Log", nullptr, &win_opts.show_debug_log);
                    change_made |= ImGui::MenuItem("ImGui Demo", nullptr, &win_opts.show_demo);
                #endif

                    ImGui::EndMenu();

                    if (change_made) {
                        bool result = win_opts.save_to_disk();
                        debug_log("windows_options::save_to_disk result: %d", result);
                    }
                }
                if (ImGui::BeginMenu("[Explorer Options]")) {
                    bool change_made = false;
                    static_assert((false | false) == false);
                    static_assert((false | true) == true);
                    static_assert((true | true) == true);

                    {
                        bool changed_dotdot_dir = ImGui::MenuItem("Show '..' directory", nullptr, &expl_opts.show_dotdot_dir);
                        if (changed_dotdot_dir) {
                            for (auto &expl : explorers) {
                                update_cwd_entries(full_refresh, &expl, expl.cwd.data(), expl_opts);
                            }
                        }
                        change_made |= changed_dotdot_dir;
                    }

                    change_made |= ImGui::MenuItem("Show cwd length", nullptr, &expl_opts.show_cwd_len);
                    change_made |= ImGui::MenuItem("Show debug info", nullptr, &expl_opts.show_debug_info);

                    {
                        bool changed_dir_separator = ImGui::MenuItem("Unix directory separators", nullptr, &expl_opts.unix_directory_separator);
                        if (changed_dir_separator) {
                            for (auto &expl : explorers) {
                                update_cwd_entries(full_refresh, &expl, expl.cwd.data(), expl_opts);
                            }
                            update_pin_dir_separators(expl_opts.dir_separator());
                        }
                        change_made |= changed_dir_separator;
                    }

                    change_made |= ImGui::MenuItem("Binary size system (1024 instead of 1000)", nullptr, &expl_opts.binary_size_system);

                    if (ImGui::BeginMenu("Refreshing")) {
                        char const *refresh_modes[] = {
                            "Adaptive",
                            "Manual",
                            "Automatic",
                        };

                        static_assert(lengthof(refresh_modes) == (u64)explorer_options::refresh_mode::count);

                        ImGui::SeparatorText("Mode");
                        change_made |= ImGui::Combo("##refresh_mode", (i32 *)&expl_opts.ref_mode, refresh_modes, lengthof(refresh_modes));

                        if (expl_opts.ref_mode == explorer_options::refresh_mode::adaptive) {
                            ImGui::SeparatorText("Threshold (# items)");
                            change_made |= ImGui::InputInt("##adaptive_refresh_threshold", &expl_opts.adaptive_refresh_threshold, 100, 1000);
                        }

                        if (expl_opts.ref_mode != explorer_options::refresh_mode::manual) {
                            ImGui::SeparatorText("Interval (ms)");
                            change_made |= ImGui::InputInt("##auto_refresh_interval_ms", &expl_opts.auto_refresh_interval_ms, 100, 500);
                        }

                        ImGui::EndMenu();
                    }

                    ImGui::EndMenu();

                    if (change_made) {
                        bool result = expl_opts.save_to_disk();
                        debug_log("explorer_options::save_to_disk result: %d", result);
                    }
                }
                ImGui::EndMainMenuBar();
            }

            style.FramePadding.y = original_padding;
        }

        if (win_opts.show_pinned) {
            render_pinned_window(explorers, win_opts, expl_opts);
        }

        if (win_opts.show_file_operations) {
            render_file_ops_window();
        }

        if (win_opts.show_explorer_0) {
            render_explorer_window(explorers[0], expl_opts);
        }
        if (win_opts.show_explorer_1) {
            render_explorer_window(explorers[1], expl_opts);
        }
        if (win_opts.show_explorer_2) {
            render_explorer_window(explorers[2], expl_opts);
        }
        if (win_opts.show_explorer_3) {
            render_explorer_window(explorers[3], expl_opts);
        }

    #if !defined(NDEBUG)
        if (win_opts.show_debug_log) {
            render_debug_log_window();
        }
    #endif

        if (win_opts.show_analytics) {
            if (ImGui::Begin("Analytics")) {
            #if !defined(NDEBUG)
                char const *build_mode = "debug";
            #else
                char const *build_mode = "release";
            #endif
                ImGui::Text("Build mode : %s", build_mode);
                ImGui::Text("FPS        : %.1f FPS", io.Framerate);
                ImGui::Text("ms/frame   : %.3f", 1000.0f / io.Framerate);
            }
            ImGui::End();
        }

    #if !defined(NDEBUG)
        if (win_opts.show_demo) {
            ImGui::ShowDemoWindow();
        }
    #endif

        render(window);
    }

    return 0;
}
catch (std::exception const &except) {
    fprintf(stderr, "fatal: %s\n", except.what());
}
catch (std::string const &err) {
    fprintf(stderr, "fatal: %s\n", err.c_str());
}
catch (char const *err) {
    fprintf(stderr, "fatal: %s\n", err);
}
catch (...) {
    fprintf(stderr, "fatal: unknown error, catch(...)\n");
}
