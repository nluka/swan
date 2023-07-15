#ifndef SWAN_COMMON_HPP
#define SWAN_COMMON_HPP

#include <vector>
#include <deque>
#include <string>

#include "path.hpp"
#include "primitives.hpp"
#include "imgui/imgui.h"

struct windows_options
{
    bool show_pinned;
    bool show_explorer_0;
    bool show_explorer_1;
    bool show_explorer_2;
    bool show_explorer_3;
    bool show_analytics;
#if !defined(NDEBUG)
    bool show_demo;
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
            directory,
            symlink,
            file,
            count
        };

        u64 size;
        FILETIME creation_time_raw;
        FILETIME last_write_time_raw;
        u32 id;
        ent_type type;
        bool is_filtered_out;
        bool is_selected;
        swan::path_t path;

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
    LARGE_INTEGER last_refresh_timestamp = {};
    u64 cwd_prev_selected_dirent_idx = NO_SELECTION; // idx of most recently clicked cwd entry, NO_SELECTION means there isn't one
    u64 wd_history_pos = 0; // where in wd_history we are, persisted in file
    u64 num_file_finds = 0;
    f64 sort_us = 0;
    f64 check_if_pinned_us = 0;
    f64 unpin_us = 0;
    f64 update_cwd_entries_total_us = 0;
    f64 update_cwd_entries_searchpath_setup_us = 0;
    f64 update_cwd_entries_filesystem_us = 0;
    f64 update_cwd_entries_filter_us = 0;
    f64 update_cwd_entries_regex_ctor_us = 0;
    f64 update_cwd_entries_save_to_disk_us = 0;

    // 1 byte alignment members

    i8 latest_save_to_disk_result = -1;
    swan::path_t cwd = {}; // current working directory, persisted in file
    std::array<char, 256> filter = {}; // persisted in file
    bool filter_case_sensitive = false; // persisted in file
    bool needs_initial_sort = true;
};

constexpr u8 const query_filesystem = 1 << 0;
constexpr u8 const filter = 1 << 1;
constexpr u8 const full_refresh = query_filesystem | filter;

void update_cwd_entries(
    u8 actions,
    explorer_window *,
    std::string_view parent_dir,
    explorer_options const &);

void new_history_from(explorer_window &expl, swan::path_t const &new_latest_entry);

std::vector<swan::path_t> const &get_pins() noexcept(true);

bool pin(swan::path_t &path, char dir_separator) noexcept(true);

void unpin(u64 pin_idx) noexcept(true);

void update_pin_dir_separators(char new_dir_separator) noexcept(true);

bool save_pins_to_disk() noexcept(true);

std::pair<bool, u64> load_pins_from_disk(char dir_separator) noexcept(true);

u64 find_pin_idx(swan::path_t const &) noexcept(true);

#endif // SWAN_COMMON_HPP
