#pragma once

#include <vector>
#include <string>
#include <atomic>
#include <string_view>

#include <boost/container/static_vector.hpp>
#include <boost/circular_buffer.hpp>

#include "primitives.hpp"
#include "path.hpp"
#include "BS_thread_pool.hpp"
#include "util.hpp"

bool explorer_init_windows_shell_com_garbage() noexcept;
void explorer_cleanup_windows_shell_com_garbage() noexcept;

typedef BS::thread_pool swan_thread_pool_t;
swan_thread_pool_t &get_thread_pool() noexcept;

struct basic_dirent
{
    enum class kind : u8 {
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
    kind type = kind::nil;
    swan_path_t path = {};

    bool is_dotdot() const noexcept;
    bool is_dotdot_dir() const noexcept;
    bool is_directory() const noexcept;
    bool is_symlink() const noexcept;
    bool is_file() const noexcept;
    bool is_non_symlink_file() const noexcept;
};

struct drive_info
{
    u64 total_bytes;
    u64 available_bytes;
    char name_utf8[512];
    char filesystem_name_utf8[512];
    char letter;
};

typedef boost::container::static_vector<drive_info, 26> drive_list_t;

drive_list_t query_drive_list();

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

    bool save_to_disk() const noexcept;
    bool load_from_disk() noexcept;
};

struct explorer_options
{
    enum class refresh_mode : i32
    {
        adaptive,
        manual,
        automatic,
        count
    };

    static i32 const min_tolerable_refresh_interval_ms = 500;

    i32 auto_refresh_interval_ms;
    i32 adaptive_refresh_threshold;
    refresh_mode ref_mode;
    bool binary_size_system; // if true, value for Kilo/Mega/Giga/Tera = 1024, else 1000
    bool show_cwd_len;
    bool show_debug_info;
    bool automatic_refresh;
    bool show_dotdot_dir;
    bool unix_directory_separator;

    bool save_to_disk() const noexcept;
    bool load_from_disk() noexcept;
    char dir_separator_utf8() const noexcept;
    u16 size_unit_multiplier() const noexcept;
};

template<typename T>
using circular_buffer = boost::circular_buffer<T>;

struct explorer_window
{
    struct dirent
    {
        basic_dirent basic;
        bool is_filtered_out = false;
        bool is_selected = false;
    };

    enum filter_mode : u64
    {
        contains,
        regex,
        // glob,
        count,
    };

    static u64 const NO_SELECTION = UINT64_MAX;
    static u64 const MAX_WD_HISTORY_SIZE = 15;
    bool save_to_disk() const noexcept;
    bool load_from_disk(char dir_separator) noexcept;
    void select_all_cwd_entries(bool select_dotdot_dir = false) noexcept;
    void deselect_all_cwd_entries() noexcept;

    // 40 byte members

    // history for working directories, persisted in file
    circular_buffer<swan_path_t> wd_history = circular_buffer<swan_path_t>(MAX_WD_HISTORY_SIZE);

    // 32 byte members

    std::string filter_error = "";

    // 24 byte members

    std::vector<dirent> cwd_entries = {}; // 24 bytes, all direct children of the cwd

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
    swan_path_t prev_valid_cwd = {};
    swan_path_t cwd = {}; // current working directory, persisted in file
    swan_path_t cwd_last_frame = {};
    std::array<char, 256> filter = {}; // persisted in file
    bool filter_case_sensitive = false; // persisted in file
    bool needs_sort = true;
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

    std::atomic<u64> total_file_size          = { 0 };
    std::atomic<u64> total_bytes_transferred  = { 0 };
    std::atomic<u64> stream_size              = { 0 };
    std::atomic<u64> stream_bytes_transferred = { 0 };
    std::atomic<time_point_t> start_time      = {   };
    std::atomic<time_point_t> end_time        = {   };
    type op_type = type::nil;
    bool success = false;
    swan_path_t src_path = {};
    swan_path_t dest_path = {};

    file_operation(type op_type, u64 file_size, swan_path_t const &src, swan_path_t const &dst) noexcept;
    file_operation(file_operation const &other) noexcept; // for boost::circular_buffer
    file_operation &operator=(file_operation const &other) noexcept; // for boost::circular_buffer
};

boost::circular_buffer<file_operation> const &get_file_ops_buffer() noexcept;

explorer_options &get_explorer_options() noexcept;

constexpr u8 const query_filesystem = 1 << 0;
constexpr u8 const filter = 1 << 1;
constexpr u8 const full_refresh = query_filesystem | filter;

bool update_cwd_entries(
    u8 actions,
    explorer_window *,
    std::string_view parent_dir,
    std::source_location sloc = std::source_location::current());

void new_history_from(explorer_window &expl, swan_path_t const &new_latest_entry);

std::vector<swan_path_t> const &get_pins() noexcept;

bool pin(swan_path_t &path, char dir_separator) noexcept;

void unpin(u64 pin_idx) noexcept;

void update_pin_dir_separators(char new_dir_separator) noexcept;

bool save_pins_to_disk() noexcept;

std::pair<bool, u64> load_pins_from_disk(char dir_separator) noexcept;

u64 find_pin_idx(swan_path_t const &) noexcept;

char *get_file_name(char *path) noexcept;
char const *cget_file_name(char const *path) noexcept;

char *get_file_ext(char *path) noexcept;
// char const *cget_file_ext(char const *path) noexcept;

struct file_name_ext
{
    char *name;
    char *ext;
    char *dot;

    file_name_ext(char *path) noexcept;
    ~file_name_ext() noexcept;
};

std::string_view get_everything_minus_file_name(char const *path) noexcept;

std::string get_last_error_string() noexcept;

bool save_focused_window(char const *window_name) noexcept;

bool load_focused_window_from_disk(char const *out) noexcept;

void imgui_sameline_spacing(u64 num_spacing_calls) noexcept;

struct bulk_rename_transform_result
{
    bool success;
    std::array<char, 128> error_msg;
};

bulk_rename_transform_result bulk_rename_transform(
    char const *name,
    char const *ext,
    std::array<char, (256 * 4) + 1> &after,
    char const *pattern,
    i32 counter,
    u64 bytes,
    bool squish_adjacent_spaces) noexcept;

struct bulk_rename_op
{
    basic_dirent *before;
    swan_path_t after;

    bool operator!=(bulk_rename_op const &other) const noexcept; // for ntest
    friend std::ostream& operator<<(std::ostream &os, bulk_rename_op const &r); // for ntest
};

struct bulk_rename_collision
{
    basic_dirent *dest_dirent;
    u64 first_rename_pair_idx;
    u64 last_rename_pair_idx;

    bool operator!=(bulk_rename_collision const &other) const noexcept; // for ntest
    friend std::ostream& operator<<(std::ostream &os, bulk_rename_collision const &c); // for ntest
};

void sort_renames_dup_elem_sequences_after_non_dups(std::vector<bulk_rename_op> &renames) noexcept;

std::vector<bulk_rename_collision> bulk_rename_find_collisions(
    std::vector<explorer_window::dirent> &dest,
    std::vector<bulk_rename_op> &renames) noexcept;

// TODO: make noexcept
void swan_render_window_explorer(explorer_window &);
void swan_render_window_pinned_directories(std::array<explorer_window, 4> &, windows_options const &) noexcept;
void swan_render_window_debug_log() noexcept;
void swan_render_window_file_operations() noexcept;
