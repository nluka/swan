#include "stdafx.hpp"
#include "common_functions.hpp"
#include "imgui_dependent_functions.hpp"

bool                            debug_log::g_logging_enabled = true;
std::vector<debug_log_record>   debug_log::g_records;
std::mutex                      debug_log::g_mutex = {};

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

void swan_windows::render_debug_log(bool &open, [[maybe_unused]] bool any_popups_open) noexcept
{
    if (!imgui::Begin(swan_windows::get_name(swan_windows::id::debug_log), &open)) {
        imgui::End();
        return;
    }

    if (imgui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)) {
        global_state::focused_window_set(swan_windows::id::debug_log);
    }

    static bool s_auto_scroll = true;

    bool jump_to_top = imgui::Button(ICON_CI_ARROW_SMALL_UP "## Top");
    imgui::SameLine();
    bool jump_to_bottom = imgui::Button(ICON_CI_ARROW_SMALL_DOWN "## Bottom");

    imgui::SameLineSpaced(1);

    if (imgui::Button("Clear")) {
        debug_log::clear_buffer();
    }

    imgui::SameLineSpaced(2);

    imgui::Checkbox("Logging", &debug_log::g_logging_enabled);

    imgui::SameLineSpaced(1);

    imgui::Checkbox("Auto-scroll", &s_auto_scroll);

    imgui::SameLineSpaced(1);

    {
        imgui::ScopedItemWidth w(imgui::CalcTextSize("123456789_12345").x);
        s32 &size_limit = global_state::debug_log_size_limit_megabytes();
        if (imgui::InputInt("MB size limit", &size_limit, 1, 10)) {
            size_limit = std::clamp(size_limit, 1, 50);
        }
    }

    imgui::Separator();

    s32 table_flags =
        ImGuiTableFlags_SizingStretchProp|
        ImGuiTableFlags_BordersV|
        ImGuiTableFlags_BordersInnerV|
        ImGuiTableFlags_Reorderable|
        ImGuiTableFlags_Resizable|
        // ImGuiTableFlags_ScrollY|
        ImGuiTableFlags_Hideable|
        (global_state::settings().table_borders_in_body ? 0 : ImGuiTableFlags_NoBordersInBody)|
        (global_state::settings().tables_alt_row_bg ? ImGuiTableFlags_RowBg : 0)
    ;

    static f32 s_scroll_y_last_frame = 0;
    static f32 s_scroll_max_y_last_frame = 0;

    if (imgui::BeginChild("debug_log_child")) {
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

            u64 num_records_this_frame = 0;
            static u64 s_num_records_last_frame = 0;

            ImGuiListClipper clipper;
            {
                std::scoped_lock lock(debug_log::g_mutex);
                {
                    u64 num_dirents_to_render = debug_log::g_records.size();
                    assert(num_dirents_to_render <= (u64)INT32_MAX);
                    clipper.Begin(s32(num_dirents_to_render));
                }

                num_records_this_frame = debug_log::g_records.size();

                while (clipper.Step()) {
                    for (u64 i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                        auto &record = debug_log::g_records[i];
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
                        }
                        if (imgui::TableSetColumnIndex(debug_log_table_col_id_source_line)) {
                            auto label = make_str_static<256>("%zu ## Line of elem %zu", record.loc.line(), i);

                            imgui::Selectable(label.data(), false, ImGuiSelectableFlags_AllowDoubleClick);
                            if (imgui::IsItemHovered() && imgui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                                auto full_command = make_str_static<512>("code --goto %s:%zu", record.loc.file_name(), record.loc.line());
                                system(full_command.data());
                            }
                        }
                        if (imgui::TableSetColumnIndex(debug_log_table_col_id_source_function)) {
                            imgui::TextUnformatted(record.loc.function_name());
                        }
                        if (imgui::TableSetColumnIndex(debug_log_table_col_id_message)) {
                            imgui::TextUnformatted(record.message.c_str());
                        }
                    }
                }

            } // scoped_lock

            bool do_auto_scroll =
                s_auto_scroll
                && (s_num_records_last_frame < num_records_this_frame) // added records since last frame
                && (s_scroll_y_last_frame == s_scroll_max_y_last_frame) // were maximally scrolled last frame
            ;

            if (jump_to_top) {
                imgui::SetScrollY(0.f);
            }
            else if (jump_to_bottom) {
                imgui::SetScrollHereY(1.f);
            }
            else if (do_auto_scroll) {
                imgui::SetScrollHereY(1.f);
            }

            s_num_records_last_frame = num_records_this_frame;
            s_scroll_y_last_frame = imgui::GetScrollY();
            s_scroll_max_y_last_frame = imgui::GetScrollMaxY();

            imgui::EndTable();
        }
        imgui::EndChild();
    }

    imgui::End();
}
