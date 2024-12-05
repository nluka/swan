/*
    Functions and structures coupled to the existence of ImGui.
*/

#pragma once

#include "stdafx.hpp"
#include "common_functions.hpp"
#include "imgui_extension.hpp"

void BeginFrame_GLFW_OpenGL3(char const *ini_file_path) noexcept;
void EndFrame_GLFW_OpenGL3(GLFWwindow *) noexcept;

void BeginFrame_Win32_DX11(char const *ini_file_path) noexcept;
bool EndFrame_Win32_DX11(ID3D11DeviceContext *, ID3D11RenderTargetView *, IDXGISwapChain *) noexcept;

void center_window_and_set_size_when_appearing(f32 width, f32 height) noexcept;

ImVec4 get_color(basic_dirent::kind t) noexcept;
ImVec4 get_color(bulk_rename_transform::status s) noexcept;

ImVec4 success_color() noexcept;
ImVec4 warning_color() noexcept;
ImVec4 warning_lite_color() noexcept;
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

ImGuiStyle swan_default_imgui_style() noexcept;

typedef wchar_t* filter_chars_callback_user_data_t;
s32 filter_chars_callback(ImGuiInputTextCallbackData *data) noexcept;

std::variant<s64, basic_dirent::kind> appropriate_icon(s64 tex_id, basic_dirent::kind obj_type) noexcept;

void render_path_with_stylish_separators(char const *path, std::variant<s64, basic_dirent::kind> icon) noexcept;

struct debug_log_record
{
    std::string message;                static_assert(sizeof(message) == 32);
    std::source_location loc;           static_assert(sizeof(loc) == 24);
    time_point_system_t system_time;    static_assert(sizeof(system_time) == 8);
    f64 imgui_time;
    s32 thread_id;
    u32 num_repeats; // when a duplicate message is printed, this counter is incremented instead of creating a new instance

    bool matches_search_text(char const *search_substr) const noexcept
    {
        assert(!cstr_empty(search_substr));
        bool match =
            StrStrIA(this->loc.file_name(),                               search_substr) ||
            StrStrIA(this->loc.function_name(),                           search_substr) ||
            StrStrIA(this->message.c_str(),                               search_substr) ||
            StrStrIA(make_str_static<32>("%zu", this->loc.line()).data(), search_substr) ||
            StrStrIA(make_str_static<32>("%zu", this->thread_id).data(),  search_substr)
        ;
        return match;
    }
};

struct debug_log
{
    char const *fmt;
    std::source_location loc;

    static std::string g_search_text;
    static std::vector<debug_log_record> g_records;
    static std::vector<u64> g_records_visible_indices;
    static std::mutex g_mutex;
    static bool g_logging_enabled;

    debug_log(char const *f, std::source_location l = std::source_location::current()) noexcept
        : fmt(f)
        , loc(l)
    {}

    static void clear() noexcept
    {
        g_records.clear();
        g_records_visible_indices.clear();
    }
};

/// @brief Writes a message to the debug log window (not stdout!). Information such as time, thread id, source location are handled for you.
/// Use this function like you would sprintf, pass it a format string followed by your variadic arguments. Operation is threadsafe, you can
/// print messages from any thread at any time.
template <typename... Args>
void print_debug_msg([[maybe_unused]] debug_log pack, [[maybe_unused]] Args&&... args) noexcept
{
    // https://stackoverflow.com/questions/57547273/how-to-use-source-location-in-a-variadic-template-function

    if (!debug_log::g_logging_enabled) {
        return;
    }

    u64 max_size = global_state::debug_log_size_limit_megabytes() * 1024 * 1024;

    auto formatted_message = make_str_static<4096>(pack.fmt, args...);
    f64 imgui_time = imgui::GetTime();
    s32 thread_id = GetCurrentThreadId();
    time_point_system_t system_time = get_time_system();

    std::time_t time = std::chrono::system_clock::to_time_t(system_time);
    std::tm tm = *std::localtime(&time);

    auto markdown_line = make_str_static<4096*2>("| %d | %d:%02d.%02d | %.3lf | %s | %d | %s | %s |\n",
        thread_id,
        tm.tm_hour, tm.tm_min, tm.tm_sec,
        imgui_time,
        path_cfind_filename(pack.loc.file_name()),
        pack.loc.line(),
        pack.loc.function_name(),
        formatted_message.data());

    {
        std::scoped_lock lock(debug_log::g_mutex);

        bool record_matches_search_text = false;

        if (debug_log::g_search_text.size()) {
            auto markdown_line_copy = markdown_line;
            char const *piece = strtok(markdown_line_copy.data(), "|");
            while (piece != nullptr) {
                if (StrStrIA(piece, debug_log::g_search_text.c_str())) {
                    record_matches_search_text = true;
                    break;
                }
                piece = strtok(nullptr, "|");
            }
        }

        if (!debug_log::g_records.empty() && cstr_eq(formatted_message.data(), debug_log::g_records.back().message.c_str())) {
            debug_log::g_records.back().num_repeats += 1;
        }
        else {
            u64 used_size = sizeof(debug_log_record) * debug_log::g_records.size();
            if (used_size >= max_size) {
                u64 halfway = debug_log::g_records.size() / 2;
                debug_log::g_records.erase(debug_log::g_records.begin(), debug_log::g_records.begin() + halfway);
            }
            debug_log::g_records.reserve(max_size / sizeof(debug_log_record));

            debug_log::g_records.emplace_back(formatted_message.data(), pack.loc, system_time, imgui_time, thread_id, 0);

            if (debug_log::g_search_text.empty() || record_matches_search_text) {
                debug_log::g_records_visible_indices.push_back(std::max<u64>(debug_log::g_records.size(), 1) - 1);
            }
        }
    }

    auto log_file_path = global_state::execution_path() / "debug_log.md";
    FILE *file = fopen(log_file_path.string().c_str(), "a");
    if (file) {
        fprintf(file, markdown_line.data());
        fflush(file);
        fclose(file);
    }
}
