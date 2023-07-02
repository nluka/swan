#define _CRT_SECURE_NO_WARNINGS
#include <cstdio>
#include <string>
#include <cstring>
#include <memory>
#include <iostream>
#include <vector>
#include <array>
#include <algorithm>

#if 0
    // [Win32] Our example includes a copy of glfw3.lib pre-compiled with VS2010 to maximize ease of testing and compatibility with old VS compilers.
    // To link with VS2010-era libraries, VS2015+ requires linking with legacy_stdio_definitions.lib, which we do using this pragma.
    // Your own project should not be affected, as you are likely to link with a newer binary of GLFW that is adequate for your version of Visual Studio.
    #if defined(_MSC_VER) && (_MSC_VER >= 1900) && !defined(IMGUI_DISABLE_WIN32_FUNCTIONS)
    #pragma comment(lib, "legacy_stdio_definitions")
    #endif
#endif

#include <Windows.h>
#include "Shlwapi.h"

#pragma warning(push)
#pragma warning(disable: 4244)
#define STB_IMAGE_IMPLEMENTATION
#include "stbi_image.h"
#pragma warning(pop)

#define GL_SILENCE_DEPRECATION
#include <glfw3.h> // Will drag system OpenGL headers

#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"

#include "on_scope_exit.hpp"
#include "primitives.hpp"

#include "util.cpp"

void debug_log([[maybe_unused]] char const *fmt, ...)
{
#if !defined(NDEBUG)
    va_list args;
    va_start(args, fmt);

    IM_ASSERT(vprintf(fmt, args) > 0);

    va_end(args);

    (void) putc('\n', stdout);
#endif
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
        window = glfwCreateWindow(screenWidth, screenHeight, "nexplorer", nullptr, nullptr);
        if (window == nullptr) {
            return nullptr;
        }
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

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
i32 directory_exists(char const *path)
{
    DWORD attributes = GetFileAttributesA(path);
    return (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY));
}

typedef std::array<char, MAX_PATH> path_t;

enum class path_append_result : i32
{
    success = 0,
    nil,
    exceeds_max_path,
};

path_append_result path_append(path_t &path, char const *str)
{
    u64 path_len = strlen(path.data());
    u64 str_len = strlen(str);
    u64 final_len_without_nul = path_len + str_len;

    if (final_len_without_nul > MAX_PATH) {
        return path_append_result::exceeds_max_path;
    }

    (void) strncat(path.data(), str, str_len);

    return path_append_result::success;
}

bool path_ends_with(path_t const &path, char const *chars)
{
    u64 len = strlen(path.data());
    char last_ch = path[len - 1];
    return strchr(chars, last_ch);
}

struct directory_entry {
    bool is_directory = 0;
    bool is_selected = 0;
    path_t path = {};
    u64 size = 0;
};

static std::vector<directory_entry> s_dir_entries = {};
static path_t s_working_dir = {};
static u64 s_num_file_searches = 0;
static u64 s_last_selected_dirent_idx = 0;
static std::array<char, 1024> s_filter = {};

static
void update_dir_entries(std::string_view parent_dir)
{
    s_dir_entries.clear();

    WIN32_FIND_DATAA find_data;

    while (parent_dir.ends_with(' ')) {
        parent_dir = std::string_view(parent_dir.data(), parent_dir.size() - 1);
    }

    if (!directory_exists(parent_dir.data())) {
        debug_log("directory [%s] doesn't exist", parent_dir.data());
        return;
    }

    static std::string search_path{};
    search_path.reserve(parent_dir.size() + strlen("\\*"));
    search_path = parent_dir;
    search_path += "\\*";

    debug_log("search_path = [%s]", search_path.c_str());

    HANDLE find_handle = FindFirstFileA(search_path.data(), &find_data);
    auto find_handle_cleanup_routine = make_on_scope_exit([&find_handle] { FindClose(find_handle); });

    if (find_handle == INVALID_HANDLE_VALUE) {
        debug_log("find_handle == INVALID_HANDLE_VALUE");
    }

    do {
        ++s_num_file_searches;

        directory_entry entry;

        entry.is_directory = find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;

        entry.size = static_cast<u64>(find_data.nFileSizeHigh) << 32;
        entry.size |= static_cast<u64>(find_data.nFileSizeLow);

        std::memcpy(entry.path.data(), find_data.cFileName, entry.path.size());

        if (strcmp(entry.path.data(), ".") == 0) {
            continue;
        }

        s_dir_entries.emplace_back(entry);

        if (strcmp(entry.path.data(), "..") == 0) {
            std::swap(s_dir_entries.back(), s_dir_entries.front());
        }
    }
    while (FindNextFileA(find_handle, &find_data));

    std::sort(s_dir_entries.begin(), s_dir_entries.end(), [](directory_entry const &lhs, directory_entry const &rhs) {
        if (lhs.is_directory && rhs.is_directory) {
            return strcmp(lhs.path.data(), "..") == 0;
        }
        else {
            return lhs.is_directory;
        }
    });
}

void try_descend_to_directory(path_t &working_dir, char const *child_dir)
{
    path_t new_working_dir = working_dir;

    auto app_res = path_append_result::nil;
    if (!path_ends_with(working_dir, "\\/")) {
        app_res = path_append(working_dir, "\\");
    }
    app_res = path_append(working_dir, child_dir);

    if (app_res != path_append_result::success) {
        debug_log("path_append failed, working_dir = [%s], append data = [\\%s]", working_dir.data(), child_dir);
        working_dir = new_working_dir;
    } else {
        if (PathCanonicalizeA(new_working_dir.data(), working_dir.data())) {
            debug_log("PathCanonicalizeA success: new_working_dir = [%s]", new_working_dir.data());
            working_dir = new_working_dir;
            update_dir_entries(working_dir.data());
        } else {
            debug_log("PathCanonicalizeA failed");
        }
    }
}

static
i32 cwd_text_input_callback(ImGuiInputTextCallbackData *data)
{
    if (data->EventFlag == ImGuiInputTextFlags_CallbackCharFilter) {
        static std::wstring const forbidden_chars = L"<>\"|?*";
        bool is_forbidden = forbidden_chars.find(data->EventChar) != std::string::npos;
        if (is_forbidden) {
            data->EventChar = L'\0';
        }
    }
    else if (data->EventFlag == ImGuiInputTextFlags_CallbackEdit) {
        debug_log("ImGuiInputTextFlags_CallbackEdit, data->Buf = [%s]", data->Buf);
        update_dir_entries(data->Buf);
    }

    return 0;
}

static
void render_file_explorer()
{
    auto &io = ImGui::GetIO();

    if (!ImGui::Begin("Explorer")) {
        ImGui::End();
        return;
    }

    {
        static f32 labels_width = max(ImGui::CalcTextSize(" <= cwd ").x, ImGui::CalcTextSize(" <= filter ").x);

        ImGui::PushItemWidth(ImGui::GetColumnWidth() - labels_width);
        ImGui::InputText(" <= cwd ", s_working_dir.data(), s_working_dir.size(),
            ImGuiInputTextFlags_CallbackCharFilter|ImGuiInputTextFlags_CallbackEdit, cwd_text_input_callback);

        ImGui::PushItemWidth(ImGui::GetColumnWidth() - labels_width);
        ImGui::InputText(" <= filter ", s_filter.data(), s_filter.size());
    }

    ImGui::Spacing();

    if (s_dir_entries.empty()) {
        ImGui::Text("Not a directory.");
        ImGui::End();
        return;
    }

    static ImVec4 const white(255, 255, 255, 255);
    static ImVec4 const yellow(255, 255, 0, 255);

    if (ImGui::BeginTable("Entries", 3, ImGuiTableFlags_Resizable|ImGuiTableFlags_Reorderable|ImGuiTableFlags_Sortable)) {
        enum class column_id : u32 { number, path, size, };

        ImGui::TableSetupColumn("Number", 0, 0.0f, (u32)column_id::number);
        ImGui::TableSetupColumn("Path", ImGuiTableColumnFlags_DefaultSort, 0.0f, (u32)column_id::path);
        ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_DefaultSort, 0.0f, (u32)column_id::size);
        ImGui::TableHeadersRow();

        if (ImGui::IsWindowFocused() && ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            for (auto &dir_ent2 : s_dir_entries)
                dir_ent2.is_selected = false;
        }
        else if (ImGui::IsWindowFocused() && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_A)) {
            for (auto &dir_ent2 : s_dir_entries)
                dir_ent2.is_selected = true;
        }

        for (u64 i = 0; i < s_dir_entries.size(); ++i) {
            auto &dir_ent = s_dir_entries[i];

            ImGui::TableNextRow();

            {
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%zu", i + 1);
            }

            {
                ImGui::TableSetColumnIndex(1);
                ImGui::PushStyleColor(ImGuiCol_Text, dir_ent.is_directory ? yellow : white);

                if (ImGui::Selectable(dir_ent.path.data(), dir_ent.is_selected, ImGuiSelectableFlags_SpanAllColumns)) {
                    if (!io.KeyCtrl && !io.KeyShift) {
                        // entry was selected but Ctrl was not held, so deselect everything
                        for (auto &dir_ent2 : s_dir_entries)
                            dir_ent2.is_selected = false;
                    }

                    swan::flip_bool(dir_ent.is_selected);

                    if (io.KeyShift) {
                        // shift click, select everything between the current item and the previously clicked item

                        u64 first_idx, last_idx;

                        if (i <= s_last_selected_dirent_idx) {
                            // prev selected item below current one
                            first_idx = i;
                            last_idx = s_last_selected_dirent_idx;
                        }
                        else {
                            first_idx = s_last_selected_dirent_idx;
                            last_idx = i;
                        }

                        debug_log("shift click, [%zu, %zu]", first_idx, last_idx);

                        for (u64 j = first_idx; j <= last_idx; ++j)
                            s_dir_entries[j].is_selected = true;
                    }

                    static f64 last_click_time = 0;
                    f64 current_time = ImGui::GetTime();

                    if (current_time - last_click_time <= 0.2) {
                        if (dir_ent.is_directory) {
                            debug_log("double clicked directory [%s]", dir_ent.path.data());
                            try_descend_to_directory(s_working_dir, dir_ent.path.data());
                        }
                        else {
                            debug_log("double clicked file [%s]", dir_ent.path.data());
                            HINSTANCE result = ShellExecuteA(nullptr, "open", dir_ent.path.data(), nullptr, nullptr, SW_SHOWNORMAL);
                            (void) result;
                        }
                    }
                    else {
                        debug_log("selected [%s]", dir_ent.path.data());
                    }

                    last_click_time = current_time;
                    s_last_selected_dirent_idx = i;
                }

                ImGui::PopStyleColor();
            }

            {
                ImGui::TableSetColumnIndex(2);
                if (dir_ent.is_directory) {
                    ImGui::Text("");
                }
                else {
                    ImGui::Text("%zu", dir_ent.size);
                }
            }
        }

        if (ImGui::IsKeyPressed(ImGuiKey_Enter)) {
            auto dirent_which_enter_pressed_on = s_dir_entries[s_last_selected_dirent_idx];
            debug_log("pressed enter on [%s]", dirent_which_enter_pressed_on.path.data());
            if (dirent_which_enter_pressed_on.is_directory) {
                try_descend_to_directory(s_working_dir, dirent_which_enter_pressed_on.path.data());
            }
        }

        ImGui::EndTable();
    }

    ImGui::End();
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

i32 main(i32, char**)
{
    GLFWwindow *window = init_glfw_and_imgui();
    if (window == nullptr) {
        return 1;
    }

    auto window_cleanup_routine = make_on_scope_exit([window]() {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        glfwDestroyWindow(window);
        glfwTerminate();
    });

    [[maybe_unused]] auto &io = ImGui::GetIO();

    {
        GLFWimage icon;
        icon.pixels = nullptr;
        icon.width = 0;
        icon.height = 0;

        // Load the icon image from file
        int icon_width, iconHeight, iconChannels;
        u8 *iconPixels = stbi_load("swan.png", &icon_width, &iconHeight, &iconChannels, STBI_rgb_alpha);
        if (iconPixels)
        {
            icon.pixels = iconPixels;
            icon.width = icon_width;
            icon.height = iconHeight;

            // Set the window icon
            glfwSetWindowIcon(window, 1, &icon);
        }

        // Free the icon image data
        stbi_image_free(iconPixels);
    }

    s_dir_entries.reserve(1024);

    {
        i32 written = GetCurrentDirectoryA((i32)s_working_dir.size(), s_working_dir.data());
        if (written == 0) {
            std::exit(1);
        }
    }

    update_dir_entries(s_working_dir.data());

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::DockSpaceOverViewport(0, ImGuiDockNodeFlags_PassthruCentralNode);

        render_file_explorer();

        if (ImGui::Begin("Analytics")) {
            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
            ImGui::Text("s_working_dir = [%s]", s_working_dir.data());
            ImGui::Text("cwd_exists = [%d]", directory_exists(s_working_dir.data()));
            ImGui::Text("s_num_file_searches = [%zu]", s_num_file_searches);
            ImGui::Text("s_dir_entries.size() = [%zu]", s_dir_entries.size());
            ImGui::Text("s_last_selected_dirent_idx = [%lld]", s_last_selected_dirent_idx);
        }
        ImGui::End();

        ImGui::ShowDemoWindow();

        render(window);
    }

    return 0;
}
