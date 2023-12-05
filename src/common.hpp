#pragma once

#include "stdafx.hpp"
#include "path.hpp"
#include "util.hpp"

s32 get_page_size() noexcept;
void set_page_size(s32) noexcept;

void apply_swan_style_overrides() noexcept;

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
    char const *kind_cstr() const noexcept;
    char const *kind_icon() const noexcept;
};

char const *get_icon(basic_dirent::kind t) noexcept;

struct drive_info
{
    u64 total_bytes;
    u64 available_bytes;
    char name_utf8[512];
    char filesystem_name_utf8[512];
    char letter;
};

typedef boost::container::static_vector<drive_info, 26> drive_list_t;

drive_list_t query_drive_list() noexcept;

struct windows_options
{
    bool show_pins_mgr;
    bool show_file_operations;
    bool show_explorer_0;
    bool show_explorer_1;
    bool show_explorer_2;
    bool show_explorer_3;
    bool show_analytics;
#if !defined(NDEBUG)
    bool show_demo;
    bool show_debug_log;
    bool show_fa_icons;
    bool show_ci_icons;
    bool show_md_icons;
#endif

    bool save_to_disk() const noexcept;
    bool load_from_disk() noexcept;
};

struct explorer_options
{
    enum class refresh_mode : s32
    {
        automatic,
        manual,
        count
    };

    static s32 const min_tolerable_refresh_interval_ms = 100;

    std::atomic<s32> auto_refresh_interval_ms;
    refresh_mode ref_mode;
    bool binary_size_system; // if true, value for Kilo/Mega/Giga/Tera = 1024, else 1000
    bool show_cwd_len;
    bool show_debug_info;
    bool automatic_refresh;
    bool show_dotdot_dir;
    bool unix_directory_separator;
    bool cwd_entries_table_alt_row_bg;
    bool cwd_entries_table_borders_in_body;

    bool save_to_disk() const noexcept;
    bool load_from_disk() noexcept;
    char dir_separator_utf8() const noexcept;
    wchar_t dir_separator_utf16() const noexcept;
    u16 size_unit_multiplier() const noexcept;
};

explorer_options &get_explorer_options() noexcept;

struct misc_options
{
    bool save_to_disk() const noexcept;
    bool load_from_disk() noexcept;
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
        contains = 0,
        regex,
        // glob,
        count,
    };

    static u64 const NO_SELECTION = UINT64_MAX;
    static u64 const MAX_WD_HISTORY_SIZE = 15;
    bool save_to_disk() const noexcept;
    bool load_from_disk(char dir_separator) noexcept;
    void select_all_visible_cwd_entries(bool select_dotdot_dir = false) noexcept;
    void deselect_all_cwd_entries() noexcept;
    void invert_selected_visible_cwd_entries() noexcept;
    void set_latest_valid_cwd_then_notify(swan_path_t const &new_val) noexcept;

    // 80 byte alignment members

    std::mutex latest_valid_cwd_mutex = {};
    std::mutex shlwapi_task_initialization_mutex = {};

    // 72 byte alignment members

    std::condition_variable latest_valid_cwd_cond = {};
    std::condition_variable shlwapi_task_initialization_cond = {};

    // 40 byte alignment members

    /* TODO:
        add time_departed, but seralizing time_point_t is not easy: https://stackoverflow.com/questions/22291506/persisting-stdchrono-time-point-instances
        might have to use system time instead
    */
    // struct history_item
    // {
    //     time_point_t? time_departed;
    //     swan_path_t path;
    // };

    // history for working directories, persisted in file
    circular_buffer<swan_path_t> wd_history = circular_buffer<swan_path_t>(MAX_WD_HISTORY_SIZE);

    // 32 byte alignment members

    std::string filter_error = "";

    // 24 byte alignment members

    std::vector<dirent> cwd_entries = {}; // 24 bytes, all direct children of the cwd

    // 8 byte alignment members

    char const *name = nullptr;
    s64 id = -1;
    filter_mode filter_mode = filter_mode::contains; // persisted in file
    time_point_t last_refresh_time = {};
    u64 cwd_prev_selected_dirent_idx = NO_SELECTION; // idx of most recently clicked cwd entry, NO_SELECTION means there isn't one
    // u64 num_selected_cwd_entries = 0;
    u64 wd_history_pos = 0; // where in wd_history we are, persisted in file
    ImGuiTableSortSpecs *sort_specs = nullptr;

    std::atomic<time_point_t> refresh_notif_time = {};

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

    std::atomic<bool> is_window_visible = false;
    swan_path_t latest_valid_cwd = {};
    swan_path_t cwd = {}; // current working directory, persisted in file
    std::array<char, 256> filter = {}; // persisted in file
    bool filter_case_sensitive = false; // persisted in file
    bool filter_polarity = true; // persisted in file

    mutable s8 latest_save_to_disk_result = -1;
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

enum update_cwd_entries_actions : u8
{
    query_filesystem = 0b01, // 1
    filter           = 0b10, // 2
    full_refresh     = 0b11, // 3
};

bool update_cwd_entries(
    update_cwd_entries_actions actions,
    explorer_window *,
    std::string_view parent_dir,
    std::source_location sloc = std::source_location::current()) noexcept;

void new_history_from(explorer_window &expl, swan_path_t const &new_latest_entry);

struct pinned_path
{
    static u64 const LABEL_MAX_LEN = 64;

    ImVec4 color;
    boost::static_string<LABEL_MAX_LEN> label;
    swan_path_t path;
};

std::vector<pinned_path> &get_pins() noexcept;

bool pin(ImVec4 color, char const *label, swan_path_t &path, char dir_separator) noexcept;

void unpin(u64 pin_idx) noexcept;

void update_pin_dir_separators(char new_dir_separator) noexcept;

bool save_pins_to_disk() noexcept;

std::pair<bool, u64> load_pins_from_disk(char dir_separator) noexcept;

u64 find_pin_idx(swan_path_t const &) noexcept;

std::string get_last_error_string() noexcept;

bool save_focused_window(char const *window_name) noexcept;

bool load_focused_window_from_disk(char const *out) noexcept;

void imgui_sameline_spacing(u64 num_spacing_calls) noexcept;

struct bulk_rename_compiled_pattern
{
    struct op
    {
        enum class type : u8
        {
            insert_char,
            insert_name,
            insert_ext,
            insert_dotext,
            insert_size,
            insert_counter,
        };

        type kind;
        char ch = 0;

        bool operator!=(op const &other) const noexcept; // for ntest
        friend std::ostream& operator<<(std::ostream &os, op const &r); // for ntest
    };

    std::vector<op> ops;
    bool squish_adjacent_spaces;
};

struct bulk_rename_compile_pattern_result
{
    bool success;
    bulk_rename_compiled_pattern compiled_pattern;
    std::array<char, 256> error;
};

bulk_rename_compile_pattern_result bulk_rename_compile_pattern(char const *pattern, bool squish_adjacent_spaces) noexcept;

struct bulk_rename_transform_result
{
    bool success;
    std::array<char, 256> error;
};

bulk_rename_transform_result bulk_rename_transform(
    bulk_rename_compiled_pattern compiled_pattern,
    swan_path_t &after,
    char const *name,
    char const *ext,
    s32 counter,
    u64 bytes) noexcept;

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

// Slow function which allocates & deallocates memory. Cache the result, don't call this function every frame.
std::vector<bulk_rename_collision> bulk_rename_find_collisions(
    std::vector<explorer_window::dirent> &dest,
    std::vector<bulk_rename_op> const &renames) noexcept;

void swan_render_window_explorer(explorer_window &, windows_options &, bool &open) noexcept;

void swan_render_window_pinned_directories(std::array<explorer_window, 4> &, bool &open) noexcept;

void swan_render_window_debug_log(bool &open) noexcept;

void swan_render_window_file_operations() noexcept;

struct icon
{
    char const *name;
    char const *content;
};

std::vector<icon> const &get_font_awesome_icons() noexcept;
std::vector<icon> const &get_codicon_icons() noexcept;
std::vector<icon> const &get_material_design_icons() noexcept;

struct icon_browser
{
    char search_input[256];
    s32 grid_width;
    std::vector<icon> matches;
};

void swan_render_window_icon_browser(
    icon_browser &browser,
    bool &open,
    char const *icon_lib_name,
    char const *icon_prefix,
    std::vector<icon> const &(*get_all_icons)() noexcept) noexcept;

void swan_open_popup_modal_bulk_rename(
    explorer_window &,
    std::function<void ()> on_rename_finish_callback) noexcept;

char const *swan_id_bulk_rename_popup_modal() noexcept;
bool swan_is_popup_modal_open_bulk_rename() noexcept;
void swan_render_popup_modal_bulk_rename() noexcept;

void swan_open_popup_modal_error(char const *action, char const *failure) noexcept;
char const *swan_id_error_popup_modal() noexcept;
bool swan_is_popup_modal_open_error() noexcept;
void swan_render_popup_modal_error() noexcept;

void swan_open_popup_modal_single_rename(
    explorer_window &expl,
    explorer_window::dirent const &entry_to_be_renamed,
    std::function<void ()> on_rename_finish_callback) noexcept;

char const *swan_id_single_rename_popup_modal() noexcept;
bool swan_is_popup_modal_open_single_rename() noexcept;
void swan_render_popup_modal_single_rename() noexcept;

void swan_open_popup_modal_new_pin(swan_path_t const &init_path, bool mutable_path) noexcept;
char const *swan_id_new_pin_popup_modal() noexcept;
bool swan_is_popup_modal_open_new_pin() noexcept;
void swan_render_popup_modal_new_pin() noexcept;

char const *swan_id_edit_pin_popup_modal() noexcept;
void swan_open_popup_modal_edit_pin(pinned_path *pin) noexcept;
bool swan_is_popup_modal_open_edit_pin() noexcept;
void swan_render_popup_modal_edit_pin() noexcept;

void explorer_change_notif_thread_func(explorer_window &expl, std::atomic<s32> const &window_close_flag) noexcept;
