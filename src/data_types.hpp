/// (Almost) all data types, defined in this file. The starting point for understanding how Swan works.
/// "Show me your data structures, and I won't usually need your code; it'll be obvious." - Fred Brooks
/// "Bad programmers worry about the code. Good programmers worry about data structures and their relationships." - Linus Torvalds

#pragma once

#include "stdafx.hpp"

template <typename ElemTy>
using circular_buffer = boost::circular_buffer<ElemTy>;

template <typename ElemTy, size_t Size>
using static_vector = boost::container::static_vector<ElemTy, Size>;

typedef BS::thread_pool swan_thread_pool_t;

#include "path.hpp"
#include "util.hpp"

inline ImVec4 default_success_color() noexcept { return ImVec4(0, 1, 0, 1); }
inline ImVec4 default_warning_color() noexcept { return ImVec4(1, 0.5f, 0, 1); }
inline ImVec4 default_error_color() noexcept { return ImVec4(1, 0, 0, 1); }
inline ImVec4 default_directory_color() noexcept { return ImVec4(1, 1, 0, 1); }
inline ImVec4 default_symlink_color() noexcept { return ImVec4(220/255.f, 189/255.f, 251/255.f, 1); }
inline ImVec4 default_file_color() noexcept { return ImVec4(0.85f, 1, 0.85f, 1); }

/// Bundle of state for a "progressive" task - a cancellable async function.
/// Provides a facility to safely query the result as the task progresses, and cancel the task.
template <typename Result>
struct progressive_task
{
    Result result = {};
    std::mutex result_mutex = {};
    std::atomic_bool cancellation_token = false;
    std::atomic_bool active_token = false;
};

struct generic_result
{
    bool success;
    // TODO: bool wrap_error;
    std::string error_or_utf8_path;
};

struct winapi_error
{
    DWORD code;
    std::string formatted_message;
};

struct swan_path final : std::array<char, ((MAX_PATH - 1) * 4) + 1>
{
    // static swan_path create(char const *data, u64 count = u64(-1)) noexcept;

    // u64 length() noexcept;

    // bool ends_with(char const *end) noexcept;

    // bool ends_with_one_of(char const *chars) noexcept;

    // bool is_empty() noexcept;

    // void clear() noexcept;

    // void convert_separators(char new_dir_separator) noexcept;

    // char pop_back() noexcept;
    // bool pop_back_if(char if_ch) noexcept;
    // bool pop_back_if_one_of(char const *chars_list) noexcept;
    // bool pop_back_if_not(char if_not_ch) noexcept;

    // u64 append(char const *append_data, char dir_separator = 0, bool prepend_slash = false, bool postpend_slash = false) noexcept;

    // swan_path reconstruct_with_squished_separators() noexcept;

    // swan_path reconstruct_canonically(char dir_sep_utf8) noexcept;

    bool operator>(swan_path const &other) const noexcept { return strcmp(this->data(), other.data()) > 0; }
};

struct basic_dirent
{
    enum class kind : s8 {
        nil,
        directory,
        file,
        symlink_to_directory,
        symlink_to_file,
        symlink_ambiguous,
        invalid_symlink,
        count
    };

    u64 size = 0;
    FILETIME creation_time_raw = {};
    FILETIME last_write_time_raw = {};
    u32 id = {};
    kind type = kind::nil;
    swan_path path = {};

    bool is_path_dotdot() const noexcept;
    bool is_dotdot_dir() const noexcept;
    bool is_directory() const noexcept;
    bool is_symlink() const noexcept;
    bool is_symlink_to_file() const noexcept;
    bool is_symlink_to_directory() const noexcept;
    bool is_symlink_ambiguous() const noexcept;
    bool is_file() const noexcept;
    char const *kind_cstr() const noexcept;
    char const *kind_short_cstr() const noexcept;
    char const *kind_icon() const noexcept;
};

struct drive_info
{
    u64 total_bytes;
    u64 available_bytes;
    char name_utf8[512];
    char filesystem_name_utf8[512];
    char letter;
};

typedef static_vector<drive_info, 26> drive_list_t;

struct recycle_bin_info
{
    HRESULT result;
    s64 bytes_used;
    s64 num_items;
};

struct swan_settings
{
    bool save_to_disk() const noexcept;
    bool load_from_disk() noexcept;

    enum explorer_refresh_mode : s32
    {
        explorer_refresh_mode_automatic,
        explorer_refresh_mode_notify,
        explorer_refresh_mode_manual,
        explorer_refresh_mode_count
    };

    ImVec4 success_color = default_success_color();
    ImVec4 warning_color = default_warning_color();
    ImVec4 error_color = default_error_color();
    ImVec4 directory_color = default_directory_color();
    ImVec4 file_color = default_file_color();
    ImVec4 symlink_color = default_symlink_color();

    s32 window_x = 10, window_y = 40; //! must be adjacent, y must come after x in memory
    s32 window_w = 1280, window_h = 720; //! must be adjacent, h must come after w in memory
    s32 size_unit_multiplier = 1024;
    explorer_refresh_mode explorer_refresh_mode = explorer_refresh_mode_automatic;
    wchar_t dir_separator_utf16 = L'\\';
    char dir_separator_utf8 = '\\';

    bool show_debug_info = false;
    bool explorer_show_dotdot_dir = false;
    bool explorer_cwd_entries_table_alt_row_bg = true;
    bool explorer_cwd_entries_table_borders_in_body = true;
    bool explorer_clear_filter_on_cwd_change = true;

    bool startup_with_window_maximized = true;
    bool startup_with_previous_window_pos_and_size = true;

    bool confirm_explorer_delete_via_keybind = true;
    bool confirm_explorer_delete_via_context_menu = true;
    bool confirm_explorer_unpin_directory = true;
    bool confirm_recent_files_clear = true;
    bool confirm_recent_files_reveal_selected_in_win_file_expl = true;
    bool confirm_recent_files_forget_selected = true;
    bool confirm_delete_pin = true;
    bool confirm_completed_file_operations_forget_single = true;
    bool confirm_completed_file_operations_forget_group = true;
    bool confirm_completed_file_operations_forget_selected = true;
    bool confirm_completed_file_operations_forget_all = true;
    bool confirm_theme_editor_color_reset = true;
    bool confirm_theme_editor_style_reset = true;

    struct window_visibility
    {
        bool explorer_0 = true;
        bool explorer_1 = false;
        bool explorer_2 = false;
        bool explorer_3 = false;
        bool finder = false;
        bool pinned = false;
        bool file_operations = false;
        bool recent_files = false;
        bool analytics = false;
        bool debug_log = false;
        bool settings = false;
        bool imgui_demo = false;
        bool theme_editor = false;
        bool icon_library = false;
        bool fa_icons = false;
        bool ci_icons = false;
        bool md_icons = false;
    };

    window_visibility show;
};

enum update_cwd_entries_actions : u8
{
    nil              = 0b00, // 0
    query_filesystem = 0b01, // 1
    filter           = 0b10, // 2
    full_refresh     = 0b11, // 3
};

struct explorer_window
{
    struct dirent
    {
        basic_dirent basic = {};
        ptrdiff_t highlight_start_idx = 0;
        u64 highlight_len = 0;
        u32 spotlight_frames_remaining = 0;
        bool is_filtered_out = false;
        bool is_selected = false;
        bool is_cut = false;
        bool context_menu_active = false;
    };

    enum filter_mode : u64
    {
        contains = 0,
        regex_match,
        // glob,
        count,
    };

    enum cwd_entries_table_col : ImGuiID
    {
        cwd_entries_table_col_number,
        cwd_entries_table_col_id,
        cwd_entries_table_col_path,
        cwd_entries_table_col_type,
        cwd_entries_table_col_size_pretty,
        cwd_entries_table_col_size_bytes,
        cwd_entries_table_col_creation_time,
        cwd_entries_table_col_last_write_time,
        cwd_entries_table_col_count
    };

    typedef static_vector<ImGuiTableColumnSortSpecs, cwd_entries_table_col_count> cwd_entries_column_sort_specs_t;

    static u64 const NO_SELECTION = UINT64_MAX;
    static u64 const MAX_WD_HISTORY_SIZE = 100;
    bool save_to_disk() const noexcept;
    bool load_from_disk(char dir_separator) noexcept;
    void select_all_visible_cwd_entries(bool select_dotdot_dir = false) noexcept;
    u64 deselect_all_cwd_entries() noexcept;
    void invert_selection_on_visible_cwd_entries() noexcept;
    void set_latest_valid_cwd(swan_path const &new_latest_valid_cwd) noexcept;
    void uncut() noexcept;
    void reset_filter() noexcept;
    cwd_entries_column_sort_specs_t copy_column_sort_specs(ImGuiTableSortSpecs const *sort_specs) noexcept;

    struct update_cwd_entries_result
    {
        bool parent_dir_exists;
        u64 num_entries_selected;
    };

    update_cwd_entries_result update_cwd_entries(
        update_cwd_entries_actions actions,
        std::string_view parent_dir,
        std::source_location sloc = std::source_location::current()) noexcept;

    void push_history_item(swan_path const &new_latest_entry) noexcept;

    // 104 byte alignment members

    cwd_entries_column_sort_specs_t column_sort_specs;

    // 80 byte alignment members

    std::mutex shlwapi_task_initialization_mutex = {};
    std::mutex select_cwd_entries_on_next_update_mutex = {};

    // 72 byte alignment members

    std::condition_variable shlwapi_task_initialization_cond = {};

    // 40 byte alignment members

    // struct history_item
    // {
    //     system_time_point_t? time_departed;
    //     swan_path_t path;
    // };

    // history for working directories, persisted in file
    circular_buffer<swan_path> wd_history = circular_buffer<swan_path>(MAX_WD_HISTORY_SIZE);

    // 32 byte alignment members

    std::string filter_error = "";
    std::string refresh_message = "";
    std::string refresh_message_tooltip = "";
    OVERLAPPED read_dir_changes_overlapped;

    // 24 byte alignment members

    std::vector<dirent> cwd_entries = {};                             // all direct children of the cwd
    std::vector<swan_path> select_cwd_entries_on_next_update = {};  // entries to select on the next update of cwd_entries

    // 8 byte alignment members

    char const *name = nullptr;
    filter_mode filter_mode = filter_mode::contains;    // persisted in file
    u64 cwd_latest_selected_dirent_idx = u64(-1);       // idx of most recently clicked cwd entry
    u64 wd_history_pos = 0;                             // where in wd_history we are, persisted in file
    u64 nth_last_cwd_dirent_scrolled = u64(-1);
    u64 scroll_to_nth_selected_entry_next_frame = u64(-1);
    HANDLE read_dir_changes_handle = INVALID_HANDLE_VALUE;
    precise_time_point_t read_dir_changes_refresh_request_time = {};
    precise_time_point_t last_filesystem_query_time = {};
    dirent *context_menu_target = nullptr;

    static u64 const NUM_TIMING_SAMPLES = 10;

    struct update_cwd_entries_timers
    {
        f64 total_us = 0;
        f64 searchpath_setup_us = 0;
        f64 filesystem_us = 0;
        f64 filter_us = 0;
        f64 regex_ctor_us = 0;
        f64 entries_to_select_sort = 0;
        f64 entries_to_select_search = 0;
    };

    mutable circular_buffer<update_cwd_entries_timers> update_cwd_entries_timing_samples = circular_buffer<update_cwd_entries_timers>(NUM_TIMING_SAMPLES);
    mutable circular_buffer<f64> sort_timing_samples                                     = circular_buffer<f64>(NUM_TIMING_SAMPLES);
    mutable circular_buffer<f64> save_to_disk_timing_samples                             = circular_buffer<f64>(NUM_TIMING_SAMPLES);
    mutable circular_buffer<f64> find_first_filtered_cwd_dirent_timing_samples           = circular_buffer<f64>(NUM_TIMING_SAMPLES);

    mutable u64 num_file_finds = 0;
    mutable f64 check_if_pinned_us = 0;
    mutable f64 unpin_us = 0;

    //? mutable because they are debug counters/timers

    // 4 byte alignment members

    s32 id = -1;
    DWORD read_dir_changes_buffer_bytes_written = 0;
    s32 frame_count_when_cwd_entries_updated = -1;
    std::array<std::byte, 64*1024> read_dir_changes_buffer = {};

    // 1 byte alignment members

    swan_path latest_valid_cwd = {};              // latest value of cwd which was a valid directory
    swan_path cwd = {};                           // current working directory, persisted in file
    swan_path read_dir_changes_target = {};       // value of current working directory when ReadDirectoryChangesW was called
    std::array<char, 256> filter_text = {};         // persisted in file
    bool filter_case_sensitive = false;             // persisted in file
    bool filter_polarity = true;                    // persisted in file
    bool filter_show_directories = true;            // persisted in file
    bool filter_show_files = true;                  // persisted in file
    bool filter_show_symlink_directories = true;    // persisted in file
    bool filter_show_symlink_files = true;          // persisted in file
    bool filter_show_invalid_symlinks = true;       // persisted in file

    bool show_filter_window = false;
    bool cwd_latest_selected_dirent_idx_changed = false;
    bool highlight = false;

    update_cwd_entries_actions update_request_from_outside = nil; /* how code from outside the Begin()/End() of the explorer window
                                                                     signals to the explorer to call update_cwd_entries */

    mutable s8 latest_save_to_disk_result = -1;
};

struct finder_window
{
    struct search_directory
    {
        bool found;
        swan_path path_utf8;
    };

    struct match
    {
        basic_dirent basic = {};
        char const *file_name = nullptr;
        ptrdiff_t highlight_start_idx = 0;
        u64 highlight_len = 0;
    };

    progressive_task<std::vector<finder_window::match>> search_task = {};
    std::array<char, 1024> search_value = {};
    std::vector<search_directory> search_directories = {};
    std::atomic<u64> num_entries_checked = 0;
    bool detailed_symlinks = false;
    bool focus_search_value_input = false;
};

struct symlink_data
{
    s32 show_cmd;
    wchar_t target_path_utf16[MAX_PATH];
    swan_path target_path_utf8;
    wchar_t working_directory_path_utf16[MAX_PATH];
    wchar_t arguments_utf16[1024];

    generic_result extract(char const *lnk_file_path_utf8, char const *cwd = nullptr) noexcept;
};

enum class file_operation_type : char
{
    nil = '\0',
    move = 'M',
    copy = 'C',
    del = 'D',
};

struct file_operation_command_buf
{
    struct item
    {
        char const *operation_desc;
        file_operation_type operation_type;
        basic_dirent::kind type;
        swan_path path;
    };

    std::vector<item> items = {};

    void clear() noexcept;
    generic_result execute(explorer_window &expl) noexcept;
};

struct completed_file_operation
{
    system_time_point_t completion_time = {};
    system_time_point_t undo_time = {};
    u32 group_id = {};
    swan_path src_path = {};
    swan_path dst_path = {};
    file_operation_type op_type = file_operation_type::nil;
    basic_dirent::kind obj_type = basic_dirent::kind::nil;
    bool selected = false;

    bool undone() noexcept { return undo_time != system_time_point_t(); }

    completed_file_operation(system_time_point_t completion_time, system_time_point_t undo_time, file_operation_type op_type,
                             char const *src, char const *dst, basic_dirent::kind obj_type, u32 group_id = 0) noexcept;

    completed_file_operation() noexcept;
    completed_file_operation(completed_file_operation const &other) noexcept;
    completed_file_operation &operator=(completed_file_operation const &other) noexcept;
};

struct explorer_file_op_progress_sink : public IFileOperationProgressSink
{
    u32 group_id;
    s32 dst_expl_id;
    swan_path dst_expl_cwd_when_operation_started;
    bool contains_delete_operations;

    HRESULT PauseTimer() noexcept override;
    HRESULT ResetTimer() noexcept override;
    HRESULT ResumeTimer() noexcept override;
    HRESULT StartOperations() noexcept override;
    HRESULT FinishOperations(HRESULT) noexcept override;
    HRESULT UpdateProgress(UINT work_total, UINT work_so_far) noexcept override;

    HRESULT PreCopyItem(DWORD, IShellItem *, IShellItem *, LPCWSTR) noexcept override;
    HRESULT PreDeleteItem(DWORD, IShellItem *) noexcept override;
    HRESULT PreMoveItem(DWORD, IShellItem *, IShellItem *, LPCWSTR) noexcept override;
    HRESULT PreNewItem(DWORD, IShellItem *, LPCWSTR) noexcept override;
    HRESULT PreRenameItem(DWORD, IShellItem *, LPCWSTR) noexcept override;

    HRESULT PostCopyItem(DWORD, IShellItem *, IShellItem *, LPCWSTR, HRESULT, IShellItem *) noexcept override;
    HRESULT PostDeleteItem(DWORD, IShellItem *, HRESULT, IShellItem *) noexcept override;
    HRESULT PostMoveItem(DWORD, IShellItem *, IShellItem *, LPCWSTR, HRESULT, IShellItem *) noexcept override;
    HRESULT PostNewItem(DWORD, IShellItem *, LPCWSTR, LPCWSTR, DWORD, HRESULT, IShellItem *) noexcept override;
    HRESULT PostRenameItem(DWORD, IShellItem *, LPCWSTR, HRESULT, IShellItem *) noexcept override;

    ULONG AddRef()  noexcept;
    ULONG Release() noexcept;

    HRESULT QueryInterface(const IID &riid, void **ppv) noexcept;
};

struct undelete_directory_progress_sink : public IFileOperationProgressSink
{
    swan_path destination_full_path_utf8;

    HRESULT PauseTimer() noexcept override;
    HRESULT ResetTimer() noexcept override;
    HRESULT ResumeTimer() noexcept override;
    HRESULT StartOperations() noexcept override;
    HRESULT FinishOperations(HRESULT) noexcept override;
    HRESULT UpdateProgress(UINT work_total, UINT work_so_far) noexcept override;

    HRESULT PreCopyItem(DWORD, IShellItem *, IShellItem *, LPCWSTR) noexcept override;
    HRESULT PreDeleteItem(DWORD, IShellItem *) noexcept override;
    HRESULT PreMoveItem(DWORD, IShellItem *, IShellItem *, LPCWSTR) noexcept override;
    HRESULT PreNewItem(DWORD, IShellItem *, LPCWSTR) noexcept override;
    HRESULT PreRenameItem(DWORD, IShellItem *, LPCWSTR) noexcept override;

    HRESULT PostCopyItem(DWORD, IShellItem *, IShellItem *, LPCWSTR, HRESULT, IShellItem *) noexcept override;
    HRESULT PostDeleteItem(DWORD, IShellItem *, HRESULT, IShellItem *) noexcept override;
    HRESULT PostMoveItem(DWORD, IShellItem *, IShellItem *, LPCWSTR, HRESULT, IShellItem *) noexcept override;
    HRESULT PostNewItem(DWORD, IShellItem *, LPCWSTR, LPCWSTR, DWORD, HRESULT, IShellItem *) noexcept override;
    HRESULT PostRenameItem(DWORD, IShellItem *, LPCWSTR, HRESULT, IShellItem *) noexcept override;

    ULONG AddRef() noexcept;
    ULONG Release() noexcept;

    HRESULT QueryInterface(const IID &riid, void **ppv) noexcept;
};

struct move_dirents_drag_drop_payload
{
    s32 src_explorer_id;
    u64 num_items;
    wchar_t *absolute_paths_delimited_by_newlines;
};

struct pin_drag_drop_payload
{
    u64 pin_idx;
};

struct pinned_path
{
    static u64 const LABEL_MAX_LEN = 64;

    ImVec4 color;
    boost::static_string<LABEL_MAX_LEN> label;
    swan_path path;
};

struct recent_file
{
    static u64 const ACTION_MAX_LEN = 64;

    boost::static_string<ACTION_MAX_LEN> action;
    system_time_point_t action_time;
    swan_path path;
    bool selected;
    bool context_menu_active;
};

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
            insert_slice,
        };

        type kind;
        char ch = 0;
        bool explicit_first = false;
        bool explicit_last = false;
        u16 slice_first = 0;
        u16 slice_last = UINT16_MAX;

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

struct bulk_rename_transform_result
{
    bool success;
    std::array<char, 256> error;
};

struct bulk_rename_op
{
    basic_dirent *before = nullptr;
    swan_path after;
    std::atomic_char result = 0;

    bulk_rename_op(basic_dirent *before, char const *after) noexcept;

    bulk_rename_op(bulk_rename_op const &other) noexcept; // for emplace_back
    bulk_rename_op &operator=(bulk_rename_op const &other) noexcept; // for emplace_back

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

struct bulk_rename_find_collisions_result
{
    std::vector<bulk_rename_collision> collisions;
    std::vector<bulk_rename_op> sorted_renames;
};

struct icon_font_glyph
{
    char const *name = nullptr;
    char const *content = nullptr;
    u64 lev_edit_distance = 0;
};

struct icon_font_browser_state
{
    char search_input[256];
    s32 grid_width;
    std::vector<icon_font_glyph> matches;
};

enum swan_confirmation_id : s32
{
    swan_id_confirm_recent_files_clear,
    swan_id_confirm_recent_files_reveal_selected_in_win_file_expl,
    swan_id_confirm_recent_files_forget_selected,

    swan_id_confirm_delete_pin,

    swan_id_confirm_explorer_execute_delete,
    swan_id_confirm_explorer_unpin_directory,

    swan_id_confirm_completed_file_operations_forget_single,
    swan_id_confirm_completed_file_operations_forget_group,
    swan_id_confirm_completed_file_operations_forget_selected,
    swan_id_confirm_completed_file_operations_forget_all,

    swan_id_confirm_theme_editor_color_reset,
    swan_id_confirm_theme_editor_style_reset,

    swan_id_confirm_count
};
