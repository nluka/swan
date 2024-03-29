/*
    Functions and structures coupled to the existence of ImGui.
*/

#pragma once

#include "stdafx.hpp"
#include "common_functions.hpp"
#include "imgui_extension.hpp"

void new_frame(char const *ini_file_path) noexcept;
void render_frame(GLFWwindow *window) noexcept;

void center_window_and_set_size_when_appearing(f32 width, f32 height) noexcept;

ImVec4 get_color(basic_dirent::kind t) noexcept;

ImVec4 success_color() noexcept;
ImVec4 warning_color() noexcept;
ImVec4 error_color() noexcept;
ImVec4 directory_color() noexcept;
ImVec4 symlink_color() noexcept;
ImVec4 file_color() noexcept;

enum class serialize_ImGuiStyle_mode : s32
{
    plain_text,
    cpp_code,
};
std::string serialize_ImGuiStyle(ImGuiStyle const &style, u64 reserve_size, serialize_ImGuiStyle_mode mode) noexcept;
void serialize_ImGuiStyle_all_except_colors(ImGuiStyle const &style, std::string &out, serialize_ImGuiStyle_mode mode) noexcept;
void serialize_ImGuiStyle_only_colors(ImVec4 const *colors, std::string &out, serialize_ImGuiStyle_mode mode) noexcept;

inline ImGuiStyle swan_default_imgui_style() noexcept;

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
    char const *full_file_name = cget_file_name(pack.loc.file_name());
    s32 thread_id = GetCurrentThreadId();

    u64 const file_name_max_len = 30;
    char shortened_buf[file_name_max_len+1];
    char const *shortened_file_name = full_file_name;
    if (strlen(full_file_name) > file_name_max_len) {
        (void) snprintf(shortened_buf, lengthof(shortened_buf), "%*s%s", s32(file_name_max_len), full_file_name, ICON_MD_MORE);
        shortened_file_name = shortened_buf;
    }

    {
        std::scoped_lock lock(debug_log_package::s_mutex);

        auto &debug_buffer = debug_log_package::s_buffer;

        debug_buffer.reserve(max_size);

        if (debug_buffer.size() > max_size) {
            debug_buffer.clear();
        }

        debug_buffer.appendf("%-5d %10.3lf %*s:%-5d ", thread_id, current_time, file_name_max_len, shortened_file_name, pack.loc.line());
        debug_buffer.appendf(pack.fmt, args...);
        debug_buffer.append("\n");
    }
}
