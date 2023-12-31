#include "stdafx.hpp"
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

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    imgui::CreateContext();
    ImGuiIO &io = imgui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigDockingWithShift = true;
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
        font = io.Fonts->AddFontFromFileTTF("data/fonts/RobotoMono-Regular.ttf", 18.0f);
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

        font = io.Fonts->AddFontFromFileTTF("data/fonts/CascadiaMonoPL.ttf", 16.0f, &config, io.Fonts->GetGlyphRangesCyrillic());
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
            io.Fonts->AddFontFromFileTTF("data/fonts/" FONT_ICON_FILE_NAME_FAS, size, &icons_config, icons_ranges);
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
            io.Fonts->AddFontFromFileTTF("data/fonts/" FONT_ICON_FILE_NAME_CI, size, &icons_config, icons_ranges);
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
            io.Fonts->AddFontFromFileTTF("data/fonts/" FONT_ICON_FILE_NAME_MD, 13, &icons_config, icons_ranges);
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

void render_main_menu_bar(std::array<explorer_window, global_constants::num_explorers> &explorers) noexcept
{
    imgui::ScopedStyle<f32> s(ImGui::GetStyle().FramePadding.y, 10.0f);

    if (imgui::BeginMainMenuBar()) {
        if (imgui::BeginMenu("[Windows]")) {
            bool change_made = false;
            static_assert((false | false) == false);
            static_assert((false | true) == true);
            static_assert((true | true) == true);

            change_made |= imgui::MenuItem(explorers[0].name, nullptr, &global_state::settings().show.explorer_0);
            change_made |= imgui::MenuItem(explorers[1].name, nullptr, &global_state::settings().show.explorer_1);
            change_made |= imgui::MenuItem(explorers[2].name, nullptr, &global_state::settings().show.explorer_2);
            change_made |= imgui::MenuItem(explorers[3].name, nullptr, &global_state::settings().show.explorer_3);

            change_made |= imgui::MenuItem(swan_windows::get_name(swan_windows::pin_manager), nullptr, &global_state::settings().show.pin_manager);
            change_made |= imgui::MenuItem(swan_windows::get_name(swan_windows::file_operations), nullptr, &global_state::settings().show.file_operations);
            change_made |= imgui::MenuItem(swan_windows::get_name(swan_windows::analytics), nullptr, &global_state::settings().show.analytics);
            change_made |= imgui::MenuItem(swan_windows::get_name(swan_windows::settings), nullptr, &global_state::settings().show.settings);

        #if !defined(NDEBUG)
            change_made |= imgui::MenuItem(swan_windows::get_name(swan_windows::debug_log), nullptr, &global_state::settings().show.debug_log);
            change_made |= imgui::MenuItem(swan_windows::get_name(swan_windows::imgui_demo), nullptr, &global_state::settings().show.imgui_demo);
            change_made |= imgui::MenuItem(swan_windows::get_name(swan_windows::icon_font_browser_font_awesome), nullptr, &global_state::settings().show.fa_icons);
            change_made |= imgui::MenuItem(swan_windows::get_name(swan_windows::icon_font_browser_codicon), nullptr, &global_state::settings().show.ci_icons);
            change_made |= imgui::MenuItem(swan_windows::get_name(swan_windows::icon_font_browser_material_design), nullptr, &global_state::settings().show.md_icons);
        #endif

            imgui::EndMenu();

            if (change_made) {
                (void) global_state::settings().save_to_disk();
            }
        }
        if (imgui::BeginMenu("[Explorer Settings]")) {
            bool change_made = false;
            static_assert((false | false) == false);
            static_assert((false | true) == true);
            static_assert((true | true) == true);

            if (imgui::MenuItem("Show '..' directory", nullptr, &global_state::settings().show_dotdot_dir)) {
                change_made = true;
                for (auto &expl : explorers) {
                    expl.update_request_from_outside = full_refresh;
                }
            }
            {
                static bool binary_size_system = {};
                binary_size_system = global_state::settings().size_unit_multiplier == 1024;

                if (imgui::MenuItem("Base2 size system, 1024 > 1000", nullptr, &binary_size_system)) {
                    change_made = true;

                    global_state::settings().size_unit_multiplier = binary_size_system ? 1024 : 1000;

                    for (auto &expl : explorers) {
                        expl.update_request_from_outside = full_refresh;
                    }
                }
            }
            {
                static bool unix_directory_separator = {};
                unix_directory_separator = global_state::settings().dir_separator_utf8 == '/';

                if (imgui::MenuItem("Unix directory separators", nullptr, &unix_directory_separator)) {
                    change_made = true;

                    char new_utf8_separator = unix_directory_separator ? '/' : '\\';

                    global_state::settings().dir_separator_utf8 = new_utf8_separator;
                    global_state::settings().dir_separator_utf16 = static_cast<wchar_t>(new_utf8_separator);
                    global_state::update_pin_dir_separators(new_utf8_separator);

                    for (auto &expl : explorers) {
                        path_force_separator(expl.cwd, new_utf8_separator);
                        // expl.update_request_from_outside = full_refresh;
                    }
                }
            }

            change_made |= imgui::MenuItem("Clear filter on navigation", nullptr, &global_state::settings().clear_filter_on_cwd_change);
            change_made |= imgui::MenuItem("Alternating table rows", nullptr, &global_state::settings().cwd_entries_table_alt_row_bg);
            change_made |= imgui::MenuItem("Borders in table body", nullptr, &global_state::settings().cwd_entries_table_borders_in_body);
            change_made |= imgui::MenuItem("Show debug info", nullptr, &global_state::settings().show_debug_info);

            if (imgui::BeginMenu("Refresh mode")) {
                char const *labels[] = {
                    "Automatic",
                    "Notify   ",
                    "Manual   ",
                };
                {
                    imgui::ScopedItemWidth w(imgui::CalcTextSize(labels[0]).x + 50);
                    imgui::ScopedStyle<ImVec2> p(imgui::GetStyle().FramePadding, { 6, 4 });
                    change_made |= imgui::Combo("##expl_refresh_mode", (s32 *)&global_state::settings().expl_refresh_mode, labels, (s32)lengthof(labels));
                }
                imgui::EndMenu();
            }

            imgui::EndMenu();

            if (change_made) {
                (void) global_state::settings().save_to_disk();
            }
        }

        imgui::EndMainMenuBar();
    }
}

void render_analytics() noexcept
{
    if (imgui::Begin(swan_windows::get_name(swan_windows::analytics), &global_state::settings().show.analytics)) {
        if (imgui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)) {
            global_state::save_focused_window(swan_windows::analytics);
        }

    #if !defined(NDEBUG)
        char const *build_mode = "Debug";
    #else
        char const *build_mode = "Release";
    #endif

        auto &io = imgui::GetIO();
        imgui::Text("Build mode : %s", build_mode);
        imgui::Text("FPS        : %.1f FPS", io.Framerate);
        imgui::Text("ms/frame   : %.3f", 1000.0f / io.Framerate);
    }
    imgui::End();
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

    (void) global_state::settings().load_from_disk();

    if (global_state::settings().start_with_window_maximized) {
        glfwMaximizeWindow(window);
    }
    if (global_state::settings().start_with_previous_window_pos_and_size) {
        glfwSetWindowPos(window, global_state::settings().window_x, global_state::settings().window_y);
        glfwSetWindowSize(window, global_state::settings().window_w, global_state::settings().window_h);
    }

    static time_point_t last_window_move_or_resize_time = current_time();
    static bool window_pos_or_size_needs_write = false;

    glfwSetWindowPosCallback(window, [](GLFWwindow *, s32 new_x, s32 new_y) {
        global_state::settings().window_x = new_x;
        global_state::settings().window_y = new_y;
        last_window_move_or_resize_time = current_time();
        window_pos_or_size_needs_write = true;
    });
    glfwSetWindowSizeCallback(window, [](GLFWwindow *, s32 new_w, s32 new_h) {
        global_state::settings().window_w = new_w;
        global_state::settings().window_h = new_h;
        last_window_move_or_resize_time = current_time();
        window_pos_or_size_needs_write = true;
    });

    imgui::StyleColorsDark();
    apply_swan_style_overrides();

    (void) global_state::load_pins_from_disk(global_state::settings().dir_separator_utf8);

    auto &explorers = global_state::explorers();
    // init explorers
    {
        char const *names[global_constants::num_explorers] = {
            swan_windows::get_name(swan_windows::explorer_0),
            swan_windows::get_name(swan_windows::explorer_1),
            swan_windows::get_name(swan_windows::explorer_2),
            swan_windows::get_name(swan_windows::explorer_3),
        };

        for (u64 i = 0; i < explorers.size(); ++i) {
            auto &expl = explorers[i];

            expl.id = s32(i);
            expl.name = names[i];
            expl.filter_error.reserve(1024);

            bool load_result = explorers[i].load_from_disk(global_state::settings().dir_separator_utf8);
            print_debug_msg("[ %d ] explorer_window::load_from_disk: %d", i, load_result);

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
        swan_windows::settings,
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

        static s32 last_focused_window = window_render_order.back();
        {
            s32 focused_now = global_state::focused_window();
            assert(focused_now != -1);

            if (last_focused_window != focused_now) {
                auto focused_now_it = std::find(window_render_order.begin(), window_render_order.end(), last_focused_window);
                std::swap(*focused_now_it, window_render_order.back());
            }

            // this is to prevent the ugly blue border (nav focus I think it's called?) when pressing escape
            if (one_of(GLFW_PRESS, { glfwGetKey(window, GLFW_KEY_ESCAPE) })) {
                // ImGui::SetWindowFocus(nullptr);
                ImGui::SetWindowFocus(swan_windows::get_name(focused_now));
            }
        }

        if (window_pos_or_size_needs_write && compute_diff_ms(last_window_move_or_resize_time, current_time()) > 1000) {
            // we check that some time has passed since last_window_move_or_resize_time to avoid spam saving, which could degrade perf
            // as the user moves or resizes the window

            print_debug_msg("window_pos_or_size_needs_write");
            (void) global_state::settings().save_to_disk();
            window_pos_or_size_needs_write = false;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        imgui::NewFrame();

        auto visib_at_frame_start = global_state::settings().show;

        SCOPE_EXIT {
            render(window);

            bool window_visibilities_changed = memcmp(&global_state::settings().show, &visib_at_frame_start, sizeof(visib_at_frame_start)) != 0;
            if (window_visibilities_changed) {
                global_state::settings().save_to_disk();
            }
        };

        // imgui::SetNextWindowPos(ImVec2(global_state::settings().window_x, global_state::settings().window_y), ImGuiCond_Always);
        // imgui::SetNextWindowSize(ImVec2(global_state::settings().window_w, global_state::settings().window_h), ImGuiCond_Always);

        imgui::DockSpaceOverViewport(0, ImGuiDockNodeFlags_PassthruCentralNode);

        render_main_menu_bar(explorers);

        auto &window_visib = global_state::settings().show;

        for (s32 window_code : window_render_order) {
            switch (window_code) {
                case swan_windows::explorer_0: {
                    if (window_visib.explorer_0) {
                        swan_windows::render_explorer(explorers[0], window_visib.explorer_0);
                    }
                    break;
                }
                case swan_windows::explorer_1: {
                    if (window_visib.explorer_1) {
                        swan_windows::render_explorer(explorers[1], window_visib.explorer_1);
                    }
                    break;
                }
                case swan_windows::explorer_2: {
                    if (window_visib.explorer_2) {
                        swan_windows::render_explorer(explorers[2], window_visib.explorer_2);
                    }
                    break;
                }
                case swan_windows::explorer_3: {
                    if (window_visib.explorer_3) {
                        swan_windows::render_explorer(explorers[3], window_visib.explorer_3);
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
                        render_analytics();
                    }
                    break;
                }
                case swan_windows::debug_log: {
                    if (window_visib.debug_log) {
                        swan_windows::render_debug_log(window_visib.debug_log);
                    }
                    break;
                }
                case swan_windows::settings: {
                    if (window_visib.settings) {
                        swan_windows::render_settings(window);
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
