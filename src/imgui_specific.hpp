#pragma once

#include <source_location>

#include "common.hpp"

#include "imgui/imgui.h"

ImVec4 orange() noexcept;
ImVec4 red() noexcept;
ImVec4 dir_color() noexcept;
ImVec4 symlink_color() noexcept;
ImVec4 file_color() noexcept;

ImVec4 get_color(basic_dirent::kind t) noexcept;

void imgui_spacing(u64 n) noexcept;

typedef wchar_t* filter_chars_callback_user_data_t;
s32 filter_chars_callback(ImGuiInputTextCallbackData *data) noexcept;

struct debug_log_package
{
    char const *fmt;
    std::source_location loc;
    time_point_t time;

    static ImGuiTextBuffer s_debug_buffer;
    static bool s_logging_enabled;

    debug_log_package(char const *f, std::source_location l = std::source_location::current()) noexcept
        : fmt(f)
        , loc(l)
        , time(current_time())
    {}

    static void clear_buffer() noexcept
    {
        s_debug_buffer.clear();
    }
};

// https://stackoverflow.com/questions/57547273/how-to-use-source-location-in-a-variadic-template-function
template <typename... Args>
void debug_log([[maybe_unused]] debug_log_package pack, [[maybe_unused]] Args&&... args) noexcept
{
    if (!debug_log_package::s_logging_enabled) {
        return;
    }

    auto &debug_buffer = debug_log_package::s_debug_buffer;
    u64 const max_size = 1024 * 1024 * 10;

    debug_buffer.reserve(max_size);

    if (debug_buffer.size() > max_size) {
        debug_buffer.clear();
    }

    char const *just_the_file_name = cget_file_name(pack.loc.file_name());

    debug_buffer.appendf("%22s:%5d ", just_the_file_name, pack.loc.line());
    debug_buffer.appendf(pack.fmt, args...);
    debug_buffer.append("\n");
}