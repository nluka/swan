#ifndef SWAN_ACTIVITY_WINDOW_CPP
#define SWAN_ACTIVITY_WINDOW_CPP

#include "common.hpp"

void render_file_ops_window() noexcept(true)
{
    if (!ImGui::Begin("File Operations")) {
        ImGui::End();
        return;
    }

    [[maybe_unused]] auto &io = ImGui::GetIO();

    auto const &file_ops_buffer = get_file_ops_buffer();

    enum file_ops_table_col : i32
    {
        file_ops_table_col_undo,
        file_ops_table_col_when,
        file_ops_table_col_op_type,
        file_ops_table_col_src_path,
        file_ops_table_col_dest_path,
        file_ops_table_col_count
    };

    if (ImGui::BeginTable("Activities", file_ops_table_col_count,
        ImGuiTableFlags_Hideable|ImGuiTableFlags_Resizable|ImGuiTableFlags_SizingStretchProp)
    ) {
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_NoSort, 0.0f, file_ops_table_col_undo);
        ImGui::TableSetupColumn("When", ImGuiTableColumnFlags_NoSort, 0.0f, file_ops_table_col_when);
        ImGui::TableSetupColumn("Op", ImGuiTableColumnFlags_NoSort, 0.0f, file_ops_table_col_op_type);
        ImGui::TableSetupColumn("Src", ImGuiTableColumnFlags_NoSort, 0.0f, file_ops_table_col_src_path);
        ImGui::TableSetupColumn("Dst", ImGuiTableColumnFlags_NoSort, 0.0f, file_ops_table_col_dest_path);
        ImGui::TableHeadersRow();

        for (auto const &file_op : file_ops_buffer) {
            ImGui::TableNextRow();

            if (ImGui::TableSetColumnIndex(file_ops_table_col_undo)) {
                ImGui::SmallButton("Undo");
            }

            if (ImGui::TableSetColumnIndex(file_ops_table_col_when)) {
                time_point_t blank_time = {};
                if (file_op.end_time != blank_time) {
                    time_point_t now = current_time();
                    ImGui::TextUnformatted(compute_when_str(file_op.end_time, now).data());
                }
                else {
                    ImGui::TextUnformatted("Queued");
                }
            }

            if (ImGui::TableSetColumnIndex(file_ops_table_col_op_type)) {
                if      (file_op.op_type == file_operation::type::move  ) ImGui::TextUnformatted("mv");
                else if (file_op.op_type == file_operation::type::copy  ) ImGui::TextUnformatted("cp");
                else if (file_op.op_type == file_operation::type::remove) ImGui::TextUnformatted("rm");
            }

            if (ImGui::TableSetColumnIndex(file_ops_table_col_src_path)) {
                ImGui::TextUnformatted(file_op.src_path.data());
            }

            if (ImGui::TableSetColumnIndex(file_ops_table_col_dest_path)) {
                ImGui::TextUnformatted(file_op.dest_path.data());
            }
        }

        ImGui::EndTable();
    }

    ImGui::End();
}

#endif // SWAN_ACTIVITY_WINDOW_CPP
