#include <cassert>

#include "BS_thread_pool.hpp"
#include "common.hpp"
#include "path.hpp"
#include "on_scope_exit.hpp"

#include "imgui/imgui.h"

static swan_thread_pool_t s_thread_pool(1);
swan_thread_pool_t &get_thread_pool() noexcept { return s_thread_pool; }

bool basic_dirent::is_dotdot() const noexcept { return path_equals_exactly(path, ".."); }
bool basic_dirent::is_dotdot_dir() const noexcept { return type == basic_dirent::kind::directory && path_equals_exactly(path, ".."); }
bool basic_dirent::is_directory() const noexcept { return type == basic_dirent::kind::directory; }
bool basic_dirent::is_symlink() const noexcept { return type == basic_dirent::kind::symlink; }
bool basic_dirent::is_file() const noexcept { return type != basic_dirent::kind::directory; }
bool basic_dirent::is_non_symlink_file() const noexcept { return is_file() && !is_symlink(); }

char const *basic_dirent::kind_cstr() const noexcept
{
    assert(this->type >= basic_dirent::kind::directory && this->type <= basic_dirent::kind::symlink);

    switch (this->type) {
        case basic_dirent::kind::directory: return "directory";
        case basic_dirent::kind::file: return "file";
        case basic_dirent::kind::symlink: return "symlink";
        default: return "";
    }
}

ImVec4 get_color(basic_dirent::kind t) noexcept
{
    if (t == basic_dirent::kind::directory)
        return ImVec4(1, 1, 0, 1); // yellow
    if (t == basic_dirent::kind::symlink)
        return ImVec4(0.1f, 1, 1, 1); // cyan
    else
        return ImVec4(0.85f, 1, 0.85f, 1); // pale_green
}

void imgui_spacing(u64 n) noexcept
{
    for (u64 i = 0; i < n; ++i) {
        ImGui::Spacing();
    }
}

char explorer_options::dir_separator_utf8() const noexcept { return unix_directory_separator ? '/' : '\\'; }
u16 explorer_options::size_unit_multiplier() const noexcept { return binary_size_system ? 1024 : 1000; }

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
