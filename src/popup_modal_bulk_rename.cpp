#include "stdafx.hpp"
#include "common_functions.hpp"
#include "imgui_dependent_functions.hpp"
#include "scoped_timer.hpp"

static constexpr u64 num_obj_types = (u64)basic_dirent::kind::count;
static constexpr u64 num_status_types = (u64)bulk_rename_transform::status::count;

namespace bulk_rename_modal_global_state
{
    static std::function<void ()>               g_on_rename_callback = {};
    static std::vector<bulk_rename_transform>   g_transforms = {};
    static swan_path                            g_cwd = {};
    static bool                                 g_open = false;
    static bool                                 g_obj_types_present[num_obj_types] = {};
}

struct transaction_counters
{
    std::atomic<u64> num_completed = 0;
    std::atomic<u64> num_failed = 0;
    std::atomic<u64> num_total = 0;
};

void swan_popup_modals::open_bulk_rename(explorer_window &expl_opened_from, std::function<void ()> on_rename_callback) noexcept
{
    using namespace bulk_rename_modal_global_state;

    g_open = true;
    g_cwd = expl_opened_from.cwd;
    g_on_rename_callback = on_rename_callback;
    memset(g_obj_types_present, false, num_obj_types);

    g_transforms.clear();

    for (auto const &dirent : expl_opened_from.cwd_entries) {
        if (dirent.selected) {
            assert(dirent.basic.type != basic_dirent::kind::nil);
            g_transforms.emplace_back(&dirent.basic, dirent.basic.path.data());
            g_obj_types_present[(u64)dirent.basic.type] = true;
        }
    }
}

struct path_comparator
{
    char dir_sep_utf8;
    swan_path bulk_rename_parent_dir;

    bool operator()(bulk_rename_transform const &lhs, recent_file const &rhs) const noexcept
    {
        swan_path lhs_full_path = bulk_rename_parent_dir;
        [[maybe_unused]] bool success = path_append(lhs_full_path, lhs.before.data(), dir_sep_utf8, true);
        assert(success);
        return strcmp(lhs_full_path.data(), rhs.path.data()) < 0;
    }
    bool operator()(recent_file const &lhs, bulk_rename_transform const &rhs) const noexcept
    {
        swan_path rhs_full_path = bulk_rename_parent_dir;
        [[maybe_unused]] bool success = path_append(rhs_full_path, rhs.before.data(), dir_sep_utf8, true);
        assert(success);
        return strcmp(lhs.path.data(), rhs_full_path.data()) < 0;
    }
};

static
void update_recent_files(std::vector<bulk_rename_transform> &transforms, swan_path const &renames_parent_path) noexcept
{
    std::sort(transforms.begin(), transforms.end(), [](bulk_rename_transform const &lhs, bulk_rename_transform const &rhs) noexcept {
        return strcmp(lhs.before.data(), rhs.before.data()) < 0;
    });

    path_comparator comparator = { global_state::settings().dir_separator_utf8, renames_parent_path };

    auto recent_files = global_state::recent_files_get();
    {
        std::scoped_lock recent_files_lock(*recent_files.mutex);

        for (auto &rf : *recent_files.container) {
            auto range = std::equal_range(transforms.begin(), transforms.end(), rf, comparator);

            if (range.first != transforms.end()) {
                bulk_rename_transform const &matching_rnm = *range.first;
                swan_path rf_new_full_path = comparator.bulk_rename_parent_dir;
                if (path_append(rf_new_full_path, matching_rnm.after.data(), comparator.dir_sep_utf8, true)) {
                    rf.path = rf_new_full_path;
                }
            }
        }
    }

    (void) global_state::recent_files_save_to_disk();
}

enum class modal_state : s32 {
    standby,
    transaction_in_progress,
    transaction_completed,
    transaction_cancelled,
};

static
bool render_execute_all_button(bool disabled_condition, bool cross_out_condition) noexcept
{
    bool pressed;
    {
        imgui::ScopedDisable d(disabled_condition);
        pressed = imgui::Button(ICON_CI_DEBUG_CONTINUE "## bulk_rename");
    }

    if (cross_out_condition) {
        ImRect button_rect = imgui::GetItemRect();
        imgui::GetWindowDrawList()->AddLine(button_rect.Min, button_rect.Max, imgui::ImVec4_to_ImU32(error_color(), true), 2.f);
    }

    if (imgui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled, .5f)) {
        if (imgui::BeginTooltip()) {
            {
                imgui::ScopedDisable d(cross_out_condition);
                imgui::TextUnformatted("Execute all transforms");
            }
            if (cross_out_condition) {
                imgui::TextColored(error_color(), "Blocked due to error, fix before proceeding");
            }
            imgui::EndTooltip();
        }
    }

    return pressed;
}

static
bool render_revert_all_button(bool disabled_condition, bool cross_out_condition) noexcept
{
    bool pressed;
    {
        imgui::ScopedDisable d(disabled_condition);
        pressed = imgui::Button(ICON_CI_DEBUG_REVERSE_CONTINUE "## bulk_rename");
    }

    if (cross_out_condition) {
        ImRect button_rect = imgui::GetItemRect();
        imgui::GetWindowDrawList()->AddLine(button_rect.Min, button_rect.Max, imgui::ImVec4_to_ImU32(error_color(), true), 2.f);
    }

    if (imgui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled, .5f)) {
        if (imgui::BeginTooltip()) {
            {
                imgui::ScopedDisable d(cross_out_condition);
                imgui::TextUnformatted("Undo all transforms");
            }
            if (cross_out_condition) {
                imgui::TextColored(error_color(), "Blocked due to error, fix before proceeding");
            } else {
                imgui::TextDisabled("Hold Ctrl to reset [After] values");
            }
            imgui::EndTooltip();
        }
    }

    return pressed;
}

static
bool render_stop_button(bool transact_active) noexcept
{
    imgui::ScopedDisable d(!transact_active);

    bool pressed = imgui::Button(ICON_CI_STOP_CIRCLE "## bulk_rename");

    if (imgui::IsItemHovered({}, .5f)) {
        imgui::SetTooltip("Stop rename operations");
    }

    return pressed;
}

static
bool render_exit_button(bool transact_active) noexcept
{
    imgui::ScopedDisable d(transact_active);

    bool pressed = imgui::Button(ICON_CI_CHROME_CLOSE "## bulk_rename");

    if (imgui::IsItemHovered({}, .5f)) {
        imgui::SetTooltip("Exit");
    }

    return pressed;
}

static
bool render_reset_all_button(bool transact_active) noexcept
{
    imgui::ScopedDisable d(transact_active);

    bool pressed = imgui::Button(ICON_CI_REFRESH "## bulk rename");

    if (imgui::IsItemHovered({}, .5f)) {
        imgui::SetTooltip("Reset all [After] values");
    }

    return pressed;
}

static
bool render_export_button(bool transact_active,
                          std::vector<bulk_rename_transform>::iterator first_transform,
                          std::vector<bulk_rename_transform>::iterator last_transform,
                          u64 transforms_size_digits) noexcept
{
    bool exported = false;

    imgui::ScopedDisable d(transact_active);

    if (imgui::Button(ICON_CI_ARROW_CIRCLE_UP "## bulk rename")) {
        u64 num = 0;
        std::string content = {};

        for (auto iter = first_transform; iter != last_transform; ++iter) {
            auto line = make_str_static<1200>("[%0*zu] %s\n", transforms_size_digits, num++, iter->after.data());
            content.append(line.data());
        }
        if (content.ends_with('\n')) {
            content.pop_back();
        }

        imgui::SetClipboardText(content.c_str());
        exported = true;
    }
    if (imgui::IsItemHovered({}, .5f)) {
        imgui::SetTooltip("Export text to clipboard");
    }

    return exported;
}

static
bool render_import_button(bool transact_active, bool export_triggered_at_least_once, std::vector<bulk_rename_transform> &transforms) noexcept
{
    bool imported = false;
    static std::vector<std::string> s_errors = {};
    static u64 s_clipboard_num_chars = 0;
    static u64 s_clipboard_num_lines = 0;

    imgui::ScopedDisable d(!export_triggered_at_least_once || transact_active);

    if (imgui::Button(ICON_CI_ARROW_CIRCLE_DOWN "## bulk rename")) {
        char const *clipboard = imgui::GetClipboardText();
        std::vector<swan_path> transforms_after = {};
        auto [success, text_sanitized, num_lines] = bulk_rename_parse_text_import(clipboard, transforms_after, transforms.size() - 1, s_errors);

        s_clipboard_num_lines = num_lines;
        s_clipboard_num_chars = text_sanitized.size();

        if (success) {
            for (u64 i = 0; i < transforms_after.size(); ++i) {
                if (!path_is_empty(transforms_after[i])) {
                    transforms[i].after = transforms_after[i];
                }
            }
            imported = true;
        }
        else {
            assert(!s_errors.empty());
            imgui::OpenPopup("## bulk_rename import errors");
        }
    }
    if (imgui::IsItemHovered({}, .5f)) {
        imgui::SetTooltip("Import text from clipboard");
    }
    if (imgui::BeginPopup("## bulk_rename import errors")) {
        imgui::TextColored(error_color(), "Import failed due to %zu error%s", s_errors.size(), s_errors.size() == 1 ? "" : "s");
        imgui::TextDisabled("Clipboard: %zu chars, %zu lines", s_clipboard_num_chars, s_clipboard_num_lines);
        imgui::Separator();
        for (auto const &err : s_errors) {
            imgui::TextColored(error_color(), "%s %s\n", ICON_CI_ERROR_SMALL, err.c_str());
        }
        imgui::EndPopup();
    }

    return imported;
}

static
bool render_transaction_progress_indicator(transaction_counters const &counters) noexcept
{
    u64 num_completed = counters.num_completed.load();
    u64 num_failed = counters.num_failed.load();
    u64 num_total = counters.num_total.load();

    if (num_total == 0) {
        return false;
    }

    f64 ratio_done = f64(num_completed + num_failed) / f64(num_total);
    f64 percent_done = ratio_done * 100.f;

    if (num_total < 10'000) {
        imgui::Text("%4.0lf %%", percent_done);
    } else {
        imgui::Text("%5.1lf %%", percent_done);
    }
    if (num_failed > 0) {
        imgui::SameLineSpaced(1);
        imgui::TextColored(error_color(), "%zu failed", num_failed);
    }

    return true;
}

enum bulk_rename_table_col_id : s32
{
    bulk_rename_table_col_id_index,
    bulk_rename_table_col_id_status,
    bulk_rename_table_col_id_obj_type,
    bulk_rename_table_col_id_before,
    bulk_rename_table_col_id_after,
    bulk_rename_table_col_id_count
};

struct table_filters_row
{
    bool any_input_edited;
    bool any_input_text_focused;
};

static_vector<ImVec2, 64> spread_points_in_rect(u64 N, const ImRect& rect, u64 grid_divisions_1 = 0, u64 grid_divisions_2 = 0) noexcept
{
    static_vector<ImVec2, 64> points;

    // assert(N > 0 && "Number of points must be greater than zero.");
    if (N == 0) {
        return points;
    }

    u64 num_subrects = N;
    if (N > 1 && N % 2 != 0) num_subrects += 1;

    u64 rows = grid_divisions_1 > 0 ? grid_divisions_1 : static_cast<u64>(std::sqrt(num_subrects));
    u64 cols = grid_divisions_2 > 0 ? grid_divisions_2 : (rows * rows < num_subrects) ? rows + 1 : rows;
    {
        u64 bigger = std::max(rows, cols);
        u64 smaller = std::min(rows, cols);

        if (rect.GetWidth() > rect.GetHeight()) {
            // wide rect, put more divisions horizontally
            rows = smaller;
            cols = bigger;
        }  else {
            // tall rect, put more divisions vertically
            rows = bigger;
            cols = smaller;
        }
    }

    f32 subrect_width = (rect.Max.x - rect.Min.x) / cols;
    f32 subrect_height = (rect.Max.y - rect.Min.y) / rows;

    // Calculate the starting position for the first sub-rectangle
    f32 start_x = rect.Min.x + subrect_width / 2;
    f32 start_y = rect.Min.y + subrect_height / 2;

    u64 index = 0;

    for (u64 i = 0; i < rows; ++i) {
        for (u64 j = 0; j < cols; ++j, ++index) {
            if (index > N) break;
            f32 center_x = start_x + j * subrect_width;
            f32 center_y = start_y + i * subrect_height;
            points.emplace_back(center_x, center_y);
        }
    }

    return points;
}

table_filters_row render_filters_row(
    std::string &filter_before_input,
    std::string &filter_after_input,
    bool obj_type_filters[num_obj_types],
    bool status_filters[num_status_types],
    bool obj_types_present[num_obj_types],
    u64 transforms_size_digits) noexcept
{
    table_filters_row retval = {};

    if (imgui::TableSetColumnIndex(bulk_rename_table_col_id_index)) {
        // render `transforms_size_digits` number of invisible digits to ensure column is always the same size,
        // this is to prevent the unsightly change in column width between frames when ajusting filters.

        imgui::ScopedTextColor tc(ImVec4(0,0,0,0)); // make text invisible
        auto buffer = make_str_static<32>("%0*d", transforms_size_digits, 0);
        imgui::TextUnformatted(buffer.data());
    }

    if (imgui::TableSetColumnIndex(bulk_rename_table_col_id_before)) {
        imgui::ScopedStyle<f32> fp(imgui::GetStyle().FramePadding.x, {});
        imgui::ScopedColor c(ImGuiCol_FrameBg, imgui::GetStyleColorVec4(ImGuiCol_ChildBg));
        imgui::ScopedColor bc(ImGuiCol_Border, imgui::GetStyleColorVec4(ImGuiCol_ChildBg));
        imgui::ScopedAvailWidth w = {};

        retval.any_input_edited |= imgui::InputTextWithHint("## before search", ICON_CI_SEARCH, &filter_before_input);
        retval.any_input_text_focused |= imgui::IsItemFocused();
    }

    if (imgui::TableSetColumnIndex(bulk_rename_table_col_id_after)) {
        imgui::ScopedStyle<f32> fp(imgui::GetStyle().FramePadding.x, {});
        imgui::ScopedColor bc(ImGuiCol_FrameBg, imgui::GetStyleColorVec4(ImGuiCol_ChildBg));
        imgui::ScopedColor bc2(ImGuiCol_Border, imgui::GetStyleColorVec4(ImGuiCol_ChildBg));
        imgui::ScopedAvailWidth w = {};

        retval.any_input_edited |= imgui::InputTextWithHint("## after search", ICON_CI_SEARCH, &filter_after_input);
        retval.any_input_text_focused |= imgui::IsItemFocused();
    }

    if (imgui::TableSetColumnIndex(bulk_rename_table_col_id_obj_type)) {
        ImRect button_rect;
        {
            imgui::ScopedStyle<f32> fp(imgui::GetStyle().FramePadding.x, {});
            imgui::ScopedColor bc(ImGuiCol_Border, imgui::GetStyleColorVec4(ImGuiCol_ChildBg));

            if (imgui::Button(ICON_CI_BLANK "## obj_type")) {
                imgui::OpenPopup("## popup obj_type");
            }
            button_rect = imgui::GetItemRect();

            u64 num_points = std::count(obj_types_present, obj_types_present + num_obj_types, true);
            auto points = spread_points_in_rect(num_points, button_rect);

            for (u64 ti = 0, pi = 0; ti < num_obj_types && pi < num_points; ++ti) {
                if (bool present = obj_types_present[ti]) {
                    auto obj_type = basic_dirent::kind(ti);
                    bool shown = obj_type_filters[ti];
                    ImVec4 vec_color = shown ? get_color(obj_type) : imgui::GetStyleColorVec4(ImGuiCol_Separator);

                    if (basic_dirent::is_symlink(obj_type)) {
                        imgui::GetWindowDrawList()->AddCircle(points[pi++], 3.f, imgui::ImVec4_to_ImU32(vec_color, true));
                    } else {
                        imgui::GetWindowDrawList()->AddCircleFilled(points[pi++], 3.f, imgui::ImVec4_to_ImU32(vec_color, true));
                    }
                }
            }
        }

        ImRect popup_rect;
        {
            ImRect cell_rect = imgui::TableGetCellBgRect(imgui::GetCurrentTable(), bulk_rename_table_col_id_obj_type);
            cell_rect.Max.x -= 2.f;
            cell_rect.Max.y -= 2.f;
            imgui::SetNextWindowPos(cell_rect.GetBR(), ImGuiCond_Appearing);
        }
        if (imgui::BeginPopup("## popup obj_type")) {
            if (imgui::Button(ICON_CI_TASKLIST "## popup obj_type")) {
                retval.any_input_edited = true;
                memset(obj_type_filters, true, sizeof(obj_type_filters));
            }
            imgui::SameLine();
            if (imgui::Button(ICON_CI_CLEAR_ALL "## popup obj_type")) {
                retval.any_input_edited = true;
                memset(obj_type_filters, false, sizeof(obj_type_filters));
            }
            imgui::SameLine();
            if (imgui::Button(ICON_CI_SYMBOL_NULL "## popup obj_type")) {
                retval.any_input_edited = true;
                for (u64 i = 0; i < num_obj_types; ++i) {
                    flip_bool(obj_type_filters[i]);
                }
            }
            imgui::Separator();
            {
                using obj_t = basic_dirent::kind;
                imgui::ScopedStyle<ImVec2> fp(imgui::GetStyle().FramePadding, {});
                retval.any_input_edited |= obj_types_present[(u64)obj_t::directory]            && imgui::Checkbox("Directory ", &obj_type_filters[(u64)obj_t::directory]);
                retval.any_input_edited |= obj_types_present[(u64)obj_t::symlink_to_directory] && imgui::Checkbox("Directory link ", &obj_type_filters[(u64)obj_t::symlink_to_directory]);
                retval.any_input_edited |= obj_types_present[(u64)obj_t::file]                 && imgui::Checkbox("File ", &obj_type_filters[(u64)obj_t::file]);
                retval.any_input_edited |= obj_types_present[(u64)obj_t::symlink_to_file]      && imgui::Checkbox("File link ", &obj_type_filters[(u64)obj_t::symlink_to_file]);
                retval.any_input_edited |= obj_types_present[(u64)obj_t::symlink_ambiguous]    && imgui::Checkbox("Ambig link ", &obj_type_filters[(u64)obj_t::symlink_ambiguous]);
                retval.any_input_edited |= obj_types_present[(u64)obj_t::invalid_symlink]      && imgui::Checkbox("Invalid link ", &obj_type_filters[(u64)obj_t::invalid_symlink]);
            }

            ImVec2 popup_pos = imgui::GetWindowPos();
            popup_rect = ImRect(popup_pos, popup_pos + imgui::GetWindowSize());

            imgui::EndPopup();

            imgui::DrawBestLineBetweenRectCorners(popup_rect, button_rect, ImVec4(255, 255, 255, 100), false, true, 1.f, 0.f);
        }
    }

    if (imgui::TableSetColumnIndex(bulk_rename_table_col_id_status)) {
        ImRect button_rect;
        {
            imgui::ScopedStyle<f32> fp(imgui::GetStyle().FramePadding.x, {});
            imgui::ScopedColor bc(ImGuiCol_Border, imgui::GetStyleColorVec4(ImGuiCol_ChildBg));

            if (imgui::Button(ICON_CI_BLANK "## status_types")) {
                imgui::OpenPopup("## popup status_types");
            }
            button_rect = imgui::GetItemRect();

            u64 num_points = num_status_types; // std::count(status_types_present, status_types_present + num_status_types, true);
            auto points = spread_points_in_rect(num_points, button_rect, 2, 4);

            for (u64 ti = 0, pi = 0; ti < num_status_types && pi < num_points; ++ti, ++pi) {
                auto status_type = bulk_rename_transform::status(ti);
                bool shown = status_filters[ti];
                ImVec4 vec_color = shown ? get_color(status_type) : imgui::GetStyleColorVec4(ImGuiCol_Separator);
                imgui::GetWindowDrawList()->AddCircleFilled(points[pi], 3.f, imgui::ImVec4_to_ImU32(vec_color, true));
            }
        }

        ImRect popup_rect;
        {
            ImRect cell_rect = imgui::TableGetCellBgRect(imgui::GetCurrentTable(), bulk_rename_table_col_id_status);
            cell_rect.Max.x -= 2.f;
            cell_rect.Max.y -= 2.f;
            imgui::SetNextWindowPos(cell_rect.GetBR(), ImGuiCond_Appearing);
        }
        if (imgui::BeginPopup("## popup status_types")) {
            if (imgui::Button(ICON_CI_TASKLIST "## popup status_types")) {
                retval.any_input_edited = true;
                memset(status_filters, true, num_status_types);
            }
            imgui::SameLine();
            if (imgui::Button(ICON_CI_CLEAR_ALL "## popup status_types")) {
                retval.any_input_edited = true;
                memset(status_filters, false, num_status_types);
            }
            imgui::SameLine();
            if (imgui::Button(ICON_CI_SYMBOL_NULL "## popup status_types")) {
                retval.any_input_edited = true;
                for (u64 i = 0; i < num_status_types; ++i) {
                    flip_bool(status_filters[i]);
                }
            }
            imgui::Separator();
            {
                using status_t = bulk_rename_transform::status;
                imgui::ScopedStyle<ImVec2> fp(imgui::GetStyle().FramePadding, {});
                retval.any_input_edited |= imgui::Checkbox("Name empty ", &status_filters[(u64)status_t::error_name_empty]);
                retval.any_input_edited |= imgui::Checkbox("Execute failed ", &status_filters[(u64)status_t::execute_failed]);
                retval.any_input_edited |= imgui::Checkbox("Revert failed ", &status_filters[(u64)status_t::revert_failed]);
                retval.any_input_edited |= imgui::Checkbox("Ready ", &status_filters[(u64)status_t::ready]);
                retval.any_input_edited |= imgui::Checkbox("Execute success ", &status_filters[(u64)status_t::execute_success]);
                retval.any_input_edited |= imgui::Checkbox("Revert success ", &status_filters[(u64)status_t::revert_success]);
                retval.any_input_edited |= imgui::Checkbox("Name unchanged ", &status_filters[(u64)status_t::name_unchanged]);
            }

            ImVec2 popup_pos = imgui::GetWindowPos();
            popup_rect = ImRect(popup_pos, popup_pos + imgui::GetWindowSize());

            imgui::EndPopup();

            imgui::DrawBestLineBetweenRectCorners(popup_rect, button_rect, ImVec4(255, 255, 255, 100), false, true, 1.f, 0.f);
        }
    }

    return retval;
}

u64 deselect_all(std::vector<bulk_rename_transform> &transforms) noexcept
{
    u64 num_deselected = 0;

    for (auto &elem : transforms) {
        bool prev = elem.selected;
        bool &curr = elem.selected;
        curr = false;
        num_deselected += curr != prev;
    }

    return num_deselected;
}

struct render_table_result
{
    table_filters_row filters;
    bool any_after_text_edited;
    bool any_inputs_blank;
    bool execute_selected;
    bool undo_selected;
};

static
render_table_result render_table(
    bool transact_active,
    std::vector<bulk_rename_transform> &transforms,
    std::string &filter_before_input,
    std::string &filter_after_input,
    bool obj_type_filters[num_obj_types],
    bool status_filters[num_status_types],
    std::vector<bulk_rename_transform>::iterator const &filtered_transforms_partition_iter,
    precise_time_point_t const &last_edit_time,
    bool obj_types_present[num_obj_types],
    u64 transforms_size_digits) noexcept
{
    render_table_result retval = {};

    // auto &io = imgui::GetIO();

    static u64 s_latest_selected_row_idx = u64(-1);
    // static bulk_rename_transform *s_deselect_on_context_menu_close = nullptr;

    ImGuiTableFlags table_flags =
        ImGuiTableFlags_Hideable|
        ImGuiTableFlags_Resizable|
        ImGuiTableFlags_SizingStretchProp|
        ImGuiTableFlags_Reorderable|
        ImGuiTableFlags_BordersOuterV|
        (global_state::settings().tables_alt_row_bg ? ImGuiTableFlags_RowBg : 0)|
        (global_state::settings().table_borders_in_body ? 0 : ImGuiTableFlags_NoBordersInBody)
    ;

    ImVec2 avail = imgui::GetContentRegionAvail();
    ImVec2 size = ImVec2(avail.x, avail.y - imgui::CalcTextSize("X").y - imgui::GetStyle().ItemSpacing.y);

    if (imgui::BeginChild("bulk_rename child", size)) {
        if (imgui::BeginTable("bulk_rename table", bulk_rename_table_col_id_count, table_flags, size)) {
            ImGuiTableColumnFlags fixed_col_flags = ImGuiTableColumnFlags_NoResize|ImGuiTableColumnFlags_WidthFixed;

            imgui::TableSetupColumn(ICON_CI_ARRAY, fixed_col_flags, 0.0f, bulk_rename_table_col_id_index);
            imgui::TableSetupColumn(ICON_CI_INFO, fixed_col_flags, 0.0f, bulk_rename_table_col_id_status);
            imgui::TableSetupColumn(ICON_CI_SYMBOL_OBJECT, fixed_col_flags, 0.0f, bulk_rename_table_col_id_obj_type);
            imgui::TableSetupColumn("Before", 0, 0.0f, bulk_rename_table_col_id_before);
            imgui::TableSetupColumn("After", ImGuiTableColumnFlags_NoHide, 0.0f, bulk_rename_table_col_id_after);
            imgui::TableSetupScrollFreeze(0, 1);
            imgui::TableHeadersRow();

            imgui::TableNextRow();
            retval.filters = render_filters_row(
                filter_before_input,
                filter_after_input,
                obj_type_filters,
                status_filters,
                obj_types_present,
                transforms_size_digits
            );

            ImGuiListClipper clipper;
            {
                u64 num_filtered = std::distance(filtered_transforms_partition_iter, transforms.end());
                u64 num_shown = transforms.size() - num_filtered;
                clipper.Begin((s32)num_shown);
            }

            while (clipper.Step())
            for (u64 i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                auto &transform = transforms[i];
                auto status = transform.stat.load();

                imgui::TableNextRow();

                if (imgui::TableSetColumnIndex(bulk_rename_table_col_id_index)) {
                    imgui::Text("%0*zu", transforms_size_digits, i);

                #if 0
                    imgui::ScopedItemFlag tab(ImGuiItemFlags_NoTabStop);
                    auto label = make_str_static<32>("%0*zu", transforms_size_digits, i);

                    if (imgui::Selectable(label.data(), transform.selected)) {
                        bool selection_state_before_activate = transform.selected;

                        u64 num_deselected = 0;
                        if (!io.KeyCtrl && !io.KeyShift) {
                            // entry was selected but Ctrl was not held, so deselect everything
                            num_deselected = deselect_all(transforms);
                        }

                        if (num_deselected > 1) {
                            transform.selected = true;
                        } else {
                            transform.selected = !selection_state_before_activate;
                        }

                        if (io.KeyShift) {
                            auto [first_idx, last_idx] = imgui::SelectRange(s_latest_selected_row_idx, i);
                            s_latest_selected_row_idx = last_idx;
                            for (u64 j = first_idx; j <= last_idx; ++j) {
                                transforms[j].selected = true;
                            }
                        } else {
                            s_latest_selected_row_idx = i;
                        }
                    }

                    if (imgui::IsItemClicked(ImGuiMouseButton_Right)) {
                        if (!transform.selected) { // ! REF1
                            // right clicked an unselected element, deselect any other elements
                            for (auto &elem : transforms) {
                                elem.selected = false;
                            }
                            transform.selected = true; // select the right clicked element when context menu opens
                            s_deselect_on_context_menu_close = &transform; // deselect the right clicked element when context menu closes
                        }
                        imgui::OpenPopup("## bulk_rename context_menu");
                    }
                #endif
                }

                if (imgui::TableSetColumnIndex(bulk_rename_table_col_id_obj_type)) {
                    ImVec4 color = get_color(transform.obj_type);
                    char const *icon = get_icon(transform.obj_type);

                    imgui::TextColored(color, icon);

                    if (imgui::IsItemHovered({}, .5f)) {
                        imgui::ScopedTextColor tc(color);
                        imgui::SetTooltip(basic_dirent::kind_description(transform.obj_type));
                    }
                }

                if (imgui::TableSetColumnIndex(bulk_rename_table_col_id_before)) {
                    {
                        imgui::ScopedDisable d(true);
                        imgui::TextUnformatted(transform.before.data());
                    }
                    imgui::RenderTooltipWhenColumnTextTruncated(bulk_rename_table_col_id_before, transform.before.data());
                }

                if (imgui::TableSetColumnIndex(bulk_rename_table_col_id_after)) {
                    imgui::ScopedStyle<ImVec2> fp(imgui::GetStyle().FramePadding, {});
                    imgui::ScopedColor bgc(ImGuiCol_FrameBg, imgui::GetStyleColorVec4(ImGuiCol_ChildBg));
                    imgui::ScopedColor bc(ImGuiCol_Border, imgui::GetStyleColorVec4(ImGuiCol_ChildBg));
                    imgui::ScopedAvailWidth w = {};

                    ImGuiInputTextFlags read_only = 0;
                    if (transact_active || status == bulk_rename_transform::status::execute_success) {
                        read_only = ImGuiInputTextFlags_ReadOnly;
                    }

                    auto label = make_str_static<32>("## after %zu", i);
                    bool after_edited = imgui::InputText(
                        label.data(),
                        transform.after.data(),
                        transform.after.max_size(),
                        ImGuiInputTextFlags_CallbackCharFilter|read_only,
                        filter_chars_callback,
                        (void *)windows_illegal_path_chars()
                    );

                    retval.any_after_text_edited |= after_edited;
                    transform.input_focused = imgui::IsItemFocused();

                    if (after_edited) {
                        using status_t = bulk_rename_transform::status;

                        if (strempty(transform.after.data())) {
                            transform.stat.store(status_t::error_name_empty);
                        }
                        else if (path_equals_exactly(transform.before, transform.after)) {
                            transform.stat.store(status_t::name_unchanged);
                        }
                        else {
                            transform.stat.store(status_t::name_unchanged);
                        }

                        if (strempty(transform.after.data())) {
                            retval.any_inputs_blank = true;
                        } else {
                            retval.any_inputs_blank = std::any_of(transforms.begin(), transforms.end(), [](bulk_rename_transform const &op) noexcept {
                                return strempty(op.after.data());
                            });
                        }
                    }
                }

                // instead of updating all transforms when an edit occurs, we track when each transform was last updated,
                // performing any outstanding updates when rendering the transform. this is very efficient because of ImGuiListClipper
                // which will not render out of view rows. this way the cost of updating is spread over many frames in the case when the user
                // is scrolling the list, or not paid at all if the user never brings the row into view.
                if (transform.last_updated_time < last_edit_time) {
                    transform.last_updated_time = current_time_precise();

                    using status_t = bulk_rename_transform::status;

                    auto old_status = status;
                    bool transform_executed = one_of(old_status, { status_t::execute_success, status_t::execute_failed, status_t::revert_success, status_t::revert_failed });

                    if (!transform_executed) {
                        auto new_status = status_t::ready;

                        if (strempty(transform.after.data())) {
                            new_status = status_t::error_name_empty;
                        }
                        else if (path_equals_exactly(transform.before, transform.after.data())) {
                            new_status = status_t::name_unchanged;
                        }

                        transform.stat.store(new_status);
                        status = new_status;
                    }
                }

                if (imgui::TableSetColumnIndex(bulk_rename_table_col_id_status)) {
                    ImVec4 status_color = get_color(status);
                    char const *status_icon = nullptr;
                    char const *status_tooltip = nullptr;

                    switch (status) {
                        case bulk_rename_transform::status::execute_success:
                            status_icon = ICON_CI_CIRCLE_LARGE_FILLED; // ICON_CI_PASS
                            status_tooltip = "Executed successfully";
                            break;
                        case bulk_rename_transform::status::revert_success:
                            status_icon = ICON_CI_ARROW_CIRCLE_LEFT; // ICON_CI_PASS
                            status_tooltip = "Reverted successfully";
                            break;
                        case bulk_rename_transform::status::execute_failed:
                        case bulk_rename_transform::status::revert_failed:
                            status_icon = ICON_CI_ERROR;
                            status_tooltip = transform.error.c_str();
                            break;
                        case bulk_rename_transform::status::error_name_empty:
                            status_icon = ICON_CI_CIRCLE_SLASH;
                            status_tooltip = "Name empty";
                            break;
                        case bulk_rename_transform::status::name_unchanged:
                            status_icon = ICON_CI_CIRCLE_LARGE_FILLED; // ICON_CI_CIRCLE_LARGE_OUTLINE
                            status_tooltip = "Same name as before";
                            break;
                        case bulk_rename_transform::status::ready:
                            status_icon = ICON_CI_CIRCLE_LARGE_FILLED; // ICON_CI_COLOR_MODE
                            status_tooltip = "Ready to execute";
                            break;
                    };

                    imgui::TextColored(status_color, status_icon);

                    if (imgui::IsItemHovered({}, .5f)) {
                        imgui::ScopedTextColor tc(status_color);
                        imgui::SetTooltip(status_tooltip);
                    }
                }
            }

        #if 0
            if (imgui::BeginPopup("## bulk_rename context_menu")) {
                if (imgui::Selectable("Execute")) {
                    retval.execute_selected = true;
                }
                if (imgui::Selectable("Undo")) {
                    retval.undo_selected = true;
                }
                imgui::EndPopup();
            }

            if (s_deselect_on_context_menu_close && !imgui::IsPopupOpen("## bulk_rename context_menu")) {
                s_deselect_on_context_menu_close->selected = false;
                s_deselect_on_context_menu_close = nullptr;
            }
        #endif

            imgui::EndTable();
        }

        imgui::EndChild();
    }

    return retval;
}

void swan_popup_modals::render_bulk_rename() noexcept
{
    using namespace bulk_rename_modal_global_state;

    if (g_open) {
        imgui::OpenPopup(swan_popup_modals::label_bulk_rename);
        center_window_and_set_size_when_appearing(1280, 720);
    }
    if (!imgui::BeginPopupModal(swan_popup_modals::label_bulk_rename, nullptr)) {
        return;
    }

    static std::string s_informational_msg = {};
    static bool s_exported = false;
    static bool s_empty_inputs = false;
    static precise_time_point_t s_last_edit_time = {};
    static progressive_task<int /* dummy type, result not used */> s_transaction_task = {};
    static transaction_counters s_transaction_counters = {};

    static std::string s_filter_before_input = {};
    static std::string s_filter_after_input = {};
    static bool s_obj_type_filters[num_obj_types]; // 1 = show, 0 = hide
    static bool s_status_filters[num_status_types]; // 1 = show, 0 = hide
    static std::vector<bulk_rename_transform>::iterator s_filtered_transforms_partition_iter = g_transforms.end();

    auto cleanup_and_close_popup = [&]() noexcept {
        g_open = false;
        g_cwd = {};
        g_transforms.clear();
        g_on_rename_callback = {};
        memset(g_obj_types_present, false, num_obj_types);

        s_informational_msg.clear();
        s_exported = false;
        s_empty_inputs = false;
        s_last_edit_time = {};

        s_filter_before_input.clear();
        s_filter_after_input.clear();
        memset(s_obj_type_filters, true, num_obj_types);
        memset(s_status_filters, true, num_status_types);
        s_filtered_transforms_partition_iter = g_transforms.end();

        imgui::CloseCurrentPopup();
    };

    auto reset = [&]() noexcept {
        s_empty_inputs = false;

        for (auto &transform : g_transforms) {
            if (!one_of(transform.stat.load(), { bulk_rename_transform::status::execute_success })) {
                transform.after = transform.before;
            }
        }

        s_filter_after_input.clear();
        s_filter_before_input.clear();
        memset(s_obj_type_filters, true, num_obj_types);
        memset(s_status_filters, true, num_status_types);
        s_filtered_transforms_partition_iter = g_transforms.end();
    };

    if (imgui::IsWindowAppearing()) {
        reset();
    }

    auto execute_task = [](std::vector<bulk_rename_transform> &transforms, swan_path working_directory_utf8, bool selected_only) noexcept {
        s_transaction_task.started.store(true);
        s_transaction_task.active_token.store(true);
        SCOPE_EXIT { s_transaction_task.active_token.store(false); };

        wchar_t working_directory_utf16[MAX_PATH];
        if (!utf8_to_utf16(working_directory_utf8.data(), working_directory_utf16, lengthof(working_directory_utf16))) {
            return;
        }

        std::wstring working_directory = working_directory_utf16;
        std::replace(working_directory.begin(), working_directory.end(), L'/', L'\\');
        if (!working_directory.ends_with(L'\\')) working_directory += L'\\';

        std::wstring before, after;

        try {
            for (u64 i = 0; i < transforms.size(); ++i) {
                if (s_transaction_task.cancellation_token.load() == true) {
                    break;
                }
                auto &transform = transforms[i];

                if ((selected_only && !transform.selected) || transform.stat.load() != bulk_rename_transform::status::ready) {
                    continue;
                }

                std::string error = transform.execute(working_directory.c_str(), before, after);

                if (error.empty()) {
                    transform.stat.store(bulk_rename_transform::status::execute_success);
                    ++s_transaction_counters.num_completed;
                } else {
                    transform.stat.store(bulk_rename_transform::status::execute_failed);
                    transform.error = std::move(error);
                    ++s_transaction_counters.num_failed;
                }
            }
        }
        catch (...) {
        }
    };

    auto launch_execute_task_if_work_available = [&execute_task](bool consider_selected_only) noexcept {
        u64 num_total = 0;
        for (auto &transform : g_transforms) {
            if (consider_selected_only && !transform.selected) continue;
            bool work_to_do = !path_equals_exactly(transform.before, transform.after);
            if (work_to_do) {
                transform.stat.store(bulk_rename_transform::status::ready);
                num_total += 1;
            }
        }
        if (num_total == 0) {
            s_informational_msg = "> No work to do";
        } else {
            s_transaction_counters.num_completed.store(0);
            s_transaction_counters.num_failed.store(0);
            s_transaction_counters.num_total.store(num_total);

            global_state::thread_pool().push_task(execute_task, std::ref(g_transforms), g_cwd, false);
            s_informational_msg = make_str("> Transaction[%s]", ICON_CI_ARROW_RIGHT);
        }
    };

    auto revert_task = [](std::vector<bulk_rename_transform> &transforms, swan_path working_directory_utf8, bool reset_names, bool selected_only) noexcept {
        s_transaction_task.started.store(true);
        s_transaction_task.active_token.store(true);
        SCOPE_EXIT { s_transaction_task.active_token.store(false); };

        wchar_t working_directory_utf16[MAX_PATH];
        if (!utf8_to_utf16(working_directory_utf8.data(), working_directory_utf16, lengthof(working_directory_utf16))) {
            return;
        }

        std::wstring working_directory = working_directory_utf16;
        std::replace(working_directory.begin(), working_directory.end(), L'/', L'\\');
        if (!working_directory.ends_with(L'\\')) working_directory += L'\\';

        std::wstring before, after;

        try {
            for (u64 i = 0; i < transforms.size(); ++i) {
                if (s_transaction_task.cancellation_token.load() == true) {
                    break;
                }
                auto &transform = transforms[i];

                if ((selected_only && !transform.selected) || transform.stat.load() != bulk_rename_transform::status::execute_success) {
                    continue;
                }

                std::string error = transform.revert(working_directory.c_str(), before, after);

                if (error.empty()) {
                    if (reset_names) {
                        transform.after = transform.before;
                        transform.stat.store(bulk_rename_transform::status::name_unchanged);
                    } else {
                        transform.stat.store(bulk_rename_transform::status::ready);
                    }
                    ++s_transaction_counters.num_completed;
                } else {
                    transform.stat.store(bulk_rename_transform::status::revert_failed);
                    transform.error = std::move(error);
                    ++s_transaction_counters.num_failed;
                }
            }
        }
        catch (...) {
        }
    };

    auto launch_revert_task_if_work_available = [&revert_task](bool consider_selected_only) noexcept {
        u64 num_total = 0;
        for (auto const &transform : g_transforms) {
            if (consider_selected_only && !transform.selected) continue;
            num_total += u64(transform.stat.load() == bulk_rename_transform::status::execute_success);
        }
        if (num_total == 0) {
            s_informational_msg = "> No work to do";
        } else {
            s_transaction_counters.num_completed.store(0);
            s_transaction_counters.num_failed.store(0);
            s_transaction_counters.num_total.store(num_total);

            bool reset_names = imgui::GetIO().KeyCtrl;
            global_state::thread_pool().push_task(revert_task, std::ref(g_transforms), g_cwd, reset_names, false);
            s_informational_msg = make_str("> Transaction[%s]", ICON_CI_ARROW_LEFT);
        }
    };

    u64 transforms_size_digits = count_digits(g_transforms.size());
    bool transact_started = s_transaction_task.started.load();
    bool transact_active = s_transaction_task.active_token.load();
    bool transact_cancelled = s_transaction_task.cancellation_token.load();

    bool revert_all_button_pressed;
    {
        bool disabled_condition = g_transforms.empty() || !transact_started || transact_active;
        bool cross_out_condition = false;
        revert_all_button_pressed = render_revert_all_button(disabled_condition, cross_out_condition);
    }
    if (revert_all_button_pressed) {
        launch_revert_task_if_work_available(false);
    }

    imgui::SameLine();

    bool stop_button_pressed = render_stop_button(transact_active);
    if (stop_button_pressed) {
        s_transaction_task.cancellation_token.store(true);
        assert(s_informational_msg.starts_with("> Transaction["));
        s_informational_msg += " aborted";
    }

    imgui::SameLine();

    bool exec_all_button_pressed;
    {
        bool disabled_condition = g_transforms.empty() || s_transaction_task.active_token.load() == true || s_empty_inputs;
        bool cross_out_condition = s_empty_inputs;
        exec_all_button_pressed = render_execute_all_button(disabled_condition, cross_out_condition);
    }
    if (exec_all_button_pressed) {
        launch_execute_task_if_work_available(false);
    }

    imgui::SameLine();

    bool exit_button_pressed = render_exit_button(transact_active);
    if (exit_button_pressed) {
        assert(!transact_active);

        if (transact_started) {
            update_recent_files(g_transforms, g_cwd);
            g_on_rename_callback();
        }
        cleanup_and_close_popup();
    }

    imgui::SameLineSpaced(2);

    if (render_export_button(transact_active, g_transforms.begin(), s_filtered_transforms_partition_iter, transforms_size_digits)) {
        s_exported = true;
        s_informational_msg = "> Exported";
    }

    imgui::SameLine();

    bool imported = render_import_button(transact_active, s_exported, g_transforms);
    if (imported) {
        s_informational_msg = "> Imported";
    }

    imgui::SameLine();

    bool reset_all_button_pressed = render_reset_all_button(transact_active);
    if (reset_all_button_pressed) {
        reset();
        s_informational_msg = "> Reset";
    }

    if (!s_informational_msg.empty()) {
        imgui::SameLineSpaced(2);
        imgui::TextUnformatted(s_informational_msg.c_str());

        if (transact_started && s_informational_msg.starts_with("> Transaction[")) {
            imgui::SameLine();
            if (!render_transaction_progress_indicator(s_transaction_counters)) {
                imgui::NewLine();
            }
        }
    } else {
        assert(s_transaction_task.active_token.load() == false);
    }

    auto table = render_table(
        transact_active,
        g_transforms,
        s_filter_before_input,
        s_filter_after_input,
        s_obj_type_filters,
        s_status_filters,
        s_filtered_transforms_partition_iter,
        s_last_edit_time,
        g_obj_types_present,
        transforms_size_digits
    );

    if (table.filters.any_input_edited) {
        s_filtered_transforms_partition_iter = std::partition(g_transforms.begin(), g_transforms.end(), [&](bulk_rename_transform const &elem) noexcept {
            bool filtered = false;

            if (!filtered) {
                filtered = !s_status_filters[(u64)elem.stat.load()]; // if checkbox checked, show the type (don't filter)
            }
            if (!filtered) {
                filtered = !s_obj_type_filters[(u64)elem.obj_type]; // if checkbox checked, show the type (don't filter)
            }
            if (!filtered && !s_filter_before_input.empty()) {
                filtered = !StrStrIA(elem.before.data(), s_filter_before_input.c_str());
            }
            if (!filtered && !s_filter_after_input.empty()) {
                filtered = !StrStrIA(elem.after.data(), s_filter_after_input.c_str());
            }

            return !filtered;
        });
    }

#if 0
    if (table.execute_selected) {
        // ! this is no good because bulk_rename_transform::selected is not threadsafe.
        // ! furthermore, render_table will sometimes deselect the target element before we get here - see REF1.
        launch_execute_task_if_work_available(true);
    }
    if (table.undo_selected) {
        // ! this is no good because bulk_rename_transform::selected is not threadsafe.
        // ! furthermore, render_table will sometimes deselect the target element before we get here - see REF1.
        launch_revert_task_if_work_available(true);
    }
#endif

    u64 num_rows_filtered = std::distance(s_filtered_transforms_partition_iter, g_transforms.end());
    if (num_rows_filtered > 0) {
        imgui::Text("%zu items filtered", num_rows_filtered);
#if 0
        imgui::SameLine();
    }
    imgui::Text("Transaction (s:%d a:%d c:%d)", transact_started, transact_active, transact_cancelled);
    imgui::SameLine();
    imgui::TextColored(success_color(), "%zu", s_transaction_counters.num_completed.load());
    imgui::SameLine();
    imgui::TextColored(error_color(), "%zu", s_transaction_counters.num_failed.load());
    imgui::SameLine();
    imgui::Text("%zu", s_transaction_counters.num_total.load());
#else
    }
#endif

    if (imported || reset_all_button_pressed || table.any_after_text_edited) {
        print_debug_msg("change made (%d %d %d) updating s_last_edit_time", imported, reset_all_button_pressed, table.any_after_text_edited);
        s_last_edit_time = current_time_precise();
    }

    if (imgui::IsWindowFocused() && imgui::IsKeyPressed(ImGuiKey_Escape) && !transact_active) {
        if (transact_started) {
            update_recent_files(g_transforms, g_cwd);
            g_on_rename_callback();
        }
        cleanup_and_close_popup();
    }

    imgui::EndPopup();
}

bulk_rename_transform &bulk_rename_transform::operator=(bulk_rename_transform const &other) noexcept // for emplace_back
{
    this->last_updated_time = other.last_updated_time;
    this->stat = other.stat.load();
    this->obj_type = other.obj_type;
    this->before = other.before;
    this->after = other.after;
    this->error = other.error;
    return *this;
}

bulk_rename_transform::bulk_rename_transform(const bulk_rename_transform &other) noexcept // for emplace_back
    : last_updated_time(other.last_updated_time)
    , stat(other.stat.load())
    , obj_type(other.obj_type)
    , before(other.before)
    , after(other.after)
    , error(other.error)
{
}

bulk_rename_transform::bulk_rename_transform(basic_dirent const *before, char const *after) noexcept
    : before(path_create(before->path.data()))
    , after(path_create(after))
    , obj_type(before->type)
    , stat(bulk_rename_transform::status::name_unchanged)
{
}

bulk_rename_transform::bulk_rename_transform(basic_dirent::kind obj_type, char const *before, char const *after) noexcept
    : obj_type(obj_type)
    , before(path_create(before))
    , after(path_create(after))
    , stat(bulk_rename_transform::status::name_unchanged)
{
}

bool bulk_rename_transform::operator!=(bulk_rename_transform const &other) const noexcept // for ntest
{
    return this->before != other.before || !path_equals_exactly(this->after, other.after);
}

std::ostream& operator<<(std::ostream &os, bulk_rename_transform const &r) // for ntest
{
    return os << "(" << (s32)r.obj_type << ") [" << r.before.data() << "]->[" << r.after.data() << ']';
}

std::tuple<bool, std::string, u64> bulk_rename_parse_text_import(
    char const *text_input,
    std::vector<swan_path> &transforms_after,
    u64 max_idx,
    std::vector<std::string> &errors) noexcept
{
    errors.clear();

    std::string text_sanitized = text_input;
    text_sanitized.erase(std::remove(text_sanitized.begin(), text_sanitized.end(), '\r'), text_sanitized.end());
    u64 num_lines = 1 + std::count(text_sanitized.begin(), text_sanitized.end(), '\n');

    {
        u64 max_num_lines = max_idx + 1;
        if (num_lines > max_num_lines) {
            errors.emplace_back(make_str("Tried to import %zu lines, expected max %zu lines", num_lines, max_num_lines));
            return { false, text_sanitized, num_lines };
        }
    }

    bool success = true;
    transforms_after.resize(num_lines);
    errors = {};

    char const *valid_line_syntax = "\\[[0-9]{1,}\\] .{1,}";
    static std::regex const s_valid_line_regex(valid_line_syntax);

    char const *line = strtok(text_sanitized.data(), "\n");

    for (u64 line_num = 1; line != nullptr; ++line_num, line = strtok(nullptr, "\n")) {
        if (strempty(line)) {
            continue;
        }
        std::string_view line_vw(line, strlen(line));

        if (!std::regex_match(line_vw.begin(), line_vw.end(), s_valid_line_regex)) {
            success = false;
            errors.emplace_back(make_str("Line %zu, syntax did not regex_match /%s/", line_num, valid_line_syntax));
            continue;
        }

        assert(line[0] == '[');

        char *parsed_idx_end;
        u64 parsed_idx = strtoull(line + 1, &parsed_idx_end, 10);
        assert(*parsed_idx_end == ']');
        char const *cparsed_idx_end = parsed_idx_end;

        if (parsed_idx > max_idx) {
            success = false;
            errors.emplace_back(make_str("Line %zu, parsed index [%zu] exceeded max of %zu", line_num, parsed_idx, max_idx));
            continue;
        }

        u64 prefix_len = std::distance(line, cparsed_idx_end) + strlen("] ");
        assert(prefix_len >= 4); // at minimum "[N] "

        char const *name = line + prefix_len;
        std::string_view name_vw(name);

        if (name_vw.ends_with(".")) {
            success = false;
            errors.emplace_back(make_str("Line %zu, name ends with [.] character", line_num));
            continue;
        }

        {
            char const *illegal_ch = nullptr;

            for (auto const &ch : name_vw) {
                illegal_ch = strchr("<>:\"/\\|?*", ch);
                if (illegal_ch) {
                    success = false;
                    errors.emplace_back(make_str("Line %zu, name contains illegal character [%c]", line_num, *illegal_ch));
                    break;
                }
            }
            if (illegal_ch) {
                continue;
            }
        }

        transforms_after[parsed_idx] = path_create(name);
    }

    return { success, text_sanitized, num_lines };
}

std::string do_transform(bulk_rename_transform const &transform, wchar_t const *working_directory, std::wstring &old_name, std::wstring &new_name, bool reverse) noexcept
{
    constexpr u64 utf16_buflen = MAX_PATH;
    wchar_t before_utf16[utf16_buflen];
    wchar_t after_utf16[utf16_buflen];

    for (auto const [name_utf8, name_utf16, builder] : { std::make_tuple(&transform.before, before_utf16, &old_name),
                                                         std::make_tuple(&transform.after, after_utf16, &new_name) }
    ) {
        if (!utf8_to_utf16(name_utf8->data(), name_utf16, utf16_buflen)) {
            return make_str("Failed to convert [%s] from UTF-8 to UTF-16.", name_utf8->data());
        }
        assert(working_directory[wcslen(working_directory)-1] == L'\\');
        *builder = working_directory;
        *builder += name_utf16;
    }

    bool success;
    if (reverse) {
        success = MoveFileW(new_name.c_str(), old_name.c_str());
    } else {
        success = MoveFileW(old_name.c_str(), new_name.c_str());
    }
    return success ? "" : get_last_winapi_error().formatted_message;
}

std::string bulk_rename_transform::execute(wchar_t const *working_directory, std::wstring &buffer_before, std::wstring &buffer_after) const noexcept
try {
    assert(this->stat.load() == status::ready);
    return do_transform(*this, working_directory, buffer_before, buffer_after, false);
}
catch (...) {
    return "Exception, catch (...)";
}

std::string bulk_rename_transform::revert(wchar_t const *working_directory, std::wstring &buffer_before, std::wstring &buffer_after) const noexcept
try {
    assert(this->stat.load() == status::execute_success);
    return do_transform(*this, working_directory, buffer_before, buffer_after, true);
}
catch (...) {
    return "Exception, catch (...)";
}
