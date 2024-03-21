/*
    A common place for functions which are not tied to the existence of ImGui.
*/

#pragma once

#include "stdafx.hpp"
#include "path.hpp"
#include "util.hpp"
#include "data_types.hpp"

namespace global_constants
{
    std::vector<icon_font_glyph> const &icon_font_glyphs_font_awesome() noexcept;
    std::vector<icon_font_glyph> const &icon_font_glyphs_codicon() noexcept;
    std::vector<icon_font_glyph> const &icon_font_glyphs_material_design() noexcept;

    constexpr u64 num_explorers = 4;
    constexpr u64 MAX_RECENT_FILES = 100;

} // global_constants

namespace global_state
{
    std::pair<circular_buffer<recent_file> *, std::mutex *> recent_files() noexcept;
    u64 find_recent_file_idx(char const *search_path) noexcept;
    void move_recent_file_idx_to_front(u64 recent_file_idx, char const *new_action = nullptr) noexcept;
    void add_recent_file(char const *action, char const *full_file_path) noexcept;
    void remove_recent_file(u64 recent_file_idx) noexcept;
    bool save_recent_files_to_disk() noexcept;
    std::pair<bool, u64> load_recent_files_from_disk(char dir_separator) noexcept;

    std::vector<pinned_path> &pins() noexcept;
    bool add_pin(ImVec4 color, char const *label, swan_path &path, char dir_separator) noexcept;
    void remove_pin(u64 pin_idx) noexcept;
    void update_pin_dir_separators(char new_dir_separator) noexcept;
    u64 find_pin_idx(swan_path const &) noexcept;
    void swap_pins(u64 pin1_idx, u64 pin2_idx) noexcept;
    bool save_pins_to_disk() noexcept;
    std::pair<bool, u64> load_pins_from_disk(char dir_separator) noexcept;

    std::pair<std::deque<completed_file_operation> *, std::mutex *> completed_file_ops() noexcept;
    bool save_completed_file_ops_to_disk(std::scoped_lock<std::mutex> *lock) noexcept;
    std::pair<bool, u64> load_completed_file_ops_from_disk(char dir_separator) noexcept;
    u32 next_group_id() noexcept;

    s32 focused_window() noexcept;
    bool save_focused_window(s32 window_code) noexcept;
    bool load_focused_window_from_disk(s32 &window_code) noexcept;

    bool &move_dirents_payload_set() noexcept;

    s32 &debug_log_text_limit_megabytes() noexcept;

    std::filesystem::path &execution_path() noexcept;

    swan_thread_pool_t &thread_pool() noexcept;

    swan_settings &settings() noexcept;

    std::array<explorer_window, global_constants::num_explorers> &explorers() noexcept;

    s32 &page_size() noexcept;

    bool any_popup_modals_open() noexcept;
    u64 &popup_modals_open_bit_field() noexcept;

} // namespace global_state

namespace swan_windows
{
    enum window_id : s32 {
        nil_window = 0,
        explorer_0,
        explorer_1,
        explorer_2,
        explorer_3,
        finder,
        pinned,
        file_operations,
        recent_files,
        analytics,
        debug_log,
        settings,
        theme_editor,
        icon_library,
        icon_font_browser_font_awesome,
        icon_font_browser_codicon,
        icon_font_browser_material_design,
        imgui_demo,
        count
    };

    char const *get_name(s32 id) noexcept
    {
        switch (id) {
            case explorer_0: return " Explorer 1 ";
            case explorer_1: return " Explorer 2 ";
            case explorer_2: return " Explorer 3 ";
            case explorer_3: return " Explorer 4 ";
            case finder: return " Finder ";
            case pinned: return " Pinned ";
            case file_operations: return " File Operations ";
            case recent_files: return " Recent Files ";
            case analytics: return " Analytics ";
            case debug_log: return " Debug Log ";
            case settings: return " Settings ";
            case theme_editor: return " Theme Editor ";
            case icon_library: return " Icon Library ";
            case icon_font_browser_font_awesome: return " Font Awesome Icons ";
            case icon_font_browser_codicon: return " Codicon Icons ";
            case icon_font_browser_material_design: return " Material Design Icons ";
            case imgui_demo: return " ImGui Demo ";
            default: assert(false && "Window has no name"); return nullptr;
        }
    }

    void render_explorer(explorer_window &, bool &open, finder_window &) noexcept;

    void render_finder(finder_window &, bool &open) noexcept;

    void render_pin_manager(std::array<explorer_window, 4> &, bool &open) noexcept;

    void render_debug_log(bool &open) noexcept;

    void render_file_operations(bool &open) noexcept;

    void render_recent_files(bool &open) noexcept;

    void render_settings(GLFWwindow *window) noexcept;

    void render_icon_font_browser(
        s32 window_code,
        icon_font_browser_state &browser,
        bool &open,
        char const *icon_lib_name,
        char const *icon_prefix,
        std::vector<icon_font_glyph> const &(*get_all_icons)() noexcept) noexcept;

    void render_icon_library(bool &open) noexcept;

    void render_theme_editor(bool &open, ImVec4 *starting_colors) noexcept;

} // namespace render_window

namespace swan_popup_modals
{
    enum bit_pos : u64
    {
        bit_pos_error = 0,
        bit_pos_single_rename,
        bit_pos_bulk_rename,
        bit_pos_new_pin,
        bit_pos_edit_pin,
        bit_pos_new_file,
        bit_pos_new_directory,
        bit_pos_count
    };

    static_assert(bit_pos_count <= 64);

    constexpr char const *label_error = "Error ## popup_modal";
    constexpr char const *label_single_rename = "Rename ## popup_modal";
    constexpr char const *label_bulk_rename = "Bulk Rename ## popup_modal";
    constexpr char const *label_new_pin = "New Pin ## popup_modal";
    constexpr char const *label_edit_pin = "Edit Pin ## popup_modal";
    constexpr char const *label_new_file = "New File ## popup_modal";
    constexpr char const *label_new_directory = "New Directory ## popup_modal";

    void open_single_rename(explorer_window &expl_opened_from, explorer_window::dirent const &rename_target, std::function<void ()> on_rename_callback) noexcept;
    void open_bulk_rename(explorer_window &expl_opened_from, std::function<void ()> on_rename_callback) noexcept;
    void open_error(char const *action, char const *failure, bool beautify_action = false, bool beautify_failure = false) noexcept;
    void open_new_pin(swan_path const &initial_path_value, bool mutable_path) noexcept;
    void open_edit_pin(pinned_path *pin) noexcept;
    void open_new_file(char const *parent_directory_utf8, s32 initiating_expl_id = -1) noexcept;
    void open_new_directory(char const *parent_directory_utf8, s32 initiating_expl_id = -1) noexcept;

    bool is_open_single_rename() noexcept;
    bool is_open_bulk_rename() noexcept;
    bool is_open_error() noexcept;
    bool is_open_new_pin() noexcept;
    bool is_open_edit_pin() noexcept;
    bool is_open_new_file() noexcept;
    bool is_open_new_directory() noexcept;

    void render_single_rename() noexcept;
    void render_bulk_rename() noexcept;
    void render_error() noexcept;
    void render_new_pin() noexcept;
    void render_edit_pin() noexcept;
    void render_new_file() noexcept;
    void render_new_directory() noexcept;

} // namespace swan_popup_modals

void init_COM_for_explorers(GLFWwindow *window, char const *ini_file_path) noexcept;

void clean_COM_for_explorers() noexcept;

void apply_swan_style_overrides() noexcept;

char const *get_icon(basic_dirent::kind t) noexcept;

char const *get_icon_for_extension(char const *extension) noexcept;

drive_list_t query_drive_list() noexcept;

recycle_bin_info query_recycle_bin() noexcept;

generic_result open_file(char const *file_name, char const *file_directory, bool as_admin = false) noexcept;

generic_result reveal_in_windows_file_explorer(swan_path const &full_path) noexcept;

winapi_error get_last_winapi_error() noexcept;

void perform_file_operations(
    s32 dst_expl_id,
    std::wstring working_directory_utf16,
    std::wstring paths_to_execute_utf16,
    std::vector<file_operation_type> operations_to_execute,
    std::mutex *init_done_mutex,
    std::condition_variable *init_done_cond,
    bool *init_done,
    std::string *init_error) noexcept;

bulk_rename_compile_pattern_result bulk_rename_compile_pattern(char const *pattern, bool squish_adjacent_spaces) noexcept;

bulk_rename_transform_result bulk_rename_transform(
    bulk_rename_compiled_pattern compiled_pattern,
    swan_path &after,
    char const *name,
    char const *ext,
    s32 counter,
    u64 bytes) noexcept;

void sort_renames_dup_elem_sequences_after_non_dups(std::vector<bulk_rename_op> &renames) noexcept;

// Slow function which allocates & deallocates memory. Cache the result, don't call this function every frame.
bulk_rename_find_collisions_result bulk_rename_find_collisions(
    std::vector<explorer_window::dirent> &dest,
    std::vector<bulk_rename_op> const &renames) noexcept;
