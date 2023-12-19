#include "stdafx.hpp"
#include "data_types.hpp"
#include "common_fns.hpp"
#include "imgui_specific.hpp"
#include "util.hpp"

static
void glfw_error_callback(s32 error, char const *description) noexcept
{
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

static
GLFWwindow *init_glfw_and_imgui() noexcept
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

    char const *icon_path = "resource/swan_4.png";

    s32 icon_width, icon_height, icon_channels;
    u8 *icon_pixels = stbi_load(icon_path, &icon_width, &icon_height, &icon_channels, STBI_rgb_alpha);

    SCOPE_EXIT { stbi_image_free(icon_pixels); };

    if (icon_pixels) {
        icon.pixels = icon_pixels;
        icon.width = icon_width;
        icon.height = icon_height;
        glfwSetWindowIcon(window, 1, &icon);
    }
    else {
        print_debug_msg("FAILED to set window icon [%s]", icon_path);
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

// SEH handler function
LONG WINAPI custom_exception_handler(EXCEPTION_POINTERS *exception_info) noexcept
{
    _tprintf(_T("Unhandled exception. Generating crash dump...\n"));

    // Create a crash dump file
    HANDLE dump_file = CreateFile(
        _T("CrashDump.dmp"),
        GENERIC_WRITE,
        0,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (dump_file != INVALID_HANDLE_VALUE) {
        MINIDUMP_EXCEPTION_INFORMATION dump_info;
        dump_info.ThreadId = GetCurrentThreadId();
        dump_info.ExceptionPointers = exception_info;
        dump_info.ClientPointers = TRUE;

        // Write the crash dump
        MiniDumpWriteDump(
            GetCurrentProcess(),
            GetCurrentProcessId(),
            dump_file,
            MiniDumpWithFullMemory,
            &dump_info,
            NULL,
            NULL
        );

        CloseHandle(dump_file);
        _tprintf(_T("Crash dump generated successfully.\n"));
    } else {
        _tprintf(_T("Error creating crash dump file: %d - %s\n"), GetLastError(), get_last_error_string().c_str());
    }

    // Allow the default exception handling to continue (e.g., generate an error report)
    return EXCEPTION_CONTINUE_SEARCH;
}

void render_main_menu_bar(
    std::array<explorer_window, global_constants::num_explorers> &explorers,
    window_visibilities &window_visib,
    explorer_options &expl_opts) noexcept
{
    imgui::ScopedStyle<f32> s(ImGui::GetStyle().FramePadding.y, 10.0f);

    if (imgui::BeginMainMenuBar()) {
        if (imgui::BeginMenu("[Windows]")) {
            bool change_made = false;
            static_assert((false | false) == false);
            static_assert((false | true) == true);
            static_assert((true | true) == true);

            if (imgui::MenuItem(explorers[0].name, nullptr, &window_visib.explorer_0)) {
                change_made = true;
                explorers[0].is_window_visible.store(!explorers[0].is_window_visible.load());
            }
            if (imgui::MenuItem(explorers[1].name, nullptr, &window_visib.explorer_1)) {
                change_made = true;
                explorers[1].is_window_visible.store(!explorers[1].is_window_visible.load());
            }
            if (imgui::MenuItem(explorers[2].name, nullptr, &window_visib.explorer_2)) {
                change_made = true;
                explorers[2].is_window_visible.store(!explorers[2].is_window_visible.load());
            }
            if (imgui::MenuItem(explorers[3].name, nullptr, &window_visib.explorer_3)) {
                change_made = true;
                explorers[3].is_window_visible.store(!explorers[3].is_window_visible.load());
            }

            change_made |= imgui::MenuItem("Pinned", nullptr, &window_visib.pin_manager);
            change_made |= imgui::MenuItem("File Operations", nullptr, &window_visib.file_operations);
            change_made |= imgui::MenuItem("Analytics", nullptr, &window_visib.analytics);

        #if !defined(NDEBUG)
            change_made |= imgui::MenuItem("Debug Log", nullptr, &window_visib.debug_log);
            change_made |= imgui::MenuItem("ImGui Demo", nullptr, &window_visib.imgui_demo);
            change_made |= imgui::MenuItem("Show FA Icons", nullptr, &window_visib.fa_icons);
            change_made |= imgui::MenuItem("Show CI Icons", nullptr, &window_visib.ci_icons);
            change_made |= imgui::MenuItem("Show MD Icons", nullptr, &window_visib.md_icons);
        #endif

            imgui::EndMenu();

            if (change_made) {
                bool result = window_visib.save_to_disk();
                print_debug_msg("windows_options::save_to_disk result: %d", result);
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
                        // expl.update_cwd_entries(full_refresh, expl.cwd.data()); //! can't do this because ImGui SortSpecs will be NULL for whatever internal ImGui reason...
                        expl.update_request_from_outside = full_refresh;
                    }
                }
                change_made |= changed_dotdot_dir;
            }

            change_made |= imgui::MenuItem("Base2 size system, 1024 > 1000", nullptr, &expl_opts.binary_size_system);

            {
                bool changed_dir_separator = imgui::MenuItem("Unix directory separators", nullptr, &expl_opts.unix_directory_separator);
                if (changed_dir_separator) {
                    for (auto &expl : explorers) {
                        expl.update_cwd_entries(full_refresh, expl.cwd.data());
                    }
                    global_state::update_pin_dir_separators(expl_opts.dir_separator_utf8());
                    bool success = global_state::save_pins_to_disk();
                    print_debug_msg("save_pins_to_disk: %d", success);
                }
                change_made |= changed_dir_separator;
            }

            change_made |= imgui::MenuItem("Clear filter on navigation", nullptr, &expl_opts.clear_filter_on_cwd_change);
            change_made |= imgui::MenuItem("Alternating table rows", nullptr, &expl_opts.cwd_entries_table_alt_row_bg);
            change_made |= imgui::MenuItem("Borders in table body", nullptr, &expl_opts.cwd_entries_table_borders_in_body);
            change_made |= imgui::MenuItem("Show cwd length", nullptr, &expl_opts.show_cwd_len);
            change_made |= imgui::MenuItem("Show debug info", nullptr, &expl_opts.show_debug_info);

            if (imgui::BeginMenu("Refreshing")) {
                char const *refresh_modes[] = {
                    "Automatic",
                    "Notify",
                    "Manual",
                };

                static_assert(lengthof(refresh_modes) == (u64)explorer_options::refresh_mode::count);

                change_made |= imgui::Combo(" Mode##ref_mode", (s32 *)&expl_opts.ref_mode, refresh_modes, lengthof(refresh_modes));

                if (expl_opts.ref_mode == explorer_options::refresh_mode::automatic) {
                    static s32 refresh_itv = expl_opts.auto_refresh_interval_ms.load();
                    bool new_refresh_itv = imgui::InputInt(" Interval##auto_refresh_interval_ms", &refresh_itv, 100, 500);
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
                print_debug_msg("explorer_options::save_to_disk result: %d", result);
            }
        }

        imgui::EndMainMenuBar();
    }
}

#if defined(NDEBUG)
#   pragma comment(linker, "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")
#endif
s32 main(s32, char**)
try
{
    SetUnhandledExceptionFilter(custom_exception_handler);

    SCOPE_EXIT { std::cout << boost::stacktrace::stacktrace(); };

    GLFWwindow *window = init_glfw_and_imgui();
    if (window == nullptr) {
        return 1;
    }

    print_debug_msg("Initializing...");

    if (!explorer_init_windows_shell_com_garbage()) {
        return 2;
    }

    SCOPE_EXIT {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        imgui::DestroyContext();
        glfwDestroyWindow(window);
        glfwTerminate();
        explorer_cleanup_windows_shell_com_garbage();
    };

    set_window_icon(window);

    [[maybe_unused]] auto &io = imgui::GetIO();
    io.IniFilename = "data/swan_imgui.ini";

    seed_fast_rand((u64)current_time().time_since_epoch().count());

    {
        SYSTEM_INFO system_info;
        GetSystemInfo(&system_info);
        print_debug_msg("GetSystemInfo.dwPageSize = %d", system_info.dwPageSize);
        global_state::page_size() = system_info.dwPageSize;
    }

    auto &expl_opts = global_state::explorer_options_();
    // init explorer options
    if (!expl_opts.load_from_disk()) {
        print_debug_msg("FAILED explorer_options::load_from_disk, using default values");
        expl_opts.auto_refresh_interval_ms = 1000;
        expl_opts.ref_mode = explorer_options::refresh_mode::automatic;
        expl_opts.show_dotdot_dir = true;
    #if !defined(NDEBUG)
        expl_opts.show_debug_info = true;
        expl_opts.show_cwd_len = true;
    #endif
    }

    window_visibilities window_visib = {};
    if (!window_visib.load_from_disk()) {
        print_debug_msg("FAILED windows_options::load_from_disk, using default values");
        window_visib.explorer_1 = true;
        window_visib.pin_manager = true;
    #if !defined(NDEBUG)
        window_visib.imgui_demo = true;
    #endif
    }

    misc_options misc_opts = {};
    // init misc. options
    if (!misc_opts.load_from_disk()) {
        print_debug_msg("FAILED misc_options::load_from_disk, using default values");
    }

    imgui::StyleColorsDark();
    apply_swan_style_overrides();

    // init pins
    {
        auto [success, num_pins_loaded] = global_state::load_pins_from_disk(expl_opts.dir_separator_utf8());
        if (!success) {
            print_debug_msg("FAILED global_state::load_pins_from_disk");
        } else {
            print_debug_msg("global_state::load_pins_from_disk success, loaded %zu pins", num_pins_loaded);
        }
    }

    auto &explorers = global_state::explorers();
    // init explorers
    {
        char const *names[global_constants::num_explorers] = {
            "Explorer 1",
            "Explorer 2",
            "Explorer 3",
            "Explorer 4"
        };

        for (u64 i = 0; i < explorers.size(); ++i) {
            auto &expl = explorers[i];

            expl.id = s32(i);
            expl.name = names[i];
            expl.filter_error.reserve(1024);

            bool load_result = explorers[i].load_from_disk(expl_opts.dir_separator_utf8());
            print_debug_msg("[ %d ] explorer_window::load_from_disk: %d", i+1, load_result);

            if (!load_result) {
                swan_path_t startup_path = {};
                expl.cwd = startup_path;

                bool save_result = explorers[i].save_to_disk();
                print_debug_msg("[%s] save_to_disk: %d", expl.name, save_result);
            }

            bool starting_dir_exists = expl.update_cwd_entries(query_filesystem, expl.cwd.data());
            if (starting_dir_exists) {
                expl.set_latest_valid_cwd(expl.cwd); // this may mutate filter
                (void) expl.update_cwd_entries(filter, expl.cwd.data());
            }
        }
    }

    std::atomic<s32> window_close_flag = glfwWindowShouldClose(window);

    std::array<s32, swan_windows::count> window_render_order = {
        swan_windows::explorer_0,
        swan_windows::explorer_1,
        swan_windows::explorer_2,
        swan_windows::explorer_3,
        swan_windows::pin_manager,
        swan_windows::file_operations,
        swan_windows::analytics,
        swan_windows::debug_log,
        swan_windows::imgui_demo,
        swan_windows::icon_font_browser_font_awesome,
        swan_windows::icon_font_browser_codicon,
        swan_windows::icon_font_browser_material_design,
    };

    {
        s32 last_focused_window;
        if (!global_state::load_focused_window_from_disk(last_focused_window)) {
            last_focused_window = swan_windows::explorer_0;
        } else {
            assert(last_focused_window != -1);
            auto last_focused_window_it = std::find(window_render_order.begin(), window_render_order.end(), last_focused_window);
            std::swap(*last_focused_window_it, window_render_order.back());
        }
    }

    print_debug_msg("Entering render loop...");

    for (
        ;
        !window_close_flag.load();
         window_close_flag.store(glfwWindowShouldClose(window))
    ) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        imgui::NewFrame();

        // this is to prevent the ugly blue border (nav focus I think it's called?) when pressing tab or escape
        if (one_of(GLFW_PRESS, { glfwGetKey(window, GLFW_KEY_ESCAPE), glfwGetKey(window, GLFW_KEY_TAB) })) {
            ImGui::SetWindowFocus(nullptr);
        }

        window_visibilities visib_at_frame_start = window_visib;

        SCOPE_EXIT {
            render(window);

            bool window_visibilities_changed = memcmp(&window_visib, &visib_at_frame_start, sizeof(window_visibilities)) != 0;
            if (window_visibilities_changed) {
                window_visib.save_to_disk();
            }
        };

        static s32 last_focused_window = window_render_order.back();
        {
            s32 focused_now = global_state::focused_window();
            assert(focused_now != -1);
            if (last_focused_window != focused_now) {
                auto focused_now_it = std::find(window_render_order.begin(), window_render_order.end(), last_focused_window);
                std::swap(*focused_now_it, window_render_order.back());
            }
        }

        imgui::DockSpaceOverViewport(0, ImGuiDockNodeFlags_PassthruCentralNode);

        render_main_menu_bar(explorers, window_visib, expl_opts);

        for (s32 window_code : window_render_order) {
            switch (window_code) {
                case swan_windows::explorer_0: {
                    if (window_visib.explorer_0) {
                        swan_windows::render_explorer(explorers[0], window_visib, window_visib.explorer_0);
                    }
                    break;
                }
                case swan_windows::explorer_1: {
                    if (window_visib.explorer_1) {
                        swan_windows::render_explorer(explorers[1], window_visib, window_visib.explorer_1);
                    }
                    break;
                }
                case swan_windows::explorer_2: {
                    if (window_visib.explorer_2) {
                        swan_windows::render_explorer(explorers[2], window_visib, window_visib.explorer_2);
                    }
                    break;
                }
                case swan_windows::explorer_3: {
                    if (window_visib.explorer_3) {
                        swan_windows::render_explorer(explorers[3], window_visib, window_visib.explorer_3);
                    }
                    break;
                }
                case swan_windows::pin_manager: {
                    if (window_visib.pin_manager) {
                        swan_windows::render_pin_manager(explorers, window_visib.pin_manager);
                    }
                    break;
                }
                case swan_windows::file_operations: {
                    if (window_visib.file_operations) {
                        swan_windows::render_file_operations();
                    }
                    break;
                }
                case swan_windows::analytics: {
                    if (window_visib.analytics) {
                        if (imgui::Begin(" Analytics ", &window_visib.analytics)) {
                            if (imgui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)) {
                                global_state::save_focused_window(swan_windows::analytics);
                            }

                        #if !defined(NDEBUG)
                            char const *build_mode = "Debug";
                        #else
                            char const *build_mode = "Release";
                        #endif

                            imgui::Text("Build mode : %s", build_mode);
                            imgui::Text("FPS        : %.1f FPS", io.Framerate);
                            imgui::Text("ms/frame   : %.3f", 1000.0f / io.Framerate);
                        }
                        imgui::End();
                    }
                    break;
                }
                case swan_windows::debug_log: {
                    if (window_visib.debug_log) {
                        swan_windows::render_debug_log(window_visib.debug_log);
                    }
                    break;
                }
                case swan_windows::imgui_demo: {
                    //! since ShowDemoWindow calls End() for you, we cannot use global_state::save_focused_window as per usual... oh well
                    if (window_visib.imgui_demo) {
                        imgui::ShowDemoWindow(&window_visib.imgui_demo);
                    }
                    break;
                }
                case swan_windows::icon_font_browser_font_awesome: {
                    static icon_font_browser_state fa_browser = { {}, 10, global_constants::icon_font_glyphs_font_awesome() };
                    if (window_visib.fa_icons) {
                        swan_windows::render_icon_font_browser(
                            swan_windows::icon_font_browser_font_awesome,
                            fa_browser,
                            window_visib.fa_icons,
                            "Font Awesome",
                            "ICON_FA_",
                            global_constants::icon_font_glyphs_font_awesome);
                    }
                    break;
                }
                case swan_windows::icon_font_browser_codicon: {
                    static icon_font_browser_state ci_browser = { {}, 10, global_constants::icon_font_glyphs_codicon() };
                    if (window_visib.ci_icons) {
                        swan_windows::render_icon_font_browser(
                            swan_windows::icon_font_browser_codicon,
                            ci_browser,
                            window_visib.ci_icons,
                            "Codicons",
                            "ICON_CI_",
                            global_constants::icon_font_glyphs_codicon);
                    }
                    break;
                }
                case swan_windows::icon_font_browser_material_design: {
                    static icon_font_browser_state md_browser = { {}, 10, global_constants::icon_font_glyphs_material_design() };
                    if (window_visib.md_icons) {
                        swan_windows::render_icon_font_browser(
                            swan_windows::icon_font_browser_material_design,
                            md_browser,
                            window_visib.md_icons,
                            "Material Design",
                            "ICON_MD_",
                            global_constants::icon_font_glyphs_material_design);
                    }
                    break;
                }
            }
        }

        if (swan_popup_modals::is_open_single_rename()) {
            swan_popup_modals::render_single_rename();
        }
        if (swan_popup_modals::is_open_bulk_rename()) {
            swan_popup_modals::render_bulk_rename();
        }
        if (swan_popup_modals::is_open_new_pin()) {
            swan_popup_modals::render_new_pin();
        }
        if (swan_popup_modals::is_open_edit_pin()) {
            swan_popup_modals::render_edit_pin();
        }
        if (swan_popup_modals::is_open_error()) {
            swan_popup_modals::render_error();
        }
    }

    //? I don't know if this is safe to do, would be good to look into it,
    //? but as of now the program seems to work...
    std::exit(0); // kill all change notif threads

    return 0;
}
catch (std::exception const &except) {
    fprintf(stderr, "fatal: %s\n", except.what());
    std::cout << boost::stacktrace::stacktrace();
}
catch (std::string const &err) {
    fprintf(stderr, "fatal: %s\n", err.c_str());
    std::cout << boost::stacktrace::stacktrace();
}
catch (char const *err) {
    fprintf(stderr, "fatal: %s\n", err);
    std::cout << boost::stacktrace::stacktrace();
}
catch (...) {
    fprintf(stderr, "fatal: unknown error, catch(...)\n");
    std::cout << boost::stacktrace::stacktrace();
}
