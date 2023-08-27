#include <cassert>

#include "BS_thread_pool.hpp"
#include "common.hpp"
#include "path.hpp"
#include "on_scope_exit.hpp"

#include "imgui/imgui.h"

static BS::thread_pool s_thread_pool(1);

bool basic_dirent::is_dotdot() const noexcept { return path_equals_exactly(path, ".."); }
bool basic_dirent::is_dotdot_dir() const noexcept { return type == basic_dirent::kind::directory && path_equals_exactly(path, ".."); }
bool basic_dirent::is_directory() const noexcept { return type == basic_dirent::kind::directory; }
bool basic_dirent::is_symlink() const noexcept { return type == basic_dirent::kind::symlink; }
bool basic_dirent::is_file() const noexcept { return type != basic_dirent::kind::directory; }
bool basic_dirent::is_non_symlink_file() const noexcept { return is_file() && !is_symlink(); }

ImVec4 get_color(basic_dirent::kind t) noexcept
{
    static ImVec4 const pale_green(0.85f, 1, 0.85f, 1);
    static ImVec4 const yellow(1, 1, 0, 1);
    static ImVec4 const cyan(0.1f, 1, 1, 1);

    if (t == basic_dirent::kind::directory) return yellow;
    if (t == basic_dirent::kind::symlink) return cyan;
    else return pale_green;
}

char explorer_options::dir_separator_utf8() const noexcept { return unix_directory_separator ? '/' : '\\'; }
u16 explorer_options::size_unit_multiplier() const noexcept { return binary_size_system ? 1024 : 1000; }

file_name_ext::file_name_ext(char *path) noexcept
{
    this->name = get_file_name(path);
    this->ext = get_file_ext(name);
    this->dot = ext ? ext - 1 : nullptr;
    if (this->dot) {
        *this->dot = '\0';
    }
}

file_name_ext::~file_name_ext() noexcept
{
    if (this->dot) {
        *this->dot = '.';
    }
}

char *get_file_name(char *path) noexcept
{
    // C:\code\swan\src\explorer_window.cpp
    //                  ^^^^^^^^^^^^^^^^^^^ what we are after
    // src/swan.cpp
    //     ^^^^^^^^ what we are after

    assert(path != nullptr);

    char *just_the_file_name = path;

    std::string_view view(just_the_file_name);

    u64 last_sep_pos = view.find_last_of("\\/");

    if (last_sep_pos != std::string::npos) {
        just_the_file_name += last_sep_pos + 1;
    }

    return just_the_file_name;
}

char const *cget_file_name(char const *path) noexcept
{
    // C:\code\swan\src\explorer_window.cpp
    //                  ^^^^^^^^^^^^^^^^^^^ what we are after
    // src/swan.cpp
    //     ^^^^^^^^ what we are after

    assert(path != nullptr);

    char const *just_the_file_name = path;

    std::string_view view(just_the_file_name);

    u64 last_sep_pos = view.find_last_of("\\/");

    if (last_sep_pos != std::string::npos) {
        just_the_file_name += last_sep_pos + 1;
    }

    return just_the_file_name;
}

char *get_file_ext(char *path) noexcept
{
    // C:\code\swan\src\explorer_window.cpp
    //                                  ^^^ what we are after
    // src/swan.cpp
    //          ^^^ what we are after

    assert(path != nullptr);

    char *just_the_file_ext = path;

    std::string_view view(just_the_file_ext);

    u64 last_dot_pos = view.find_last_of(".");

    if (last_dot_pos != std::string::npos) {
        just_the_file_ext += last_dot_pos + 1;
        return just_the_file_ext;
    } else {
        return nullptr;
    }
}

std::string_view get_everything_minus_file_name(char const *path) noexcept
{
    // C:\code\swan\src\explorer_window.cpp
    // ^^^^^^^^^^^^^^^^^ what we are after
    // src/swan.cpp
    // ^^^^ what we are after

    assert(path != nullptr);

    std::string_view full_path_view(path);

    u64 last_sep_pos = full_path_view.find_last_of("\\/");

    if (last_sep_pos == std::string::npos) {
        return path;
    }
    else {
        return std::string_view(path, last_sep_pos + 1);
    }
}

std::string get_last_error_string() noexcept
{
    DWORD error_code = GetLastError();
    if (error_code == 0) {
        return "No error.";
    }

    LPSTR buffer = nullptr;
    DWORD buffer_size = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        error_code,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPSTR>(&buffer),
        0,
        nullptr
    );

    if (buffer_size == 0) {
        return "Error formatting message.";
    }

    std::string error_message(buffer, buffer + buffer_size);
    LocalFree(buffer);

    // Remove trailing newline characters
    while (!error_message.empty() && (error_message.back() == '\r' || error_message.back() == '\n')) {
        error_message.pop_back();
    }

    return error_message;
}

void imgui_sameline_spacing(u64 num_spacing_calls) noexcept
{
    ImGui::SameLine();
    for (u64 i = 0; i < num_spacing_calls; ++i) {
        ImGui::Spacing();
        ImGui::SameLine();
    }
}
