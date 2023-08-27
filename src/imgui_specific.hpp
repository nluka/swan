#pragma once

#include <source_location>

#include "common.hpp"

#include "imgui/imgui.h"

ImVec4 get_color(basic_dirent::kind t) noexcept;

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
void debug_log([[maybe_unused]] debug_log_package pack, [[maybe_unused]] Args&&... args)
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
