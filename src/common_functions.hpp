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
    std::vector<icon_font_glyph> const &icon_font_glyphs_FontAwesome5() noexcept;
    std::vector<icon_font_glyph> const &icon_font_glyphs_codicon() noexcept;
    std::vector<icon_font_glyph> const &icon_font_glyphs_lucide() noexcept;

    constexpr u64 num_explorers = 4;
    constexpr u64 MAX_RECENT_FILES = 100;

} // global_constants

namespace swan_windows
{
    enum class id : s32 {
        nil_window = 0,
        explorer_0,
        explorer_1,
        explorer_2,
        explorer_3,
        explorer_0_debug,
        explorer_1_debug,
        explorer_2_debug,
        explorer_3_debug,
        finder,
        pinned,
        file_operations,
        recent_files,
        analytics,
        debug_log,
        settings,
        theme_editor,
        icon_library,
        imgui_demo,
        imspinner_demo,
        count
    };

    inline char const *get_name(id id) noexcept
    {
        switch (id) {
            case id::explorer_0: return " Explorer 1 ";
            case id::explorer_1: return " Explorer 2 ";
            case id::explorer_2: return " Explorer 3 ";
            case id::explorer_3: return " Explorer 4 ";
            case id::explorer_0_debug: return " Explorer 1 Debug ";
            case id::explorer_1_debug: return " Explorer 2 Debug ";
            case id::explorer_2_debug: return " Explorer 3 Debug ";
            case id::explorer_3_debug: return " Explorer 4 Debug ";
            case id::finder: return " Finder ";
            case id::pinned: return " Bookmarks ";
            case id::file_operations: return " File Operations ";
            case id::recent_files: return " Recent Files ";
            case id::analytics: return " Analytics ";
            case id::debug_log: return " Debug Log ";
            case id::settings: return " Advanced Settings ";
            case id::theme_editor: return " Theme Editor ";
            case id::icon_library: return " Icon Library ";
            case id::imgui_demo: return " ImGui Demo ";
            case id::imspinner_demo: return " ImSpinner Demo ";
            default: assert(false && "Window has no name"); return nullptr;
        }
    }

    //? render_* with return type bool call and return result of imgui::Begin

    bool render_explorer(explorer_window &, bool &open, finder_window &, bool any_popups_open) noexcept;

    bool render_explorer_debug(explorer_window &, bool &open, bool any_popups_open) noexcept;

    bool render_finder(finder_window &, bool &open, bool any_popups_open) noexcept;

    std::pair<pinned_path *, bool> render_pinned(pinned_path *s_context_target, bool is_popup_modal) noexcept;

    bool render_debug_log(bool &open, bool any_popups_open) noexcept;

    bool render_file_operations(bool &open, bool any_popups_open) noexcept;

    bool render_recent_files(bool &open, bool any_popups_open) noexcept;

    bool render_settings(bool &open, bool any_popups_open, bool &changes_applied) noexcept;

    bool render_icon_library(bool &open, bool any_popups_open) noexcept;

    bool render_theme_editor(bool &open, ImGuiStyle const &fallback_style, bool any_popups_open) noexcept;

    bool render_analytics(std::array<swan_windows::id, (u64)swan_windows::id::count - 1> const &window_render_order) noexcept;

    bool render_imspinner_demo(bool &open, bool any_popups_open) noexcept;

}

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

    constexpr char const *label_error = " Error ## popup_modal";
    constexpr char const *label_single_rename = " Rename ## popup_modal";
    constexpr char const *label_bulk_rename = " Bulk Rename ## popup_modal";
    constexpr char const *label_new_pin = " New Bookmark ## popup_modal";
    constexpr char const *label_edit_pin = " Edit Bookmark ## popup_modal";
    constexpr char const *label_new_file = " New File ## popup_modal";
    constexpr char const *label_new_directory = " New Directory ## popup_modal";

    void open_single_rename(explorer_window &expl_opened_from, explorer_window::dirent const &rename_target, std::function<void ()> on_rename_callback) noexcept;
    void open_bulk_rename(explorer_window &expl_opened_from, std::function<void ()> on_rename_callback) noexcept;
    void open_error(char const *action, char const *failure, bool beautify_action = false, bool beautify_failure = false) noexcept;
    void open_new_pin(swan_path const &initial_path_value, bool mutable_path) noexcept;
    void open_edit_pin(pinned_path *pin) noexcept;
    void open_new_file(char const *parent_directory_utf8, s32 initiating_expl_id = -1) noexcept;
    void open_new_directory(char const *parent_directory_utf8, s32 initiating_expl_id = -1) noexcept;

    void render_single_rename() noexcept;
    void render_bulk_rename() noexcept;
    void render_error() noexcept;
    void render_new_pin() noexcept;
    void render_edit_pin() noexcept;
    void render_new_file() noexcept;
    void render_new_directory() noexcept;

} // namespace swan_popup_modals

namespace global_state
{
    // Bundles of global state are accessible through this API.
    // Some globals are used in a multiple threads, their getters provide a mutex.
    // Globals which are only used in the main thread do not provide a mutex through their getter.

    struct recent_files
    {
        std::deque<recent_file> *container;
        std::mutex              *mutex;
    };
    recent_files            recent_files_get() noexcept;
    void                    recent_files_update(char const *action, char const *full_file_path) noexcept;
    u64                     recent_files_find_idx(char const *search_path) noexcept;
    void                    recent_files_move_to_front(u64 recent_file_idx, char const *new_action = nullptr) noexcept;
    void                    recent_files_add(char const *action, char const *full_file_path) noexcept;
    void                    recent_files_remove(u64 recent_file_idx) noexcept;
    bool                    recent_files_save_to_disk(std::scoped_lock<std::mutex> *supplied_lock) noexcept;
    std::pair<bool, u64>    recent_files_load_from_disk(char dir_separator) noexcept;

    struct completed_file_operations
    {
        std::deque<completed_file_operation> *container;
        std::mutex                           *mutex;
    };
    completed_file_operations   completed_file_operations_get() noexcept;
    std::pair<bool, u64>        completed_file_operations_load_from_disk(char dir_separator) noexcept;
    u32                         completed_file_operations_calc_next_group_id() noexcept;
    bool                        completed_file_operations_save_to_disk(std::scoped_lock<std::mutex> *lock) noexcept;

    std::vector<pinned_path> &  pinned_get() noexcept;
    std::pair<bool, u64>        pinned_load_from_disk(char override_dir_separator) noexcept;
    u64                         pinned_find_idx(swan_path const &) noexcept;
    bool                        pinned_add(ImVec4 color, char const *label, swan_path &path, char dir_separator) noexcept;
    bool                        pinned_save_to_disk() noexcept;
    void                        pinned_remove(u64 pin_idx) noexcept;
    void                        pinned_update_directory_separators(char new_dir_separator) noexcept;
    void                        pinned_swap(u64 pin1_idx, u64 pin2_idx) noexcept;

    HWND &                  window_handle() noexcept;
    std::filesystem::path & execution_path() noexcept;
    swan_thread_pool_t &    thread_pool() noexcept;
    swan_settings &         settings() noexcept;
    bool &                  move_dirents_payload_set() noexcept;
    s32 &                   debug_log_size_limit_megabytes() noexcept;
    s32 &                   page_size() noexcept;

    file_operation_command_buf &file_op_cmd_buf() noexcept;

    std::vector<s64> &delete_icon_textures_queue() noexcept;

    std::array<explorer_window, global_constants::num_explorers> &explorers() noexcept;

} // namespace global_state

void init_explorer_COM_GLFW_OpenGL3(GLFWwindow *window, char const *ini_file_path) noexcept;

struct init_explorer_COM_Win32_DX11_result
{
    bool success;
    char const *what_failed;
    u64 num_attempts_made;
};
init_explorer_COM_Win32_DX11_result init_explorer_COM_Win32_DX11() noexcept;

void cleanup_explorer_COM() noexcept;

void apply_swan_style_overrides() noexcept;

char const *get_icon(basic_dirent::kind t) noexcept;

std::array<char, 64> get_type_text_for_extension(char const *extension) noexcept;

drive_info_array_t query_available_drives_info() noexcept;

recycle_bin_info query_recycle_bin() noexcept;

generic_result open_file(char const *file_name, char const *file_directory, bool as_admin = false) noexcept;

generic_result open_file_with(char const *file_name, char const *file_directory) noexcept;

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
    std::string *init_error,
    char dir_sep_ut8,
    s32 num_max_file_operations) noexcept;

std::tuple<bool, std::string, u64> bulk_rename_parse_text_import(
    char const *text,
    std::vector<swan_path> &transforms_after,
    u64 max_idx,
    std::vector<std::string> &errors) noexcept;

std::optional<ntest::report_result> run_tests(std::filesystem::path const &output_path,
                                              void (*assertion_callback)(ntest::assertion const &, bool)) noexcept;

struct help_indicator
{
    bool hovered;
    bool left_clicked;
    bool right_clicked;
};
help_indicator render_help_indicator(bool align_text_to_frame_padding) noexcept;

ImVec2 help_indicator_size() noexcept;

void free_explorer_drag_drop_payload() noexcept;

void render_main_menu_bar(GLFWwindow *window, std::array<explorer_window, global_constants::num_explorers> &explorers) noexcept;

ImVec4 compute_drive_usage_color(f32 fraction_used) noexcept;

bool find_in_swan_explorer_0(char const *full_path) noexcept;

std::pair<s64, ImVec2> load_icon_texture(char const *full_path_utf8 = nullptr,
                                         wchar_t const *full_path_utf16 = nullptr,
                                         char const *debug_label = nullptr) noexcept;

void delete_icon_texture(s64 &id, char const *debug_label = nullptr) noexcept;

void erase(global_state::completed_file_operations &obj,
           std::deque<completed_file_operation>::iterator first,
           std::deque<completed_file_operation>::iterator last) noexcept;

void pop_back(global_state::completed_file_operations &obj) noexcept;

void erase(global_state::recent_files &obj,
           std::deque<recent_file>::iterator first,
           std::deque<recent_file>::iterator last,
           bool delete_icon_texture = true) noexcept;

void open_file_properties(char const *full_path_utf8) noexcept;

bool window_render_order_save_to_disk(std::array<swan_windows::id, (u64)swan_windows::id::count - 1> data) noexcept;

std::array<swan_windows::id, (u64)swan_windows::id::count - 1> window_render_order_load_from_disk() noexcept;

u64 recent_files_reorder_and_dedupe(std::deque<recent_file> &elems) noexcept;
