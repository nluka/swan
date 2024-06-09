#include "stdafx.hpp"
#include "common_functions.hpp"
#include "imgui_dependent_functions.hpp"
#include "util.hpp"
#include "resource.h"

struct failed_assertion
{
    ntest::assertion ntest;
    swan_path expected_path;
    swan_path actual_path;
};

static std::vector<failed_assertion>    g_failed_assertions = {};
static std::optional<bool>              g_test_suite_ran_without_crashes = std::nullopt;

static LONG WINAPI custom_exception_handler(EXCEPTION_POINTERS *exception_info) noexcept;

static std::pair<HWND, WNDCLASSEXW> create_barebones_window(swan_settings const &settings) noexcept;

static std::tuple<bool, static_vector<ImFont*, 8>, static_vector<std::filesystem::path, 8>> load_custom_fonts() noexcept;

static void render_ntest_output_window(swan_path const &output_directory_path) noexcept;

static void run_tests_integrated(swan_path const &ntest_output_directory_path) noexcept;

static s32 run_tests_only(swan_path const &ntest_output_directory_path) noexcept;

static ID3D11Device*            g_pd3dDevice = nullptr;
static ID3D11DeviceContext*     g_pd3dDeviceContext = nullptr;
static IDXGISwapChain*          g_pSwapChain = nullptr;
static bool                     g_swapChainOccluded = false;
static UINT                     g_ResizeWidth = 0, g_ResizeHeight = 0;
static ID3D11RenderTargetView*  g_mainRenderTargetView = nullptr;

bool CreateDeviceD3D(HWND hWnd) noexcept;
void CleanupDeviceD3D() noexcept;
void CreateRenderTarget() noexcept;
void CleanupRenderTarget() noexcept;
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) noexcept;

#if RELEASE_MODE
#   pragma comment(linker, "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")
#endif
s32 APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPSTR lpCmdLine, int nCmdShow)
// s32 main([[maybe_unused]] s32 argc, char const *argv[])
try {
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
    for (u64 i = 1; i < argc; ++i) {
        if (cstr_eq(argv[i], "--tests-only")) {
            return run_tests_only(ntest_output_directory_path);
        }
    }
#endif

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

    auto [hwnd, wndclass] = create_barebones_window(global_state::settings());
    if (hwnd == NULL) {
        return 1;
    }
    global_state::window_handle() = hwnd;
    print_debug_msg("SUCCESS barebones window created");
    print_debug_msg("global_state::execution_path = [%s]", global_state::execution_path().generic_string().c_str());

    SCOPE_EXIT {
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        // implot::DestroyContext();
        imgui::DestroyContext();

        CleanupDeviceD3D();
        DestroyWindow(hwnd);
        UnregisterClassW(wndclass.lpszClassName, wndclass.hInstance);
    };

    static static_vector<ImFont*, 8> s_loaded_fonts = {};
    while (true) {
        auto [success, loaded_fonts, failed_fonts] = load_custom_fonts();
        if (success) {
            s_loaded_fonts = loaded_fonts;
            break;
        }
        std::string error = "Fonts could not be loaded, maybe files are locked or missing?\n\n";
        for (auto const &font : failed_fonts) {
            error += font.generic_string() += '\n';
        }
        s32 clicked = MessageBoxA(NULL, error.c_str(), "Failed to load fonts", MB_RETRYCANCEL);
        if (clicked == IDCANCEL) return 1;
    }
    print_debug_msg("SUCCESS custom fonts loaded");

    while (true) {
        auto [success, what_failed, attempts_made] = init_explorer_COM_Win32_DX11();
        if (success) {
            break;
        }
        s32 clicked = MessageBoxA(NULL, what_failed, "COM Initialization Error", MB_RETRYCANCEL);
        if (clicked == IDCANCEL) return 1;
        assert(clicked == IDRETRY);
    }
    SCOPE_EXIT { cleanup_explorer_COM(); };
    print_debug_msg("SUCCESS COM initialized");

    {
    #if DEBUG_MODE
        run_tests_integrated(ntest_output_directory_path);
    #else
        for (u64 i = 1; i < argc; ++i) {
            if (cstr_eq(argv[i], "--with-tests")) {
                run_tests_integrated(ntest_output_directory_path);
                break;
            }
        }
    #endif
    }

    ImGuiStyle const our_default_imgui_style = swan_default_imgui_style();

    // other initialization stuff which is either: optional, cannot fail, or whose failure is considered non-fatal
    {
        HICON icon = (HICON)LoadIconA(hInstance, MAKEINTRESOURCEA(MAIN_ICON));
        if (icon) {
            SendMessageA(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)icon);
            SendMessageA(hwnd, WM_SETICON, ICON_BIG, (LPARAM)icon);
        } else {
            print_debug_msg("FAILED LoadImageA(MAIN_ICON): %s", get_last_winapi_error().formatted_message.c_str());
        }

        imgui::GetStyle() = our_default_imgui_style;

        seed_fast_rand((u64)get_time_precise().time_since_epoch().count());

        SYSTEM_INFO system_info;
        GetSystemInfo(&system_info);
        global_state::page_size() = system_info.dwPageSize;
        print_debug_msg("global_state::page_size = %d", global_state::page_size());

        (void) global_state::settings().load_from_disk();

        s32 pos_x = 25, pos_y = 25, width = 1280, height = 720;
        if (global_state::settings().startup_with_previous_window_pos_and_size) {
            pos_x = global_state::settings().window_x;
            pos_y = global_state::settings().window_y;
            width = global_state::settings().window_w;
            height = global_state::settings().window_h;
        }
        SetWindowPos(hwnd, HWND_TOP, pos_x, pos_y, width, height, SWP_SHOWWINDOW);
        ShowWindow(hwnd, global_state::settings().startup_with_window_maximized ? SW_MAXIMIZE : nCmdShow);

        (void) global_state::pinned_load_from_disk(global_state::settings().dir_separator_utf8);
        (void) global_state::recent_files_load_from_disk(global_state::settings().dir_separator_utf8);
        (void) global_state::completed_file_operations_load_from_disk(global_state::settings().dir_separator_utf8);
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
            print_debug_msg("[ %d ] explorer_window::load_from_disk: %d", i, load_result);

            if (!load_result) {
                expl.cwd = path_create("");
                bool save_result = explorers[i].save_to_disk();
                print_debug_msg("[%s] save_to_disk: %d", expl.name, save_result);
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

    std::array<swan_windows::id, (u64)swan_windows::id::count - 1> window_render_order = {
        swan_windows::id::explorer_0,
        swan_windows::id::explorer_1,
        swan_windows::id::explorer_2,
        swan_windows::id::explorer_3,
        swan_windows::id::finder,
        swan_windows::id::pinned,
        swan_windows::id::file_operations,
        swan_windows::id::recent_files,
        swan_windows::id::ntfs_mft_reader,
        swan_windows::id::analytics,
        swan_windows::id::debug_log,
        swan_windows::id::settings,
        swan_windows::id::theme_editor,
        swan_windows::id::icon_library,
        swan_windows::id::icon_font_browser_font_awesome,
        swan_windows::id::icon_font_browser_codicon,
        swan_windows::id::icon_font_browser_material_design,
        swan_windows::id::imgui_demo,
    };

    for ([[maybe_unused]] auto const &window_id : window_render_order) {
        assert(window_id != swan_windows::id::nil_window && "Forgot to add window id to initializer list of `window_render_order`");
    }

    {
        swan_windows::id last_focused_window_id;

        if (!global_state::focused_window_load_from_disk(last_focused_window_id)) {
            last_focused_window_id = swan_windows::id::explorer_0;
        }
        else {
            assert(last_focused_window_id != swan_windows::id::nil_window);
            auto last_focused_window_iter = std::find(window_render_order.begin(), window_render_order.end(), last_focused_window_id);
            std::swap(*last_focused_window_iter, window_render_order.back());
        }
    }

    std::string const ini_file_path = (global_state::execution_path() / "data\\swan_imgui.ini").generic_string();

    static time_point_precise_t last_window_move_or_resize_time = get_time_precise();
    static bool window_pos_or_size_needs_write = false;

    print_debug_msg("Entering render loop...");

    while (true) {
        // check before polling events or starting the frame because
        // ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopup) unexpectedly returns false after ImGuiKey_Escape is pressed if
        // this value is queried later in the frame
        bool any_popups_open = imgui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopup);

        MSG msg;
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT) return 0;
        }

        // Handle window being minimized or screen locked
        if (g_swapChainOccluded && g_pSwapChain->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED) {
            Sleep(10);
            continue;
        }
        g_swapChainOccluded = false;

        // Handle window resize (we don't resize directly in the WM_SIZE handler)
        if (g_ResizeWidth != 0 && g_ResizeHeight != 0) {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, g_ResizeWidth, g_ResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
            g_ResizeWidth = g_ResizeHeight = 0;
            CreateRenderTarget();
        }

        BeginFrame_Win32_DX11(ini_file_path.c_str());

        static swan_windows::id last_focused_window_id = window_render_order.back();
        {
            swan_windows::id focused_now_id = global_state::focused_window_get();
            assert(focused_now_id != swan_windows::id::nil_window);

            if (last_focused_window_id != focused_now_id) {
                auto focused_now_iter = std::find(window_render_order.begin(), window_render_order.end(), last_focused_window_id);
                std::swap(*focused_now_iter, window_render_order.back());
            }

            if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) {
                // prevent imgui nav focus
                ImGui::SetWindowFocus(swan_windows::get_name(focused_now_id));
            }
        }

        if (window_pos_or_size_needs_write && time_diff_ms(last_window_move_or_resize_time, get_time_precise()) > 250) {
            // we check that some time has passed since last_window_move_or_resize_time to avoid spamming the disk as the user moves or resizes the window
            print_debug_msg("window_pos_or_size_needs_write");
            (void) global_state::settings().save_to_disk();
            window_pos_or_size_needs_write = false;
        }

        auto visib_at_frame_start = global_state::settings().show;

        SCOPE_EXIT {
            bool window_visibilities_changed = memcmp(&global_state::settings().show, &visib_at_frame_start, sizeof(visib_at_frame_start)) != 0;
            if (window_visibilities_changed) {
                global_state::settings().save_to_disk();
            }
        };

        imgui::DockSpaceOverViewport(0, ImGuiDockNodeFlags_PassthruCentralNode);

        render_main_menu_bar(explorers);

        auto &window_visib = global_state::settings().show;

        for (swan_windows::id window_id : window_render_order) {
            switch (window_id) {
                case swan_windows::id::explorer_0: {
                    if (window_visib.explorer_0) {
                        swan_windows::render_explorer(explorers[0], window_visib.explorer_0, finder, any_popups_open);
                    }
                    break;
                }
                case swan_windows::id::explorer_1: {
                    if (window_visib.explorer_1) {
                        swan_windows::render_explorer(explorers[1], window_visib.explorer_1, finder, any_popups_open);
                    }
                    break;
                }
                case swan_windows::id::explorer_2: {
                    if (window_visib.explorer_2) {
                        swan_windows::render_explorer(explorers[2], window_visib.explorer_2, finder, any_popups_open);
                    }
                    break;
                }
                case swan_windows::id::explorer_3: {
                    if (window_visib.explorer_3) {
                        swan_windows::render_explorer(explorers[3], window_visib.explorer_3, finder, any_popups_open);
                    }
                    break;
                }
                case swan_windows::id::finder: {
                    if (window_visib.finder) {
                        swan_windows::render_finder(finder, window_visib.finder, any_popups_open);
                    }
                    break;
                }
                case swan_windows::id::pinned: {
                    if (window_visib.pinned) {
                        swan_windows::render_pinned(explorers, window_visib.pinned, any_popups_open);
                    }
                    break;
                }
                case swan_windows::id::file_operations: {
                    if (window_visib.file_operations) {
                        swan_windows::render_file_operations(window_visib.file_operations, any_popups_open);
                    }
                    break;
                }
                case swan_windows::id::recent_files: {
                    if (window_visib.recent_files) {
                        swan_windows::render_recent_files(window_visib.recent_files, any_popups_open);
                    }
                    break;
                }
                case swan_windows::id::ntfs_mft_reader: {
                    if (window_visib.ntfs_mft_reader) {
                        swan_windows::render_ntfs_mft_reader(window_visib.ntfs_mft_reader, any_popups_open);
                    }
                    break;
                }
                case swan_windows::id::analytics: {
                    if (window_visib.analytics) {
                        swan_windows::render_analytics();
                    }
                    break;
                }
                case swan_windows::id::debug_log: {
                    if (window_visib.debug_log) {
                        swan_windows::render_debug_log(window_visib.debug_log, any_popups_open);
                    }
                    break;
                }
                case swan_windows::id::settings: {
                    if (window_visib.settings) {
                        bool changes_applied = swan_windows::render_settings(window_visib.settings, any_popups_open);
                        if (changes_applied) {
                            SetWindowPos(hwnd, HWND_TOP,
                                         global_state::settings().window_x, global_state::settings().window_y,
                                         global_state::settings().window_w, global_state::settings().window_h,
                                         SWP_SHOWWINDOW);

                            (void) global_state::settings().save_to_disk();
                        }
                    }
                    break;
                }
                case swan_windows::id::imgui_demo: {
                    if (window_visib.imgui_demo) {
                        imgui::ShowDemoWindow(&window_visib.imgui_demo);
                        if (imgui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)) {
                            global_state::focused_window_set(swan_windows::id::imgui_demo);
                        }
                        imgui::End();
                    }
                    break;
                }
                case swan_windows::id::theme_editor: {
                    if (window_visib.theme_editor) {
                        swan_windows::render_theme_editor(window_visib.theme_editor, our_default_imgui_style, any_popups_open);
                    }
                    break;
                }
                case swan_windows::id::icon_library: {
                    if (window_visib.icon_library) {
                        swan_windows::render_icon_library(window_visib.icon_library, any_popups_open);
                    }
                    break;
                }
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
        // if (imgui::IsMouseReleased(ImGuiMouseButton_Left) && !imgui::IsDragDropPayloadBeingAccepted()) {
            free_explorer_drag_drop_payload();
        }

        g_swapChainOccluded = EndFrame_Win32_DX11(g_pd3dDeviceContext, g_mainRenderTargetView, g_pSwapChain);
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
std::pair<HWND, WNDCLASSEXW> create_barebones_window(swan_settings const &settings) noexcept
{
    s32 screen_width, screen_height;

    if (global_state::settings().startup_with_previous_window_pos_and_size) {
        screen_width = settings.window_x, settings.window_y;
        screen_height = settings.window_w, settings.window_h;
    } else {
        screen_width = GetSystemMetrics(SM_CXSCREEN);
        screen_height = GetSystemMetrics(SM_CYSCREEN);
    }

    std::string window_title_utf8 = make_str("Swan - %s - Win32 + DX11 - built %s %s", get_build_mode().str, __DATE__, __TIME__);

    wchar_t window_title_utf16[256];
    utf8_to_utf16(window_title_utf8.data(), window_title_utf16, lengthof(window_title_utf16));

    WNDCLASSEXW wndclass = {
        .cbSize = sizeof(wndclass),
        .style = CS_CLASSDC,
        .lpfnWndProc = WndProc,
        .cbClsExtra = 0L,
        .cbWndExtra = 0L,
        .hInstance = GetModuleHandle(nullptr),
        .hIcon = nullptr,
        .hCursor = nullptr,
        .hbrBackground = nullptr,
        .lpszMenuName = nullptr,
        .lpszClassName = L"Swan",
        .hIconSm = nullptr
    };

    ::RegisterClassExW(&wndclass);

    HWND hwnd = ::CreateWindowW(wndclass.lpszClassName, window_title_utf16, WS_OVERLAPPEDWINDOW, 100, 100,
                                screen_width, screen_height, nullptr, nullptr, wndclass.hInstance, nullptr);

    // Initialize Direct3D
    if (!CreateDeviceD3D(hwnd)) {
        CleanupDeviceD3D();
        UnregisterClassW(wndclass.lpszClassName, wndclass.hInstance);
        return { NULL, {} };
    }

    // Show the window
    // ShowWindow(hwnd, SW_SHOWDEFAULT);
    // UpdateWindow(hwnd);

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
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    return { hwnd, wndclass };
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

static
std::tuple<bool, static_vector<ImFont*, 8>, static_vector<std::filesystem::path, 8>> load_custom_fonts() noexcept
{
    static_vector<ImFont*, 8> loaded_fonts = {};
    static_vector<std::filesystem::path, 8> failed_fonts = {};

    auto font_loaded = [](ImFont const *font) noexcept
    {
        return font && !font->IsLoaded();
    };

    auto attempt_load_font = [&](
        char const *path,
        f32 size,
        bool merge,
        bool path_rel_to_exec = true,
        ImWchar const *ranges = nullptr) noexcept
    {
        std::filesystem::path font_file_path;
        if (path_rel_to_exec) {
            font_file_path = global_state::execution_path() / path;
        } else {
            font_file_path = path;
        }

        ImFontConfig cfg = {};
        cfg.MergeMode = merge;

        auto font = imgui::GetIO().Fonts->AddFontFromFileTTF(font_file_path.string().c_str(), size, &cfg, ranges);

        if (font_loaded(font)) {
            loaded_fonts.push_back(font);
        } else {
            failed_fonts.push_back(font_file_path);
        }
    };

    attempt_load_font("fonts/RobotoMono-Regular.ttf", 17.0f, false);
    // attempt_load_font("C:/Windows/Fonts/consola.ttf", 17.0f, false, false);
    // attempt_load_font("C:/Windows/Fonts/arialuni.ttf", 20.0f, false, false);
    attempt_load_font("fonts/CascadiaMonoPL.ttf", 16.0f, true, true, imgui::GetIO().Fonts->GetGlyphRangesCyrillic());

    auto attempt_load_icon_font = [&](char const *path, f32 size, f32 offset_x, f32 offset_y, ImWchar const *s_glyph_ranges) noexcept
    {
        std::filesystem::path font_file_path = global_state::execution_path() / path;

        ImFontConfig icons_config;
        icons_config.MergeMode = true;
        icons_config.PixelSnapH = true;
        icons_config.GlyphOffset.x = offset_x;
        icons_config.GlyphOffset.y = offset_y;
        icons_config.GlyphMinAdvanceX = size;
        icons_config.GlyphMaxAdvanceX = size;

        auto font = imgui::GetIO().Fonts->AddFontFromFileTTF(font_file_path.generic_string().c_str(), size, &icons_config, s_glyph_ranges);

        if (font_loaded(font)) {
            loaded_fonts.push_back(font);
        } else {
            failed_fonts.push_back(font_file_path);
        }
    };

    // font awesome
    {
        static ImWchar const s_glyph_ranges[] = { ICON_MIN_FA, ICON_MAX_16_FA, 0 };
        attempt_load_icon_font("fonts\\" FONT_ICON_FILE_NAME_FAS, 16, 0.25f, 0, s_glyph_ranges);
    }

    // codicons
    {
        static ImWchar const s_glyph_ranges[] = { ICON_MIN_CI, ICON_MAX_16_CI, 0 };
        attempt_load_icon_font("fonts\\" FONT_ICON_FILE_NAME_CI, 18, 0, 3, s_glyph_ranges);
    }

    return {
        failed_fonts.empty(), // success
        loaded_fonts,
        failed_fonts
    };
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

bool CreateDeviceD3D(HWND hWnd) noexcept
{
    // Setup swap chain
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    //createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res == DXGI_ERROR_UNSUPPORTED) // Try high-performance WARP software driver if hardware is not available.
        res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D() noexcept
{
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

void CreateRenderTarget() noexcept
{
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget() noexcept
{
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0 // From Windows SDK 8.1+ headers
#endif

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) noexcept;

// Win32 message handler
// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
// Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) noexcept
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED)
            return 0;
        g_ResizeWidth = (UINT)LOWORD(lParam); // Queue resize
        g_ResizeHeight = (UINT)HIWORD(lParam);
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_DPICHANGED:
        if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_DpiEnableScaleViewports) {
            // s32 dpi = HIWORD(wParam);
            // printf("WM_DPICHANGED to %d (%.0f%%)\n", dpi, (float)dpi / 96.0f * 100.0f);
            RECT *suggested_rect = (RECT*)lParam;
            SetWindowPos(hWnd, nullptr, suggested_rect->left, suggested_rect->top, suggested_rect->right - suggested_rect->left, suggested_rect->bottom - suggested_rect->top, SWP_NOZORDER|SWP_NOACTIVATE);
        }
        break;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}
