#ifndef SWAN_COMMON_HPP
#define SWAN_COMMON_HPP

#include <vector>
#include <deque>
#include <string>
#include <source_location>

#include "path.hpp"
#include "util.hpp"
#include "primitives.hpp"
#include "imgui/imgui.h"

struct windows_options
{
    bool show_pinned;
    bool show_file_operations;
    bool show_explorer_0;
    bool show_explorer_1;
    bool show_explorer_2;
    bool show_explorer_3;
    bool show_analytics;
#if !defined(NDEBUG)
    bool show_demo;
    bool show_debug_log;
#endif

    bool save_to_disk() const noexcept(true);
    bool load_from_disk() noexcept(true);
};

struct explorer_options
{
    bool binary_size_system; // if true, value for Kilo/Mega/Giga/Tera = 1024, else 1000
    bool show_cwd_len;
    bool show_debug_info;
    bool automatic_refresh;
    bool show_dotdot_dir;
    bool unix_directory_separator;

    bool save_to_disk() const noexcept(true);
    bool load_from_disk() noexcept(true);

    char dir_separator() const noexcept(true)
    {
        return unix_directory_separator ? '/' : '\\';
    }
};

struct explorer_window
{
    struct directory_entry
    {
        enum class ent_type : u8
        {
            nil = 0,
            directory,
            symlink,
            file,
            count
        };

        u64 size = 0;
        FILETIME creation_time_raw = {};
        FILETIME last_write_time_raw = {};
        u32 id = {};
        ent_type type = ent_type::nil;
        bool is_filtered_out = false;
        bool is_selected = false;
        swan::path_t path = {};

        bool is_directory() const noexcept(true)
        {
            return type == directory_entry::ent_type::directory;
        }

        bool is_symlink() const noexcept(true)
        {
            return type == directory_entry::ent_type::symlink;
        }

        bool is_file() const noexcept(true)
        {
            return type != directory_entry::ent_type::directory;
        }

        bool is_non_symlink_file() const noexcept(true)
        {
            return is_file() && !is_symlink();
        }

        ImVec4 get_color() const noexcept(true)
        {
            return get_color(this->type);
        }

        static ImVec4 get_color(ent_type type) noexcept(true)
        {
            static ImVec4 const pale_green(0.85f, 1, 0.85f, 1);
            static ImVec4 const yellow(1, 1, 0, 1);
            static ImVec4 const cyan(0.1f, 1, 1, 1);

            if (type == ent_type::directory) return yellow;
            if (type == ent_type::symlink) return cyan;
            else return pale_green;
        }
    };

    enum filter_mode : u64
    {
        contains,
        regex,
        // glob,
        count,
    };

    static u64 const NO_SELECTION = UINT64_MAX;
    bool save_to_disk() const noexcept(true);
    bool load_from_disk(char dir_separator) noexcept(true);

    // 40 byte members

    std::deque<swan::path_t> wd_history = {}; // history for working directories, persisted in file

    // 32 byte members

    std::string filter_error = "";

    // 24 byte members

    std::vector<directory_entry> cwd_entries = {}; // 24 bytes, all direct children of the cwd

    // 8 byte members

    char const *name = nullptr;
    filter_mode filter_mode = filter_mode::contains; // persisted in file
    time_point_t last_refresh_time = {};
    u64 cwd_prev_selected_dirent_idx = NO_SELECTION; // idx of most recently clicked cwd entry, NO_SELECTION means there isn't one
    u64 num_selected_cwd_entries = 0;
    u64 wd_history_pos = 0; // where in wd_history we are, persisted in file
    mutable u64 num_file_finds = 0;
    mutable f64 sort_us = 0;
    mutable f64 check_if_pinned_us = 0;
    mutable f64 unpin_us = 0;
    mutable f64 update_cwd_entries_total_us = 0;
    mutable f64 update_cwd_entries_searchpath_setup_us = 0;
    mutable f64 update_cwd_entries_filesystem_us = 0;
    mutable f64 update_cwd_entries_filter_us = 0;
    mutable f64 update_cwd_entries_regex_ctor_us = 0;
    mutable f64 save_to_disk_us = 0;

    // 1 byte alignment members

    mutable i8 latest_save_to_disk_result = -1;
    swan::path_t prev_valid_cwd = {};
    swan::path_t cwd = {}; // current working directory, persisted in file
    swan::path_t cwd_last_frame = {};
    std::array<char, 256> filter = {}; // persisted in file
    bool filter_case_sensitive = false; // persisted in file
    bool needs_initial_sort = true;
};

struct file_operation
{
    enum class type : u8
    {
        nil = 0,
        move,
        copy,
        remove,
        count
    };

    u64 total_file_size = 0;
    u64 total_bytes_transferred = 0;
    u64 stream_size = 0;
    u64 stream_bytes_transferred = 0;
    time_point_t completed_time = {};
    type op_type = type::nil;
    swan::path_t src_path = {};
    swan::path_t dest_path = {};
};

struct file_op_progress_callback_user_data
{
    file_operation *file_op = nullptr;
};

DWORD file_op_progress_callback(
    LARGE_INTEGER total_file_size,
    LARGE_INTEGER total_bytes_transferred,
    LARGE_INTEGER steam_size,
    LARGE_INTEGER steam_bytes_transferred,
    DWORD stream_num,
    DWORD callback_reason,
    HANDLE src_handle,
    HANDLE dest_handle,
    void *user_data) noexcept(true);

bool enqueue_file_op(
    file_operation::type op_type,
    swan::path_t const &file_path,
    swan::path_t const &dest_path,
    char dir_separator) noexcept(true);

std::deque<file_operation> const &get_file_ops_queue() noexcept(true);

constexpr u8 const query_filesystem = 1 << 0;
constexpr u8 const filter = 1 << 1;
constexpr u8 const full_refresh = query_filesystem | filter;

bool update_cwd_entries(
    u8 actions,
    explorer_window *,
    std::string_view parent_dir,
    explorer_options const &,
    std::source_location sloc = std::source_location::current());

void new_history_from(explorer_window &expl, swan::path_t const &new_latest_entry);

std::vector<swan::path_t> const &get_pins() noexcept(true);

bool pin(swan::path_t &path, char dir_separator) noexcept(true);

void unpin(u64 pin_idx) noexcept(true);

void update_pin_dir_separators(char new_dir_separator) noexcept(true);

bool save_pins_to_disk() noexcept(true);

std::pair<bool, u64> load_pins_from_disk(char dir_separator) noexcept(true);

u64 find_pin_idx(swan::path_t const &) noexcept(true);

char const *get_just_file_name(char const *std__source_location__file_path) noexcept(true);

struct debug_log_package {
    char const *fmt;
    std::source_location loc;
    time_point_t time;
    static ImGuiTextBuffer s_debug_buffer;

    debug_log_package(char const *f, std::source_location l = std::source_location::current()) noexcept(true)
        : fmt(f)
        , loc(l)
        , time(current_time())
    {}

    static void clear_buffer() noexcept(true)
    {
        s_debug_buffer.clear();
    }
};

// https://stackoverflow.com/questions/57547273/how-to-use-source-location-in-a-variadic-template-function
template <typename... Args>
void debug_log([[maybe_unused]] debug_log_package pack, [[maybe_unused]] Args&&... args)
{
// #if !defined(NDEBUG)
    auto &debug_buffer = debug_log_package::s_debug_buffer;
    u64 const max_size = 1024 * 1024 * 10;

    if (debug_buffer.size() > max_size) {
        debug_buffer.clear();
    }

    char const *just_the_file_name = get_just_file_name(pack.loc.file_name());

    debug_buffer.appendf("%21s:%5d ", just_the_file_name, pack.loc.line());
    debug_buffer.appendf(pack.fmt, args...);
    debug_buffer.append("\n");
// #endif
}

#endif // SWAN_COMMON_HPP
