#pragma once

#include "stdafx.hpp"
#include "path.hpp"
#include "util.hpp"

template <typename ElemTy>
using circular_buffer = boost::circular_buffer<ElemTy>;

template <typename ElemTy, size_t Size>
using static_vector = boost::container::static_vector<ElemTy, Size>;

typedef BS::thread_pool swan_thread_pool_t;

struct generic_result
{
    bool success;
    // TODO: bool wrap_error;
    std::string error_or_utf8_path;
};

// TODO:
/*
  Requirements for the underlying data type which will replace std::array for swan_path_t:
  - has a .data() member function which returns a non-const char *
  - can easily be visualized as a string in the debugger
*/
typedef std::array<char, ((MAX_PATH - 1) * 4) + 1> swan_path_t;

struct basic_dirent
{
    enum class kind : s8 {
        nil = -1,
        directory,
        file,
        symlink_to_directory,
        symlink_to_file,
        invalid_symlink,
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
    bool is_symlink_to_file() const noexcept;
    bool is_symlink_to_directory() const noexcept;
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

    s32 window_x = 0, window_y = 0; //! must be adjacent, y must always come after x in memory
    s32 window_w = 1280, window_h = 720; //! must be adjacent, h must always come after w in memory
    s32 size_unit_multiplier = 1024;
    explorer_refresh_mode expl_refresh_mode = explorer_refresh_mode_automatic;
    wchar_t dir_separator_utf16 = L'\\';
    char dir_separator_utf8 = '\\';

    bool show_debug_info = false;
    bool show_dotdot_dir = false;
    bool cwd_entries_table_alt_row_bg = true;
    bool cwd_entries_table_borders_in_body = true;
    bool clear_filter_on_cwd_change = true;

    bool start_with_window_maximized = true;
    bool start_with_previous_window_pos_and_size = true;

    struct window_visibility
    {
        bool pin_manager = false;
        bool file_operations = false;
        bool explorer_0 = true;
        bool explorer_1 = false;
        bool explorer_2 = false;
        bool explorer_3 = false;
        bool analytics = false;
        bool debug_log = false;
        bool settings = false;
    #if !defined(NDEBUG)
        bool imgui_demo = false;
        bool fa_icons = false;
        bool ci_icons = false;
        bool md_icons = false;
    #endif
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
        basic_dirent basic;
        ptrdiff_t highlight_start_idx = 0;
        u64 highlight_len = 0;
        bool is_filtered_out = false;
        bool is_selected = false;
        bool is_cut = false;
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

    static u64 const NO_SELECTION = UINT64_MAX;
    static u64 const MAX_WD_HISTORY_SIZE = 15;
    bool save_to_disk() const noexcept;
    bool load_from_disk(char dir_separator) noexcept;
    void select_all_visible_cwd_entries(bool select_dotdot_dir = false) noexcept;
    void deselect_all_cwd_entries() noexcept;
    void invert_selected_visible_cwd_entries() noexcept;
    void set_latest_valid_cwd(swan_path_t const &new_latest_valid_cwd) noexcept;
    void uncut() noexcept;
    void reset_filter() noexcept;

    bool update_cwd_entries(
        update_cwd_entries_actions actions,
        std::string_view parent_dir,
        std::source_location sloc = std::source_location::current()) noexcept;

    void push_history_item(swan_path_t const &new_latest_entry) noexcept;

    // 104 byte alignment members
    static_vector<ImGuiTableColumnSortSpecs, cwd_entries_table_col_count> column_sort_specs;

    // 80 byte alignment members

    std::mutex shlwapi_task_initialization_mutex = {};

    // 72 byte alignment members

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
    std::string refresh_message = "";
    OVERLAPPED read_dir_changes_overlapped;

    // 24 byte alignment members

    std::vector<dirent> cwd_entries = {}; // 24 bytes, all direct children of the cwd

    // 8 byte alignment members

    char const *name = nullptr;
    filter_mode filter_mode = filter_mode::contains; // persisted in file
    u64 cwd_prev_selected_dirent_idx = NO_SELECTION; // idx of most recently clicked cwd entry, NO_SELECTION means there isn't one
    u64 wd_history_pos = 0; // where in wd_history we are, persisted in file
    // ImGuiTableSortSpecs *sort_specs = nullptr;
    HANDLE read_dir_changes_handle = INVALID_HANDLE_VALUE;

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
    //? mutable because they are debug counters

    // 4 byte alignment members

    s32 id = -1;
    DWORD read_dir_changes_buffer_bytes_written = 0;
    s32 frame_count_when_cwd_entries_updated = -1;
    std::array<std::byte, 64*1024> read_dir_changes_buffer = {};

    // 1 byte alignment members

    swan_path_t latest_valid_cwd = {};              // latest value of cwd which was a valid directory
    swan_path_t cwd = {};                           // current working directory, persisted in file
    swan_path_t read_dir_changes_target = {};       // value of current working directory when ReadDirectoryChangesW was called
    std::array<char, 256> filter_text = {};         // persisted in file
    bool filter_case_sensitive = false;             // persisted in file
    bool filter_polarity = true;                    // persisted in file
    bool filter_show_directories = true;            // persisted in file
    bool filter_show_files = true;                  // persisted in file
    bool filter_show_symlink_directories = true;    // persisted in file
    bool filter_show_symlink_files = true;          // persisted in file
    bool filter_show_invalid_symlinks = true;       // persisted in file
    // bool table_sort_specs_dirty = true;
    update_cwd_entries_actions update_request_from_outside = nil; // how code from outside the Begin()/End() of the explorer window
                                                                  // tells this explorer to update its cwd_entries

    mutable s8 latest_save_to_disk_result = -1;
};

struct file_operation_command_buf
{
    struct item
    {
        char const *operation;
        char operation_code;
        basic_dirent::kind type;
        swan_path_t path;
    };

    std::vector<item> items = {};

    void clear() noexcept;
    generic_result execute(explorer_window &expl) noexcept;
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

    std::atomic<u64> total_file_size          = {};
    std::atomic<u64> total_bytes_transferred  = {};
    std::atomic<u64> stream_size              = {};
    std::atomic<u64> stream_bytes_transferred = {};
    std::atomic<time_point_t> start_time      = {};
    std::atomic<time_point_t> end_time        = {};
    type op_type = type::nil;
    bool success = false;
    swan_path_t src_path = {};
    swan_path_t dest_path = {};

    file_operation(type op_type, u64 file_size, swan_path_t const &src, swan_path_t const &dst) noexcept;
    file_operation(file_operation const &other) noexcept; // for boost::circular_buffer
    file_operation &operator=(file_operation const &other) noexcept; // for boost::circular_buffer
};

struct progress_sink : public IFileOperationProgressSink
{
    HRESULT FinishOperations(HRESULT) override;
    HRESULT PauseTimer() override;
    HRESULT PostCopyItem(DWORD, IShellItem *, IShellItem *, LPCWSTR, HRESULT, IShellItem *) override;
    HRESULT PostDeleteItem(DWORD, IShellItem *, HRESULT, IShellItem *) override;
    HRESULT PostMoveItem(DWORD, IShellItem *, IShellItem *, LPCWSTR, HRESULT, IShellItem *) override;
    HRESULT PostNewItem(DWORD, IShellItem *, LPCWSTR, LPCWSTR, DWORD, HRESULT, IShellItem *) override;
    HRESULT PostRenameItem(DWORD, IShellItem *, LPCWSTR, HRESULT, IShellItem *) override;
    HRESULT PreCopyItem(DWORD, IShellItem *, IShellItem *, LPCWSTR) override;
    HRESULT PreDeleteItem(DWORD, IShellItem *) override;
    HRESULT PreMoveItem(DWORD, IShellItem *, IShellItem *, LPCWSTR) override;
    HRESULT PreNewItem(DWORD, IShellItem *, LPCWSTR) override;
    HRESULT PreRenameItem(DWORD, IShellItem *, LPCWSTR) override;
    HRESULT ResetTimer() override;
    HRESULT ResumeTimer() override;
    HRESULT StartOperations() override;
    HRESULT UpdateProgress(UINT work_total, UINT work_so_far) override;

    ULONG AddRef();
    ULONG Release();

    HRESULT QueryInterface(const IID &riid, void **ppv);
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

void perform_file_operations(
    std::wstring working_directory_utf16,
    std::wstring paths_to_execute_utf16,
    std::vector<char> operations_to_execute,
    std::mutex *init_done_mutex,
    std::condition_variable *init_done_cond,
    bool *init_done,
    std::string *init_error) noexcept;

struct pinned_path
{
    static u64 const LABEL_MAX_LEN = 64;

    ImVec4 color;
    boost::static_string<LABEL_MAX_LEN> label;
    swan_path_t path;
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
        u16 slice_last = 0;

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

struct icon_font_glyph
{
    char const *name;
    char const *content;
};

struct icon_font_browser_state
{
    char search_input[256];
    s32 grid_width;
    std::vector<icon_font_glyph> matches;
};
