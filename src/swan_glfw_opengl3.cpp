#define GLFW_EXPOSE_NATIVE_WIN32
#include "glfw3native.h"

#include "stdafx.hpp"
#include "common_functions.hpp"
#include "imgui_dependent_functions.hpp"
#include "util.hpp"

#include "binary/CascadiaMonoPL_ttf.h"
#include "binary/FontAwesome5_Solid_900_ttf.h"
#include "binary/RobotoMono_Regular_ttf.h"
#include "binary/codicon_ttf.h"
#include "binary/lucide_ttf.h"
#include "binary/swan_png.h"

struct failed_assertion
{
    ntest::assertion ntest;
    swan_path expected_path;
    swan_path actual_path;
};

static std::vector<failed_assertion>    g_failed_assertions = {};
static std::optional<bool>              g_test_suite_ran_without_crashes = std::nullopt;

static LONG WINAPI  custom_exception_handler(EXCEPTION_POINTERS *exception_info) noexcept;
static GLFWwindow * create_barebones_window() noexcept;
static void         glfw_error_callback(s32 error, char const *description) noexcept;
static void         load_custom_fonts(GLFWwindow *window, char const *ini_file_path) noexcept;
static void         set_window_icon(GLFWwindow *window) noexcept;
static void         render_ntest_output_window(swan_path const &output_directory_path) noexcept;
static void         run_tests_integrated(swan_path const &ntest_output_directory_path) noexcept;
static s32          run_tests_only(swan_path const &ntest_output_directory_path) noexcept;

s32 APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
try {
    (void) hInstance;
    (void) hPrevInstance;
    (void) lpCmdLine;
    // (void) nCmdShow;

    SetUnhandledExceptionFilter(custom_exception_handler);

    {
        char exe_path[MAX_PATH];
        GetModuleFileNameA(NULL, exe_path, MAX_PATH);

        std::filesystem::path swan_exec_path = exe_path;
        swan_exec_path = swan_exec_path.remove_filename();
        global_state::execution_path() = swan_exec_path;
    }
    swan_path ntest_output_directory_path = path_create( (global_state::execution_path() / "ntest").string().c_str() );

#if RELEASE_MODE
{
    LPWSTR *argv;
    s32 argc;
    argv = CommandLineToArgvW(GetCommandLineW(), &argc);

    std::vector<std::string> args;
    for (int i = 0; i < argc; ++i) {
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, argv[i], -1, NULL, 0, NULL, NULL);
        std::string arg(size_needed, 0);
        WideCharToMultiByte(CP_UTF8, 0, argv[i], -1, &arg[0], size_needed, NULL, NULL);
        args.push_back(arg);
    }

    for (u64 i = 1; i < argc; ++i) {
        if (cstr_eq(args[i].c_str(), "--tests-only")) {
            LocalFree(argv);
            return run_tests_only(ntest_output_directory_path);
        }
    }

    for (u64 i = 1; i < argc; ++i) {
        if (cstr_eq(args[i].c_str(), "--with-tests")) {
            run_tests_integrated(ntest_output_directory_path);
            break;
        }
    }
}
#endif

    GLFWwindow *window = create_barebones_window();
    if (window == nullptr) {
        return 1;
    }
    global_state::window_handle() = glfwGetWin32Window(window);

    if (glewInit() != GLEW_OK) {
        return 1;
    }

    // reset log file
    {
        auto log_file_path = global_state::execution_path() / "debug_log.md";
        FILE *file = fopen(log_file_path.string().c_str(), "w");
        if (file) {
            fprintf(file, "| Thread ID | System Time | ImGui Time | File | Line | Function | Message |\n");
            fprintf(file, "| --------- | ----------- | ---------- | ---- | ---- | -------- | ------- |\n");
            fclose(file);
        }
    }

    print_debug_msg("SUCCESS barebones window created");

    std::string const ini_file_path = (global_state::execution_path() / "data\\swan_imgui.ini").generic_string();

    SCOPE_EXIT {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        // implot::DestroyContext();
        imgui::DestroyContext(0, ini_file_path.c_str());
        glfwDestroyWindow(window);
        glfwTerminate();
    };

    print_debug_msg("global_state::execution_path = [%s]", global_state::execution_path().generic_string().c_str());

    // block execution until all custom fonts are loaded successfully.
    // a custom font is one not built into Dear ImGui.
    // the user is notified of any font load failures and given the ability to "Retry" which will attempt to reload the fonts.
    load_custom_fonts(window, ini_file_path.c_str());
    print_debug_msg("SUCCESS non-default fonts loaded");

    // block until COM is successfully initialized. the user is notified if an error occurs,
    // and has the ability to "Retry" which will attempt to re-initialize COM.
    init_explorer_COM_GLFW_OpenGL3(window, ini_file_path.c_str());
    print_debug_msg("SUCCESS COM initialized");
    SCOPE_EXIT { cleanup_explorer_COM(); };

#if DEBUG_MODE
    run_tests_integrated(ntest_output_directory_path);
#endif

    ImGuiStyle const our_default_imgui_style = swan_default_imgui_style();

    // other initialization stuff which is either: optional, cannot fail, or whose failure is considered non-fatal
    {
        set_window_icon(window);

        imgui::GetStyle() = our_default_imgui_style;

        seed_fast_rand((u64)get_time_precise().time_since_epoch().count());

        SYSTEM_INFO system_info;
        GetSystemInfo(&system_info);
        global_state::page_size() = system_info.dwPageSize;
        print_debug_msg("global_state::page_size = %d", global_state::page_size());

        (void) global_state::settings().load_from_disk();
        (void) global_state::pinned_load_from_disk(global_state::settings().dir_separator_utf8);
        {
            auto result = global_state::recent_files_load_from_disk(global_state::settings().dir_separator_utf8);
            if (!result.first) {
                auto recent_files = global_state::recent_files_get();
                std::scoped_lock recent_files_lock(*recent_files.mutex);
                recent_files.container->clear();
            }
        }
        {
            auto result = global_state::completed_file_operations_load_from_disk(global_state::settings().dir_separator_utf8);
            if (!result.first) {
                auto completed_file_operations = global_state::completed_file_operations_get();
                std::scoped_lock recent_files_lock(*completed_file_operations.mutex);
                completed_file_operations.container->clear();
            }
        }

        if (global_state::settings().startup_with_window_maximized) {
            glfwMaximizeWindow(window);
        }
        if (global_state::settings().startup_with_previous_window_pos_and_size) {
            glfwSetWindowPos(window, global_state::settings().window_x, global_state::settings().window_y);
            glfwSetWindowSize(window, global_state::settings().window_w, global_state::settings().window_h);
        }
    }

    auto &explorers = global_state::explorers();
    // init explorers
    {
        char const *names[global_constants::num_explorers] = {
            swan_windows::get_name(swan_windows::id::explorer_0),
            swan_windows::get_name(swan_windows::id::explorer_1),
            swan_windows::get_name(swan_windows::id::explorer_2),
            swan_windows::get_name(swan_windows::id::explorer_3),
        };

        for (u64 i = 0; i < explorers.size(); ++i) {
            auto &expl = explorers[i];

            expl.id = s32(i);
            expl.name = names[i];
            expl.filter_error.reserve(1024);

            bool load_result = explorers[i].load_from_disk(global_state::settings().dir_separator_utf8);

            if (!load_result) {
                expl.cwd = path_create("");
                (void) explorers[i].save_to_disk();
            }

            auto [starting_dir_exists, _] = expl.update_cwd_entries(query_filesystem, expl.cwd.data());
            if (starting_dir_exists) {
                expl.set_latest_valid_cwd(expl.cwd); // this may mutate filter
                (void) expl.update_cwd_entries(filter, expl.cwd.data());
            }
        }
    }

    finder_window finder = {
        .search_directories = { { false, path_create("") } }
    };

    // last elem is the last window to be rendered, the most forward window
    std::array<swan_windows::id, (u64)swan_windows::id::count - 1> window_render_order = window_render_order_load_from_disk();

    for ([[maybe_unused]] auto const &window_id : window_render_order) {
        assert(window_id != swan_windows::id::nil_window && "Forgot to add window id to initializer list of `window_render_order`");
    }

    static time_point_precise_t last_window_move_or_resize_time = get_time_precise();
    static bool window_pos_or_size_needs_write = false;

    glfwSetWindowPosCallback(window, [](GLFWwindow *, s32 new_x, s32 new_y) noexcept {
        global_state::settings().window_x = new_x;
        global_state::settings().window_y = new_y;
        last_window_move_or_resize_time = get_time_precise();
        window_pos_or_size_needs_write = true;
    });
    glfwSetWindowSizeCallback(window, [](GLFWwindow *, s32 new_w, s32 new_h) noexcept {
        global_state::settings().window_w = new_w;
        global_state::settings().window_h = new_h;
        last_window_move_or_resize_time = get_time_precise();
        window_pos_or_size_needs_write = true;
    });

    print_debug_msg("Entering render loop...");

    while (!glfwWindowShouldClose(window)) {
        // check before polling events or starting the frame because
        // ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopup) unexpectedly returns false after ImGuiKey_Escape is pressed if
        // this value is queried later in the frame
        bool any_popups_open = imgui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopup);

        glfwPollEvents();

        // this is to prevent the ugly blue border (nav focus I think it's called?) when pressing escape
        if (one_of(GLFW_PRESS, { glfwGetKey(window, GLFW_KEY_ESCAPE) })) {
            ImGui::SetWindowFocus(swan_windows::get_name(window_render_order.back()));
        }

        if (window_pos_or_size_needs_write && time_diff_ms(last_window_move_or_resize_time, get_time_precise()) > 250) {
            // we check that some time has passed since last_window_move_or_resize_time to avoid spamming the disk as the user moves or resizes the window
            print_debug_msg("window_pos_or_size_needs_write");
            (void) global_state::settings().save_to_disk();
            window_pos_or_size_needs_write = false;
        }

        BeginFrame_GLFW_OpenGL3(ini_file_path.c_str());

        auto visib_at_frame_start = global_state::settings().show;

        SCOPE_EXIT {
            EndFrame_GLFW_OpenGL3(window);

            bool window_visibilities_changed = memcmp(&global_state::settings().show, &visib_at_frame_start, sizeof(visib_at_frame_start)) != 0;
            if (window_visibilities_changed) {
                global_state::settings().save_to_disk();
            }
        };

        imgui::DockSpaceOverViewport(0, ImGuiDockNodeFlags_PassthruCentralNode);

        render_main_menu_bar(window, explorers);

        auto &window_visib = global_state::settings().show;

        bool expl_rendered[global_constants::num_explorers] = {};

        auto window_render_order_2 = window_render_order; // make a copy because we cannot update the original whilst also iterating it

        auto window_render_order_move_to_back = [&window_render_order_2](swan_windows::id window_id) noexcept {
            if (window_render_order_2.back() != window_id) {
                auto iter = std::find(window_render_order_2.begin(), window_render_order_2.end(), window_id);
                std::rotate(iter, iter + 1, window_render_order_2.end());
            }
        };

        for (swan_windows::id window_id : window_render_order) {
            switch (window_id) {
                case swan_windows::id::explorer_0: {
                    if (window_visib.explorer_0) {
                        expl_rendered[0] = swan_windows::render_explorer(explorers[0], window_visib.explorer_0, finder, any_popups_open);

                        if (expl_rendered[0] && imgui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) && imgui::GetFrameCount() > 1) {
                            window_render_order_move_to_back(swan_windows::id::explorer_0);
                        }
                        imgui::End();
                    }
                    break;
                }
                case swan_windows::id::explorer_1: {
                    if (window_visib.explorer_1) {
                        expl_rendered[1] = swan_windows::render_explorer(explorers[1], window_visib.explorer_1, finder, any_popups_open);

                        if (expl_rendered[1] && imgui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) && imgui::GetFrameCount() > 1) {
                            window_render_order_move_to_back(swan_windows::id::explorer_1);
                        }
                        imgui::End();
                    }
                    break;
                }
                case swan_windows::id::explorer_2: {
                    if (window_visib.explorer_2) {
                        expl_rendered[2] = swan_windows::render_explorer(explorers[2], window_visib.explorer_2, finder, any_popups_open);

                        if (expl_rendered[2] && imgui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) && imgui::GetFrameCount() > 1) {
                            window_render_order_move_to_back(swan_windows::id::explorer_2);
                        }
                        imgui::End();
                    }
                    break;
                }
                case swan_windows::id::explorer_3: {
                    if (window_visib.explorer_3) {
                        expl_rendered[3] = swan_windows::render_explorer(explorers[3], window_visib.explorer_3, finder, any_popups_open);

                        if (expl_rendered[3] && imgui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) && imgui::GetFrameCount() > 1) {
                            window_render_order_move_to_back(swan_windows::id::explorer_3);
                        }
                        imgui::End();
                    }
                    break;
                }
                case swan_windows::id::explorer_0_debug: {
                    if (window_visib.explorer_0_debug && expl_rendered[0]) {
                        if (swan_windows::render_explorer_debug(explorers[0], window_visib.explorer_0_debug, any_popups_open)) {
                            if (imgui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) && imgui::GetFrameCount() > 1) {
                                window_render_order_move_to_back(swan_windows::id::explorer_0_debug);
                            }
                        }
                        imgui::End();
                    }
                    break;
                }
                case swan_windows::id::explorer_1_debug: {
                    if (window_visib.explorer_1_debug && expl_rendered[1]) {
                        if (swan_windows::render_explorer_debug(explorers[1], window_visib.explorer_1_debug, any_popups_open)) {
                            if (imgui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) && imgui::GetFrameCount() > 1) {
                                window_render_order_move_to_back(swan_windows::id::explorer_1_debug);
                            }
                        }
                        imgui::End();
                    }
                    break;
                }
                case swan_windows::id::explorer_2_debug: {
                    if (window_visib.explorer_2_debug && expl_rendered[2]) {
                        if (swan_windows::render_explorer_debug(explorers[2], window_visib.explorer_2_debug, any_popups_open)) {
                            if (imgui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) && imgui::GetFrameCount() > 1) {
                                window_render_order_move_to_back(swan_windows::id::explorer_2_debug);
                            }
                        }
                        imgui::End();
                    }
                    break;
                }
                case swan_windows::id::explorer_3_debug: {
                    if (window_visib.explorer_3_debug && expl_rendered[3]) {
                        if (swan_windows::render_explorer_debug(explorers[3], window_visib.explorer_3_debug, any_popups_open)) {
                            if (imgui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) && imgui::GetFrameCount() > 1) {
                                window_render_order_move_to_back(swan_windows::id::explorer_3_debug);
                            }
                        }
                        imgui::End();
                    }
                    break;
                }
                case swan_windows::id::finder: {
                    if (window_visib.finder) {
                        if (swan_windows::render_finder(finder, window_visib.finder, any_popups_open)) {
                            if (imgui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) && imgui::GetFrameCount() > 1) {
                                window_render_order_move_to_back(swan_windows::id::finder);
                            }
                        }
                        imgui::End();
                    }
                    break;
                }
                case swan_windows::id::pinned: {
                    if (window_visib.pinned) {
                        if (swan_windows::render_pinned(explorers, window_visib.pinned, any_popups_open)) {
                            if (imgui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) && imgui::GetFrameCount() > 1) {
                                window_render_order_move_to_back(swan_windows::id::pinned);
                            }
                        }
                        imgui::End();
                    }
                    break;
                }
                case swan_windows::id::file_operations: {
                    if (window_visib.file_operations) {
                        if (swan_windows::render_file_operations(window_visib.file_operations, any_popups_open)) {
                            if (imgui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) && imgui::GetFrameCount() > 1) {
                                window_render_order_move_to_back(swan_windows::id::file_operations);
                            }
                        }
                        imgui::End();
                    }
                    break;
                }
                case swan_windows::id::recent_files: {
                    if (window_visib.recent_files) {
                        if (swan_windows::render_recent_files(window_visib.recent_files, any_popups_open)) {
                            if (imgui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) && imgui::GetFrameCount() > 1) {
                                window_render_order_move_to_back(swan_windows::id::recent_files);
                            }
                        }
                        imgui::End();
                    }
                    break;
                }
                case swan_windows::id::analytics: {
                    if (window_visib.analytics) {
                        if (swan_windows::render_analytics(window_render_order)) {
                            if (imgui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) && imgui::GetFrameCount() > 1) {
                                window_render_order_move_to_back(swan_windows::id::analytics);
                            }
                        }
                        imgui::End();
                    }
                    break;
                }
                case swan_windows::id::debug_log: {
                    if (window_visib.debug_log) {
                        swan_windows::render_debug_log(window_visib.debug_log, any_popups_open);
                        if (imgui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) && imgui::GetFrameCount() > 1) {
                            window_render_order_move_to_back(swan_windows::id::debug_log);
                        }
                        imgui::End();
                    }
                    break;
                }
                case swan_windows::id::settings: {
                    if (window_visib.settings) {
                        bool changes_applied = false;

                        if (swan_windows::render_settings(window_visib.settings, any_popups_open, changes_applied)) {
                            if (imgui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) && imgui::GetFrameCount() > 1) {
                                window_render_order_move_to_back(swan_windows::id::settings);
                            }
                            imgui::End();

                            if (changes_applied) {
                                glfwSetWindowPos(window, global_state::settings().window_x, global_state::settings().window_y);
                                glfwSetWindowSize(window, global_state::settings().window_w, global_state::settings().window_h);
                                (void) global_state::settings().save_to_disk();
                            }
                        }
                    }
                    break;
                }
                case swan_windows::id::imgui_demo: {
                    if (window_visib.imgui_demo) {
                        imgui::ShowDemoWindow(&window_visib.imgui_demo);
                        if (imgui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) && imgui::GetFrameCount() > 1) {
                            window_render_order_move_to_back(swan_windows::id::imgui_demo);
                        }
                        imgui::End();
                    }
                    break;
                }
                case swan_windows::id::theme_editor: {
                    if (window_visib.theme_editor) {
                        if (swan_windows::render_theme_editor(window_visib.theme_editor, our_default_imgui_style, any_popups_open)) {
                            if (imgui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) && imgui::GetFrameCount() > 1) {
                                window_render_order_move_to_back(swan_windows::id::theme_editor);
                            }
                        }
                        imgui::End();
                    }
                    break;
                }
                case swan_windows::id::icon_library: {
                    if (window_visib.icon_library) {
                        if (swan_windows::render_icon_library(window_visib.icon_library, any_popups_open)) {
                            if (imgui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) && imgui::GetFrameCount() > 1) {
                                window_render_order_move_to_back(swan_windows::id::icon_library);
                            }
                        }
                        imgui::End();
                    }
                    break;
                }
                default: break;
            }
        }

        if (memcmp(window_render_order_2.data(), window_render_order.data(), sizeof(window_render_order.front()) * window_render_order.size()) != 0) {
            // there was a change in window focus, window_render_order_2 reflects this change
            window_render_order_save_to_disk(window_render_order_2);
            window_render_order = window_render_order_2;
        }
        if (imgui::GetFrameCount() == 1) {
            // After rendering all windows for the first time (FrameCount == 1),
            // tell imgui which window will have initial focus based on what was loaded from [focused_window.txt].
            // Notice that we check for imgui::GetFrameCount() > 1 before calling window_render_order_save_to_disk(),
            // this is because imgui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) returns true for all windows on frame 1.

            for (auto const &window_id : window_render_order) {
                char const *window_name = swan_windows::get_name(window_id);
                imgui::SetWindowFocus(window_name);
            }
        }

        swan_popup_modals::render_single_rename();
        swan_popup_modals::render_bulk_rename();
        swan_popup_modals::render_new_file();
        swan_popup_modals::render_new_directory();
        swan_popup_modals::render_new_pin();
        swan_popup_modals::render_edit_pin();
        swan_popup_modals::render_error();

        imgui::RenderConfirmationModal();

        if (!g_test_suite_ran_without_crashes.value_or(true) || g_failed_assertions.size() > 0) {
            imgui::OpenPopup(" Test Output ");
            render_ntest_output_window(ntest_output_directory_path);
        }

        // free memory if explorer payload was not accepted and the user dropped it
        if (!imgui::IsMouseDragging(ImGuiMouseButton_Left) && global_state::move_dirents_payload_set() == true) {
            free_explorer_drag_drop_payload();
        }
    }

    return 0;
}
catch (std::exception const &except) {
    fprintf(stderr, "fatal: %s\n", except.what());
    return 1;
}
catch (std::string const &err) {
    fprintf(stderr, "fatal: %s\n", err.c_str());
    return 1;
}
catch (char const *err) {
    fprintf(stderr, "fatal: %s\n", err);
    return 1;
}
catch (...) {
    fprintf(stderr, "fatal: unknown error, catch(...)\n");
    return 1;
}

static
void glfw_error_callback(s32 error, char const *description) noexcept
{
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

static
GLFWwindow *create_barebones_window() noexcept
{
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) {
        return nullptr;
    }

    // GL 3.0 + GLSL 130
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    // glfwWindowHint(GLFW_HOVERED, GLFW_TRUE);
    // glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    // glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only

    GLFWwindow *window = nullptr;
    // Create window with graphics context
    {
        s32 screen_width = GetSystemMetrics(SM_CXSCREEN);
        s32 screen_height = GetSystemMetrics(SM_CYSCREEN);

        std::string window_title = make_str("swan - %s - GLFW + OpenGL3 - built %s %s", get_build_mode().str, __DATE__, __TIME__);

        window = glfwCreateWindow(screen_width, screen_height, window_title.c_str(), nullptr, nullptr);
        if (window == nullptr) {
            return nullptr;
        }
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // vsync

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    imgui::CreateContext();
    // implot::CreateContext();
    ImGuiIO &io = imgui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigDockingWithShift = true;
    imgui::SetNextWindowSizeConstraints(io.DisplaySize, io.DisplaySize);

    // Setup Platform/Renderer backends
    if (!ImGui_ImplGlfw_InitForOpenGL(window, true)) {
        return nullptr;
    }
    if (!ImGui_ImplOpenGL3_Init("#version 130")) {
        return nullptr;
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

    std::filesystem::path icon_file_path = global_state::execution_path() / "swan.png";
    std::string icon_file_path_str = icon_file_path.generic_string();

    s32 icon_channels;
    icon.pixels = stbi_load_from_memory(swan_png, (s32)lengthof(swan_png), &icon.width, &icon.height, &icon_channels, STBI_rgb_alpha);
    SCOPE_EXIT { stbi_image_free(icon.pixels); };

    if (icon.pixels) {
        glfwSetWindowIcon(window, 1, &icon);
    } else {
        print_debug_msg("FAILED to set window icon [%s]", icon_file_path_str.c_str());
    }
}

// SEH handler function
LONG WINAPI custom_exception_handler(EXCEPTION_POINTERS *exception_info) noexcept
{
    _tprintf(_T("Unhandled exception. Generating crash dump...\n"));

    // Create a crash dump file
    HANDLE dump_file = CreateFile(
        _T("swan_crash.dmp"),
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
        auto last_error = get_last_winapi_error();
        _tprintf(_T("Error creating crash dump file: %d - %s\n"), last_error.code, last_error.formatted_message.c_str());
    }

    // Allow the default exception handling to continue (e.g., generate an error report)
    return EXCEPTION_CONTINUE_SEARCH;
}

#if 0

#endif

void load_custom_fonts(GLFWwindow *window, char const *ini_file_path) noexcept
{
    auto font_loaded = [](ImFont const *font) noexcept { return font && !font->IsLoaded(); };

    bool retry = true; // start true to do initial load
    std::vector<std::filesystem::path> failed_fonts = {};

    while (true) {
        if (retry) {
            retry = false;
            failed_fonts = {};

            auto attempt_load_font = [&](char const *font_name, unsigned char const *ttf_data_static, u64 ttf_data_len,
                                         f32 size_pixels, bool merge, ImWchar const *ranges = nullptr) noexcept
            {
                ImFontConfig cfg = {};
                cfg.MergeMode = merge;

                // create a malloc'd copy, because ImGui will call free on the TTF data pointer in imgui::DestroyContext
                auto ttf_data_alloc = (unsigned char *) malloc(ttf_data_len);
                memcpy(ttf_data_alloc, ttf_data_static, ttf_data_len);

                auto font = imgui::GetIO().Fonts->AddFontFromMemoryTTF(ttf_data_alloc, (s32)ttf_data_len, size_pixels, &cfg, ranges);
                if (!font_loaded(font)) failed_fonts.push_back(font_name);
            };

            auto attempt_load_icon_font = [&](char const *font_name, unsigned char const *ttf_data_static, u64 ttf_data_len,
                                              f32 size, ImVec2 offset, ImWchar const *s_glyph_ranges) noexcept
            {
                ImFontConfig icons_config = {};
                icons_config.MergeMode = icons_config.PixelSnapH = true;
                icons_config.GlyphOffset = offset;
                icons_config.GlyphMinAdvanceX = icons_config.GlyphMaxAdvanceX = size;

                // create a malloc'd copy, because ImGui will call free on the TTF data pointer in imgui::DestroyContext
                auto ttf_data_alloc = (unsigned char *) malloc(ttf_data_len);
                memcpy(ttf_data_alloc, ttf_data_static, ttf_data_len);

                auto font = imgui::GetIO().Fonts->AddFontFromMemoryTTF(ttf_data_alloc, (s32)ttf_data_len, size, &icons_config, s_glyph_ranges);
                if (!font_loaded(font)) failed_fonts.push_back(font_name);
            };

            attempt_load_font("RobotoMono_Regular", RobotoMono_Regular_ttf, lengthof(RobotoMono_Regular_ttf), 16.0f, false);
            attempt_load_font("CascadiaMonoPL", CascadiaMonoPL_ttf, lengthof(CascadiaMonoPL_ttf), 16.0f, true, imgui::GetIO().Fonts->GetGlyphRangesCyrillic());
            {
                static ImWchar const s_glyph_ranges[] = { ICON_MIN_FA, ICON_MAX_16_FA, 0 };
                attempt_load_icon_font("FontAwesome5_Solid_900", FontAwesome5_Solid_900_ttf, lengthof(FontAwesome5_Solid_900_ttf), 16, ImVec2(0.25f, 0), s_glyph_ranges);
            }
            {
                static ImWchar const s_glyph_ranges[] = { ICON_MIN_CI, ICON_MAX_16_CI, 0 };
                attempt_load_icon_font("codicon", codicon_ttf, lengthof(codicon_ttf), 16, ImVec2(0, 3), s_glyph_ranges);
            }
            {
                static ImWchar const s_glyph_ranges[] = { ICON_MIN_LC, ICON_MAX_16_LC, 0 };
                attempt_load_icon_font("lucide", lucide_ttf, lengthof(lucide_ttf), 16, ImVec2(0, 3), s_glyph_ranges);
            }
        }

        if (failed_fonts.empty()) {
            break;
        }

        BeginFrame_GLFW_OpenGL3(ini_file_path);

        if (imgui::Begin("Startup Error", nullptr, ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_AlwaysAutoResize)) {
            imgui::TextColored(error_color(), "Application is unable to continue, font(s) failed to load:");
            imgui::Spacing();
            for (auto const &font : failed_fonts) {
                imgui::TextUnformatted(font.generic_string().c_str());
            }
            imgui::Spacing();
            retry = imgui::Button("Retry");
        }
        imgui::End();

        EndFrame_GLFW_OpenGL3(window);
    }
}

static
void render_ntest_output_window([[maybe_unused]] swan_path const &output_directory_path) noexcept
{
    // static bool s_open = false;

    if (imgui::BeginPopupModal(" Test Output ", nullptr, ImGuiWindowFlags_NoTitleBar)) {
        if (imgui::IsWindowFocused() && imgui::IsKeyPressed(ImGuiKey_Escape)) {
            g_test_suite_ran_without_crashes = true;
            g_failed_assertions.clear();
            imgui::CloseCurrentPopup();
        }

        imgui::TextColored(error_color(), "%zu assertions failed.", g_failed_assertions.size());
        imgui::SameLine();
        imgui::TextDisabled("Double click Line to open in VSCode.");

        imgui::Separator();

        if (imgui::BeginChild("## ntest_output_window child")) {
            ImGuiTableFlags table_flags =
                ImGuiTableFlags_SizingStretchProp|
                ImGuiTableFlags_Resizable|
                ImGuiTableFlags_BordersV|
                ImGuiTableFlags_ScrollY
            ;

            if (imgui::BeginTable("## ntest_output_window table", 4, table_flags)) {
                imgui::TableSetupColumn("Line");
                imgui::TableSetupColumn("Diff Exp & Act");
                imgui::TableSetupColumn("Expected");
                imgui::TableSetupColumn("Actual");
                imgui::TableSetupScrollFreeze(0, 1);
                imgui::TableHeadersRow();

                ImGuiListClipper clipper;
                clipper.Begin((s32)g_failed_assertions.size());

                while (clipper.Step())
                for (s32 i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                    auto const &a = g_failed_assertions[i];
                    auto serialized = a.ntest.extract_serialized_values(false);

                    imgui::TableNextColumn();
                    {
                        auto label = make_str_static<256>("%zu ## Line of elem %zu", a.ntest.loc.line(), i);

                        imgui::Selectable(label.data(), false, ImGuiSelectableFlags_AllowDoubleClick);
                        if (imgui::IsItemHovered() && imgui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                            auto full_command = make_str_static<512>("code --goto %s:%zu", a.ntest.loc.file_name(), a.ntest.loc.line());
                            system(full_command.data());
                        }
                    }

                    imgui::TableNextColumn();
                    if (path_is_empty(a.expected_path) || path_is_empty(a.actual_path)) {
                        imgui::TextDisabled("N/A");
                    } else {
                        auto label = make_str_static<256>("Open" "## %zu", i);

                        if (imgui::SmallButton(label.data())) {
                            auto full_command = make_str_static<1024>("code --diff %s %s", a.expected_path.data(), a.actual_path.data());
                            system(full_command.data());
                        }
                    }

                    imgui::TableNextColumn();
                    if (path_is_empty(a.expected_path)) {
                        imgui::TextUnformatted(serialized.expected);
                    } else {
                        char const *file_name = path_cfind_filename(a.expected_path.data());
                        auto label = make_str_static<256>("%s ## %zu", file_name, i);

                        imgui::Selectable(label.data(), false, ImGuiSelectableFlags_AllowDoubleClick);
                        if (imgui::IsItemHovered() && imgui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                            auto full_command = make_str_static<512>("code %s", a.expected_path.data());
                            system(full_command.data());
                        }
                    }

                    imgui::TableNextColumn();
                    if (path_is_empty(a.actual_path)) {
                        imgui::TextUnformatted(serialized.actual);
                    } else {
                        char const *file_name = path_cfind_filename(a.actual_path.data());
                        auto label = make_str_static<256>("%s ## %zu", file_name, i);

                        imgui::Selectable(label.data(), false, ImGuiSelectableFlags_AllowDoubleClick);
                        if (imgui::IsItemHovered() && imgui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                            auto full_command = make_str_static<512>("code %s", a.actual_path.data());
                            system(full_command.data());
                        }
                    }
                }

                imgui::EndTable();
            }
        }
        imgui::EndChild();

        imgui::EndPopup();
    }
}

static
void run_tests_integrated(swan_path const &ntest_output_directory_path) noexcept
{
    auto assertion_callback = [](ntest::assertion const &a, bool passed) noexcept {
        if (!passed) {
            failed_assertion a2 = { a, {}, {}, };
            g_failed_assertions.push_back(a2);
        }
    };

    auto result = run_tests(global_state::execution_path() / "ntest", assertion_callback);

    g_test_suite_ran_without_crashes = result.has_value();

    for (auto &a : g_failed_assertions) {
        auto serialized = a.ntest.extract_serialized_values(false);

        if (serialized.expected[0] == '[') {
            auto expected_file_name = make_str_static<128>("%s", serialized.expected + 1); // skip [
            strtok(expected_file_name.data(), "]");
            a.expected_path = ntest_output_directory_path;
            (void) path_append(a.expected_path, expected_file_name.data(), '\\', true);
        }
        if (serialized.expected[0] == '[') {
            auto actual_file_name = make_str_static<128>("%s", serialized.actual + 1); // skip [
            strtok(actual_file_name.data(), "]");
            a.actual_path = ntest_output_directory_path;
            (void) path_append(a.actual_path, actual_file_name.data(), '\\', true);
        }
    }
}

#if RELEASE_MODE
static
s32 run_tests_only([[maybe_unused]] swan_path const &ntest_output_directory_path) noexcept
{
    auto result = run_tests(global_state::execution_path() / "ntest", nullptr);

    if (!result.has_value()) {
        return -1;
    }

    return s32(result.value().num_fails);
}
#endif
