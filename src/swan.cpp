#include "stdafx.hpp"
#include "common.hpp"
#include "imgui_specific.hpp"
#include "util.hpp"

namespace imgui = ImGui;

static
void glfw_error_callback(s32 error, char const *description) noexcept
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
    imgui::CreateContext();
    ImGuiIO &io = imgui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigDockingWithShift = true;
    imgui::SetNextWindowPos(ImVec2(0, 0));
    imgui::SetNextWindowSize(io.DisplaySize);
    imgui::SetNextWindowSizeConstraints(io.DisplaySize, io.DisplaySize);

    // Setup Platform/Renderer backends
    {
        char const *glsl_version = "#version 130";
        ImGui_ImplGlfw_InitForOpenGL(window, true);
        ImGui_ImplOpenGL3_Init(glsl_version);
    }

    {
        ImFontConfig config;
        config.MergeMode = true;

        ImFont *font = nullptr;

    #if 0
        font = io.Fonts->AddFontDefault();
        assert(font != nullptr);
    #endif

    #if 1
        font = io.Fonts->AddFontFromFileTTF("data/RobotoMono-Regular.ttf", 18.0f);
        assert(font != nullptr);
    #endif

    #if 0
        font = io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/consola.ttf", 17.0f);
        assert(font != nullptr);
    #endif

    #if 0
        font = io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/arialuni.ttf", 20.0f);
        assert(font != nullptr);
    #endif

        font = io.Fonts->AddFontFromFileTTF("data/CascadiaMonoPL.ttf", 16.0f, &config, io.Fonts->GetGlyphRangesCyrillic());
        assert(font != nullptr);

    #if 1
        // font awesome
        {
            f32 size = 16;
            static ImWchar const icons_ranges[] = { ICON_MIN_FA, ICON_MAX_16_FA, 0 };
            ImFontConfig icons_config;
            icons_config.MergeMode = true;
            icons_config.PixelSnapH = true;
            icons_config.GlyphOffset.x = 0.25f;
            icons_config.GlyphOffset.y = 0;
            icons_config.GlyphMinAdvanceX = size;
            icons_config.GlyphMaxAdvanceX = size;
            io.Fonts->AddFontFromFileTTF("data/" FONT_ICON_FILE_NAME_FAS, size, &icons_config, icons_ranges);
        }
        // codicons
        {
            f32 size = 18;
            static ImWchar const icons_ranges[] = { ICON_MIN_CI, ICON_MAX_16_CI, 0 };
            ImFontConfig icons_config;
            icons_config.MergeMode = true;
            icons_config.PixelSnapH = true;
            icons_config.GlyphOffset.x = 0;
            icons_config.GlyphOffset.y = 3;
            icons_config.GlyphMinAdvanceX = size;
            icons_config.GlyphMaxAdvanceX = size;
            io.Fonts->AddFontFromFileTTF("data/" FONT_ICON_FILE_NAME_CI, size, &icons_config, icons_ranges);
        }
        // material design
        {
            f32 size = 18;
            static ImWchar const icons_ranges[] = { ICON_MIN_MD, ICON_MAX_16_MD, 0 };
            ImFontConfig icons_config;
            icons_config.MergeMode = true;
            icons_config.PixelSnapH = true;
            icons_config.GlyphOffset.x = 0;
            icons_config.GlyphOffset.y = 3;
            icons_config.GlyphMinAdvanceX = size;
            icons_config.GlyphMaxAdvanceX = size;
            io.Fonts->AddFontFromFileTTF("data/" FONT_ICON_FILE_NAME_MD, 13, &icons_config, icons_ranges);
        }
    #endif
    }

    return window;
}

static
void set_window_icon(GLFWwindow *window) noexcept
{
    GLFWimage icon;
    icon.pixels = nullptr;
    icon.width = 0;
    icon.height = 0;

    s32 icon_width, icon_height, icon_channels;
    u8 *icon_pixels = stbi_load("resource/swan.png", &icon_width, &icon_height, &icon_channels, STBI_rgb_alpha);

    SCOPE_EXIT { stbi_image_free(icon_pixels); };
    // auto cleanup_icon_pixels_routine = make_on_scope_exit([icon_pixels] {
    //     stbi_image_free(icon_pixels);
    // });

    if (icon_pixels) {
        icon.pixels = icon_pixels;
        icon.width = icon_width;
        icon.height = icon_height;

        glfwSetWindowIcon(window, 1, &icon);
    }
}

static
void render(GLFWwindow *window) noexcept
{
    imgui::Render();

    s32 display_w, display_h;
    ImVec4 clear_color(0.45f, 0.55f, 0.60f, 1.00f);

    glfwGetFramebufferSize(window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(imgui::GetDrawData());

    glfwSwapBuffers(window);
}

#if defined(NDEBUG)
#   pragma comment(linker, "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")
#endif
s32 main(s32, char**)
try
{
    std::atexit([]() {
        std::cout << boost::stacktrace::stacktrace();
    });

    GLFWwindow *window = init_glfw_and_imgui();
    if (window == nullptr) {
        return 1;
    }

    debug_log("Initializing...");

    if (!explorer_init_windows_shell_com_garbage()) {
        return 1;
    }

    SCOPE_EXIT {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        imgui::DestroyContext();
        glfwDestroyWindow(window);
        glfwTerminate();
        explorer_cleanup_windows_shell_com_garbage();
    };
    // auto cleanup_routine = make_on_scope_exit([window]() {
    //     ImGui_ImplOpenGL3_Shutdown();
    //     ImGui_ImplGlfw_Shutdown();
    //     imgui::DestroyContext();
    //     glfwDestroyWindow(window);
    //     glfwTerminate();
    //     explorer_cleanup_windows_shell_com_garbage();
    // });

    set_window_icon(window);

    [[maybe_unused]] auto &io = imgui::GetIO();
    io.IniFilename = "data/swan_imgui.ini";

    seed_fast_rand((u64)current_time().time_since_epoch().count());

    // {
    //     char const *last_focused_window = nullptr;
    //     if (load_focused_window_from_disk(last_focused_window)) {
    //         imgui::SetWindowFocus(last_focused_window);
    //     }
    // }

    {
        SYSTEM_INFO system_info;
        GetSystemInfo(&system_info);
        debug_log("GetSystemInfo.dwPageSize = %d", system_info.dwPageSize);
        set_page_size(system_info.dwPageSize);
    }

    auto &expl_opts = get_explorer_options();
    // init explorer options
    if (!expl_opts.load_from_disk()) {
        debug_log("explorer_options::load_from_disk failed, setting defaults");
        expl_opts.auto_refresh_interval_ms = 1000;
        expl_opts.ref_mode = explorer_options::refresh_mode::automatic;
        expl_opts.show_dotdot_dir = true;
    #if !defined(NDEBUG)
        expl_opts.show_debug_info = true;
        expl_opts.show_cwd_len = true;
    #endif
    }

    windows_options win_opts = {};
    // init window options
    if (!win_opts.load_from_disk()) {
        debug_log("windows_options::load_from_disk failed, setting defaults");
        win_opts.show_explorer_1 = true;
        win_opts.show_pins_mgr = true;
    #if !defined(NDEBUG)
        win_opts.show_demo = true;
    #endif
    }

    misc_options misc_opts = {};
    // init misc. options
    if (!misc_opts.load_from_disk()) {
        debug_log("misc_options::load_from_disk failed, setting defaults");
    }

    imgui::StyleColorsDark();
    apply_swan_style_overrides();

    // init pins
    {
        auto [success, num_pins_loaded] = load_pins_from_disk(expl_opts.dir_separator_utf8());
        if (!success) {
            debug_log("load_pins_from_disk failed");
        } else {
            debug_log("load_pins_from_disk success, loaded %zu pins", num_pins_loaded);
        }
    }

    constexpr u64 num_explorers = 4;
    std::array<explorer_window, num_explorers> explorers = {};
    // init explorers
    {
        char const *names[] = {
            "Explorer 1",
            "Explorer 2",
            "Explorer 3",
            "Explorer 4"
        };

        for (u64 i = 0; i < explorers.size(); ++i) {
            auto &expl = explorers[i];

            expl.id = i+1;
            expl.name = names[i];
            expl.filter_error.reserve(1024);

            bool load_result = explorers[i].load_from_disk(expl_opts.dir_separator_utf8());
            debug_log("[ %d ] explorer_window::load_from_disk: %d", i+1, load_result);

            if (!load_result) {
                swan_path_t startup_path = {};
                expl.cwd = startup_path;

                bool save_result = explorers[i].save_to_disk();
                debug_log("[%s] save_to_disk: %d", expl.name, save_result);
            }

            bool starting_dir_exists = update_cwd_entries(full_refresh, &expl, expl.cwd.data());
            if (starting_dir_exists) {
                expl.set_latest_valid_cwd_then_notify(expl.cwd);
            }
        }
    }

    std::atomic<s32> window_close_flag = glfwWindowShouldClose(window);

    std::jthread expl_change_notif_thread_0([&]() { explorer_change_notif_thread_func(explorers[0], window_close_flag); });
    std::jthread expl_change_notif_thread_1([&]() { explorer_change_notif_thread_func(explorers[1], window_close_flag); });
    std::jthread expl_change_notif_thread_2([&]() { explorer_change_notif_thread_func(explorers[2], window_close_flag); });
    std::jthread expl_change_notif_thread_3([&]() { explorer_change_notif_thread_func(explorers[3], window_close_flag); });

    // (void) set_thread_priority(THREAD_PRIORITY_HIGHEST);

    debug_log("Entering render loop...");

    for (
        ;
        !window_close_flag.load();
         window_close_flag.store(glfwWindowShouldClose(window))
    ) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        imgui::NewFrame();

        imgui::DockSpaceOverViewport(0, ImGuiDockNodeFlags_PassthruCentralNode);

        // main menu bar
        {
            imgui_scoped_style<f32> s(ImGui::GetStyle().FramePadding.y, 10.0f);

            if (imgui::BeginMainMenuBar()) {
                if (imgui::BeginMenu("[Windows]")) {
                    bool change_made = false;
                    static_assert((false | false) == false);
                    static_assert((false | true) == true);
                    static_assert((true | true) == true);

                    if (imgui::MenuItem(explorers[0].name, nullptr, &win_opts.show_explorer_0)) {
                        change_made = true;
                        explorers[0].is_window_visible.store(!explorers[0].is_window_visible.load());
                    }
                    if (imgui::MenuItem(explorers[1].name, nullptr, &win_opts.show_explorer_1)) {
                        change_made = true;
                        explorers[1].is_window_visible.store(!explorers[1].is_window_visible.load());
                    }
                    if (imgui::MenuItem(explorers[2].name, nullptr, &win_opts.show_explorer_2)) {
                        change_made = true;
                        explorers[2].is_window_visible.store(!explorers[2].is_window_visible.load());
                    }
                    if (imgui::MenuItem(explorers[3].name, nullptr, &win_opts.show_explorer_3)) {
                        change_made = true;
                        explorers[3].is_window_visible.store(!explorers[3].is_window_visible.load());
                    }

                    change_made |= imgui::MenuItem("Pinned", nullptr, &win_opts.show_pins_mgr);
                    change_made |= imgui::MenuItem("File Operations", nullptr, &win_opts.show_file_operations);
                    change_made |= imgui::MenuItem("Analytics", nullptr, &win_opts.show_analytics);

                #if !defined(NDEBUG)
                    change_made |= imgui::MenuItem("Debug Log", nullptr, &win_opts.show_debug_log);
                    change_made |= imgui::MenuItem("ImGui Demo", nullptr, &win_opts.show_demo);
                    change_made |= imgui::MenuItem("Show FA Icons", nullptr, &win_opts.show_fa_icons);
                    change_made |= imgui::MenuItem("Show CI Icons", nullptr, &win_opts.show_ci_icons);
                    change_made |= imgui::MenuItem("Show MD Icons", nullptr, &win_opts.show_md_icons);
                #endif

                    imgui::EndMenu();

                    if (change_made) {
                        bool result = win_opts.save_to_disk();
                        debug_log("windows_options::save_to_disk result: %d", result);
                    }
                }
                if (imgui::BeginMenu("[Explorer Options]")) {
                    bool change_made = false;
                    static_assert((false | false) == false);
                    static_assert((false | true) == true);
                    static_assert((true | true) == true);

                    {
                        bool changed_dotdot_dir = imgui::MenuItem("Show '..' directory", nullptr, &expl_opts.show_dotdot_dir);
                        if (changed_dotdot_dir) {
                            for (auto &expl : explorers) {
                                // update_cwd_entries(full_refresh, &expl, expl.cwd.data());
                                expl.refresh_notif_time.store(current_time(), std::memory_order::seq_cst);
                            }
                        }
                        change_made |= changed_dotdot_dir;
                    }

                    change_made |= imgui::MenuItem("Base2 size system, 1024 > 1000", nullptr, &expl_opts.binary_size_system);

                    {
                        bool changed_dir_separator = imgui::MenuItem("Unix directory separators", nullptr, &expl_opts.unix_directory_separator);
                        if (changed_dir_separator) {
                            for (auto &expl : explorers) {
                                update_cwd_entries(full_refresh, &expl, expl.cwd.data());
                            }
                            update_pin_dir_separators(expl_opts.dir_separator_utf8());
                            bool success = save_pins_to_disk();
                            debug_log("save_pins_to_disk: %d", success);
                        }
                        change_made |= changed_dir_separator;
                    }

                    change_made |= imgui::MenuItem("Alternating table rows", nullptr, &expl_opts.cwd_entries_table_alt_row_bg);
                    change_made |= imgui::MenuItem("Borders in table body", nullptr, &expl_opts.cwd_entries_table_borders_in_body);
                    change_made |= imgui::MenuItem("Show cwd length", nullptr, &expl_opts.show_cwd_len);
                    change_made |= imgui::MenuItem("Show debug info", nullptr, &expl_opts.show_debug_info);

                    if (imgui::BeginMenu("Refreshing")) {
                        char const *refresh_modes[] = {
                            "Automatic",
                            "Manual",
                        };

                        static_assert(lengthof(refresh_modes) == (u64)explorer_options::refresh_mode::count);

                        imgui::SeparatorText("Mode");
                        change_made |= imgui::Combo("##refresh_mode", (s32 *)&expl_opts.ref_mode, refresh_modes, lengthof(refresh_modes));

                        if (expl_opts.ref_mode == explorer_options::refresh_mode::automatic) {
                            imgui::SeparatorText("Interval (ms)");
                            static s32 refresh_itv = expl_opts.auto_refresh_interval_ms.load();
                            bool new_refresh_itv = imgui::InputInt("##auto_refresh_interval_ms", &refresh_itv, 100, 500);
                            change_made |= new_refresh_itv;
                            if (new_refresh_itv) {
                                expl_opts.auto_refresh_interval_ms.store(refresh_itv);
                            }
                        }

                        imgui::EndMenu();
                    }

                    imgui::EndMenu();

                    if (change_made) {
                        bool result = expl_opts.save_to_disk();
                        debug_log("explorer_options::save_to_disk result: %d", result);
                    }
                }

                imgui::EndMainMenuBar();
            }
        }

        if (win_opts.show_pins_mgr) {
            swan_render_window_pinned_directories(explorers, win_opts.show_pins_mgr);
        }

        if (win_opts.show_file_operations) {
            swan_render_window_file_operations();
        }

        if (win_opts.show_explorer_0) {
            swan_render_window_explorer(explorers[0], win_opts, win_opts.show_explorer_0);
        }
        if (win_opts.show_explorer_1) {
            swan_render_window_explorer(explorers[1], win_opts, win_opts.show_explorer_1);
        }
        if (win_opts.show_explorer_2) {
            swan_render_window_explorer(explorers[2], win_opts, win_opts.show_explorer_2);
        }
        if (win_opts.show_explorer_3) {
            swan_render_window_explorer(explorers[3], win_opts, win_opts.show_explorer_3);
        }

    #if !defined(NDEBUG)
        if (win_opts.show_debug_log) {
            swan_render_window_debug_log(win_opts.show_debug_log);
        }
    {
        static icon_browser fa_browser = { {}, 10, get_font_awesome_icons() };
        static icon_browser ci_browser = { {}, 10, get_codicon_icons() };
        static icon_browser md_browser = { {}, 10, get_material_design_icons() };

        if (win_opts.show_fa_icons) {
            swan_render_window_icon_browser(fa_browser, win_opts.show_fa_icons, "Font Awesome", "ICON_FA_", get_font_awesome_icons);
        }
        if (win_opts.show_ci_icons) {
            swan_render_window_icon_browser(ci_browser, win_opts.show_ci_icons, "Codicons", "ICON_CI_", get_codicon_icons);
        }
        if (win_opts.show_md_icons) {
            swan_render_window_icon_browser(md_browser, win_opts.show_md_icons, "Material Design", "ICON_MD_", get_material_design_icons);
        }
    }
    #endif

        if (swan_is_popup_modal_open_single_rename()) {
            swan_render_popup_modal_single_rename();
        }
        if (swan_is_popup_modal_open_bulk_rename()) {
            swan_render_popup_modal_bulk_rename();
        }
        if (swan_is_popup_modal_open_new_pin()) {
            swan_render_popup_modal_new_pin();
        }
        if (swan_is_popup_modal_open_edit_pin()) {
            swan_render_popup_modal_edit_pin();
        }
        if (swan_is_popup_modal_open_error()) {
            swan_render_popup_modal_error();
        }

        if (win_opts.show_analytics) {
            if (imgui::Begin(" Analytics ", &win_opts.show_analytics)) {
            #if !defined(NDEBUG)
                char const *build_mode = "debug";
            #else
                char const *build_mode = "release";
            #endif
                imgui::Text("Build mode : %s", build_mode);
                imgui::Text("FPS        : %.1f FPS", io.Framerate);
                imgui::Text("ms/frame   : %.3f", 1000.0f / io.Framerate);
            }
            imgui::End();
        }

    #if !defined(NDEBUG)
        if (win_opts.show_demo) {
            imgui::ShowDemoWindow(&win_opts.show_demo);
        }
    #endif

        render(window);
    }

    //? I don't know if this is safe to do, would be good to look into it,
    //? but as of now the program seems to work...
    std::exit(0); // kill all change notif threads

    return 0;
}
catch (std::exception const &except) {
    fprintf(stderr, "fatal: %s\n", except.what());
    // std::cout << boost::stacktrace::stacktrace();
}
catch (std::string const &err) {
    fprintf(stderr, "fatal: %s\n", err.c_str());
    // std::cout << boost::stacktrace::stacktrace();
}
catch (char const *err) {
    fprintf(stderr, "fatal: %s\n", err);
    // std::cout << boost::stacktrace::stacktrace();
}
catch (...) {
    fprintf(stderr, "fatal: unknown error, catch(...)\n");
    // std::cout << boost::stacktrace::stacktrace();
}
