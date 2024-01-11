/*
    Functions and structures coupled to the existence of ImGui.
*/

#pragma once

#include "stdafx.hpp"
#include "common_fns.hpp"
#include "imgui_ext.hpp"

void new_frame(char const *ini_file_path) noexcept;
void render_frame(GLFWwindow *window) noexcept;

ImVec4 orange() noexcept;
ImVec4 red() noexcept;
ImVec4 dir_color() noexcept;
ImVec4 symlink_color() noexcept;
ImVec4 file_color() noexcept;
ImVec4 get_color(basic_dirent::kind t) noexcept;

typedef wchar_t* filter_chars_callback_user_data_t;
s32 filter_chars_callback(ImGuiInputTextCallbackData *data) noexcept;

struct debug_log_package
{
    char const *fmt;
    std::source_location loc;

    static ImGuiTextBuffer s_buffer;
    static std::mutex s_mutex;
    static bool s_logging_enabled;

    debug_log_package(char const *f, std::source_location l = std::source_location::current()) noexcept
        : fmt(f)
        , loc(l)
    {}

    static void clear_buffer() noexcept
    {
        s_buffer.clear();
    }
};

/// @brief Writes a message to the debug log window (not stdout!). Information such as time, thread id, source location are handled for you.
/// Use this function like you would sprintf, pass it a format string followed by your variadic arguments. Operation is threadsafe, you can
/// print messages from any thread at any time safely.
template <typename... Args>
void print_debug_msg([[maybe_unused]] debug_log_package pack, [[maybe_unused]] Args&&... args) noexcept
{
    // https://stackoverflow.com/questions/57547273/how-to-use-source-location-in-a-variadic-template-function

    if (!debug_log_package::s_logging_enabled) {
        return;
    }

    f64 current_time = ImGui::GetTime();
    s32 max_size = global_state::debug_log_text_limit_megabytes() * 1024 * 1024;
    char const *just_the_file_name = cget_file_name(pack.loc.file_name());
    s32 thread_id = GetCurrentThreadId();

    {
        std::scoped_lock lock(debug_log_package::s_mutex);

        auto &debug_buffer = debug_log_package::s_buffer;

        debug_buffer.reserve(max_size);

        if (debug_buffer.size() > max_size) {
            debug_buffer.clear();
        }

        debug_buffer.appendf("%-5d %10.3lf %18s:%-5d ", thread_id, current_time, just_the_file_name, pack.loc.line());
        debug_buffer.appendf(pack.fmt, args...);
        debug_buffer.append("\n");
    }
}
