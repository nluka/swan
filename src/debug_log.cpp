#include "stdafx.hpp"
#include "common_functions.hpp"
#include "imgui_dependent_functions.hpp"

debug_log_record *              debug_log::g_last_record = nullptr;
std::string                     debug_log::g_search_text = {};
std::vector<debug_log_record>   debug_log::g_records_shown = {};
std::vector<debug_log_record>   debug_log::g_records_hidden = {};
std::mutex                      debug_log::g_mutex = {};
u64                             debug_log::g_next_id = 1;
bool                            debug_log::g_logging_enabled = true;


static s32 g_debug_log_size_limit_megabytes = 5;

s32 &global_state::debug_log_size_limit_megabytes() noexcept { return g_debug_log_size_limit_megabytes; }

enum debug_log_table_col_id : s32
{
    debug_log_table_col_id_thread_id,
    debug_log_table_col_id_system_time,
    debug_log_table_col_id_imgui_time,
    debug_log_table_col_id_source_file,
    debug_log_table_col_id_source_line,
    debug_log_table_col_id_source_function,
    debug_log_table_col_id_message,
    debug_log_table_col_id_count,
};

bool swan_windows::render_debug_log(bool &open, [[maybe_unused]] bool any_popups_open) noexcept
{
    if (!imgui::Begin(swan_windows::get_name(swan_windows::id::debug_log), &open)) {
        return false;
    }

    static bool s_auto_scroll = true;

    bool jump_to_top;
    {
        imgui::ScopedItemFlag no_nav(ImGuiItemFlags_NoNav, true);
        jump_to_top = imgui::Button(ICON_CI_ARROW_SMALL_UP "## Top");
    }

    imgui::SameLine();

    bool jump_to_bottom;
    {
        imgui::ScopedItemFlag no_nav(ImGuiItemFlags_NoNav, true);
        jump_to_bottom = imgui::Button(ICON_CI_ARROW_SMALL_DOWN "## Bottom");
    }

    imgui::SameLineSpaced(0);

    {
        imgui::ScopedItemFlag no_nav(ImGuiItemFlags_NoNav, true);
        if (imgui::Button(ICON_CI_CLEAR_ALL "## debug_log")) {
            debug_log::clear();
        }
    }

    imgui::SameLineSpaced(1);

    {
        imgui::ScopedItemFlag no_nav(ImGuiItemFlags_NoNav, true);

        imgui::Checkbox(ICON_CI_OUTPUT "## debug_log Logging", &debug_log::g_logging_enabled);
        if (imgui::IsItemHovered({}, 1)) {
            imgui::SetTooltip("Click to %s logging", debug_log::g_logging_enabled ? "disable" : "enable");
        }
    }

    imgui::SameLineSpaced(1);

    {
        imgui::ScopedItemFlag no_nav(ImGuiItemFlags_NoNav, true);

        imgui::Checkbox(ICON_CI_FOLD_DOWN "## debug_log Auto-scroll", &s_auto_scroll);
        if (imgui::IsItemHovered({}, 1)) {
            imgui::SetTooltip("Click to %s auto-scroll", s_auto_scroll ? "disable" : "enable");
        }
    }

    imgui::SameLineSpaced(1);

    {
        imgui::ScopedItemWidth w(imgui::CalcTextSize("12345").x);
        imgui::ScopedItemFlag no_nav(ImGuiItemFlags_NoNav, true);
        s32 &size_limit = global_state::debug_log_size_limit_megabytes();

        if (imgui::InputInt("MB limit", &size_limit, 0)) {
            size_limit = std::clamp(size_limit, 1, 50);
        }
    }

    imgui::SameLineSpaced(1);

    bool search_text_edited;
    {
        // imgui::ScopedAvailWidth w = {};
        imgui::ScopedItemWidth w(imgui::CalcTextSize("123456789_123456789_123456789_").x);
        imgui::ScopedItemFlag no_nav(ImGuiItemFlags_NoNav, true);

        search_text_edited = imgui::InputTextWithHint("## debug_log search", ICON_CI_SEARCH, &debug_log::g_search_text);
    }
    imgui::SameLineSpaced(1);
    {
        std::scoped_lock lock(debug_log::g_mutex);
        imgui::TextUnformatted("Shown:");
        imgui::SameLine();
        imgui::ProgressBar(f32(debug_log::g_records_shown.size()) / f32(debug_log::g_records_shown.size() + debug_log::g_records_hidden.size()), ImVec2(100, 0));
        // imgui::TextDisabled("%zu shown  %zu hidden", debug_log::g_records_shown.size(), debug_log::g_records_hidden.size());
    }

    // TODO: fix ugly 1 frame flicker when search_text_edited caused by scroll updating 1 frame after updated records rendered inside table

    static f32 s_scroll_y_target = -1.f;

    if (search_text_edited) {
        std::scoped_lock lock(debug_log::g_mutex);

        u64 num_erased = 0;
        auto &shown = debug_log::g_records_shown;
        auto &hidden = debug_log::g_records_hidden;

        std::copy(hidden.begin(), hidden.end(), std::back_inserter(shown));
        hidden.clear();

        if (!debug_log::g_search_text.empty()) {
            auto partition_point_iter = std::partition(shown.begin(), shown.end(), [](debug_log_record const &elem) noexcept {
                return elem.matches_search_text(debug_log::g_search_text.c_str()); // matches to the left partition
            });
            std::move(partition_point_iter, shown.end(), std::back_inserter(hidden));
            num_erased = std::distance(partition_point_iter, shown.end());
            shown.erase(partition_point_iter, shown.end());
        }

        std::sort(std::execution::parallel_unsequenced_policy(), shown.begin(), shown.end(), std::less<debug_log_record>());

        s_scroll_y_target = imgui::GetTextLineHeightWithSpacing() * shown.size();
        s_scroll_y_target = 0;
    }

    s32 table_flags =
        ImGuiTableFlags_SizingStretchProp|
        ImGuiTableFlags_BordersV|
        ImGuiTableFlags_BordersInnerV|
        ImGuiTableFlags_Reorderable|
        ImGuiTableFlags_Resizable|
        ImGuiTableFlags_ScrollY|
        ImGuiTableFlags_Hideable|
        (global_state::settings().table_borders_in_body ? 0 : ImGuiTableFlags_NoBordersInBody)|
        (global_state::settings().tables_alt_row_bg ? ImGuiTableFlags_RowBg : 0)
    ;
    imgui::ScopedItemFlag no_nav(ImGuiItemFlags_NoNav, true);

    {
        static f32 s_scroll_y_last_frame = 0;
        static f32 s_scroll_y_max_last_frame = 0;
        static u64 s_num_records_last_frame = 0;

        std::scoped_lock lock(debug_log::g_mutex);

        u64 num_records_this_frame = debug_log::g_records_shown.size();

        if (jump_to_top) {
            imgui::SetNextWindowScroll(ImVec2(-1.0f, 0));
        }
        else if (jump_to_bottom) {
            f32 scroll_target = imgui::GetTextLineHeightWithSpacing() * num_records_this_frame;
            imgui::SetNextWindowScroll(ImVec2(-1.0f, scroll_target));
        }
        else if (s_scroll_y_target != -1.f) {
            imgui::SetNextWindowScroll(ImVec2(-1.0f, s_scroll_y_target));
            s_scroll_y_target = -1.f;
        }

        if (imgui::BeginTable("debug_log_table", debug_log_table_col_id_count, table_flags)) {
            imgui::TableSetupColumn("Thread ID", ImGuiTableColumnFlags_NoSort, 0.0f, debug_log_table_col_id_thread_id);
            imgui::TableSetupColumn("System Time", ImGuiTableColumnFlags_NoSort, 0.0f, debug_log_table_col_id_system_time);
            imgui::TableSetupColumn("ImGui Time", ImGuiTableColumnFlags_NoSort, 0.0f, debug_log_table_col_id_imgui_time);
            imgui::TableSetupColumn("File", ImGuiTableColumnFlags_NoSort, 0.0f, debug_log_table_col_id_source_file);
            imgui::TableSetupColumn("Line", ImGuiTableColumnFlags_NoSort, 0.0f, debug_log_table_col_id_source_line);
            imgui::TableSetupColumn("Function", ImGuiTableColumnFlags_NoSort, 0.0f, debug_log_table_col_id_source_function);
            imgui::TableSetupColumn("Message", ImGuiTableColumnFlags_NoSort|ImGuiTableColumnFlags_NoHide, 0.0f, debug_log_table_col_id_message);
            ImGui::TableSetupScrollFreeze(0, 1);
            imgui::TableHeadersRow();

            ImGuiListClipper clipper;
            {
                u64 num_dirents_to_render = debug_log::g_records_shown.size();
                assert(num_dirents_to_render <= (u64)INT32_MAX);
                clipper.Begin(s32(num_dirents_to_render));
            }

            while (clipper.Step()) {
                for (u64 i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                    auto &record = debug_log::g_records_shown[i];
                    imgui::TableNextRow();

                    if (imgui::TableSetColumnIndex(debug_log_table_col_id_thread_id)) {
                        imgui::Text("%d", record.thread_id);
                    }
                    if (imgui::TableSetColumnIndex(debug_log_table_col_id_system_time)) {
                        std::time_t time = std::chrono::system_clock::to_time_t(record.system_time);
                        std::tm tm = *std::localtime(&time);
                        imgui::Text("%d:%02d.%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);
                    }
                    if (imgui::TableSetColumnIndex(debug_log_table_col_id_imgui_time)) {
                        imgui::Text("%.3lf", record.imgui_time);
                    }
                    if (imgui::TableSetColumnIndex(debug_log_table_col_id_source_file)) {
                        imgui::TextUnformatted(path_cfind_filename(record.loc.file_name()));
                        imgui::RenderTooltipWhenColumnTextTruncated(debug_log_table_col_id_source_file, record.loc.file_name());
                    }
                    if (imgui::TableSetColumnIndex(debug_log_table_col_id_source_line)) {
                        auto label = make_str_static<256>("%zu ## Line of elem %zu", record.loc.line(), i);

                        imgui::Selectable(label.data(), false);
                        if (imgui::IsItemClicked(ImGuiMouseButton_Left)) {
                            auto full_command = make_str_static<512>("code --goto %s:%zu", record.loc.file_name(), record.loc.line());
                            system(full_command.data());
                        }
                    }
                    if (imgui::TableSetColumnIndex(debug_log_table_col_id_source_function)) {
                        imgui::TextUnformatted(record.loc.function_name());
                        imgui::RenderTooltipWhenColumnTextTruncated(debug_log_table_col_id_source_function, record.loc.function_name());
                    }
                    if (imgui::TableSetColumnIndex(debug_log_table_col_id_message)) {
                        if (record.num_repeats > 0) {
                            imgui::ScopedTextColor tc(warning_lite_color());
                            imgui::Text("(%zu)", record.num_repeats+1);
                            imgui::SameLine();
                        }

                        imgui::ScopedStyle<ImVec2> fp(imgui::GetStyle().FramePadding, {});
                        imgui::ScopedColor bgc(ImGuiCol_FrameBg, imgui::GetStyleColorVec4(ImGuiCol_ChildBg));
                        imgui::ScopedColor bc(ImGuiCol_Border, imgui::GetStyleColorVec4(ImGuiCol_ChildBg));
                        imgui::ScopedAvailWidth w = {};

                        auto label = make_str_static<64>("## message_%zu", i);
                        imgui::InputText(label.data(), &record.message, ImGuiInputTextFlags_ReadOnly);
                    }
                }
            }

            bool do_auto_scroll =
                s_auto_scroll &&
                (
                    // search_text_edited ||
                    (
                        (num_records_this_frame > s_num_records_last_frame) // added records since last frame
                        && (s_scroll_y_last_frame == s_scroll_y_max_last_frame) // were maximally scrolled last frame
                    )
                )
            ;
            if (do_auto_scroll) {
                s_scroll_y_target = ImGui::GetTextLineHeightWithSpacing() * num_records_this_frame;
                s_scroll_y_last_frame = s_scroll_y_max_last_frame = s_scroll_y_target;
            } else {
                s_scroll_y_last_frame = imgui::GetScrollY();
                s_scroll_y_max_last_frame = imgui::GetScrollMaxY();
            }
            s_num_records_last_frame = num_records_this_frame;

            imgui::EndTable();
        }
    }

    return true;
}
