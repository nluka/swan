#ifndef SWAN_ACTIVITY_WINDOW_CPP
#define SWAN_ACTIVITY_WINDOW_CPP

#include "common.hpp"

void render_file_ops_window() noexcept(true)
{
    namespace imgui = ImGui;

    if (!imgui::Begin("File Operations")) {
        imgui::End();
        return;
    }

    [[maybe_unused]] auto &io = imgui::GetIO();

    auto const &file_ops_buffer = get_file_ops_buffer();

    enum file_ops_table_col : i32
    {
        file_ops_table_col_action,
        file_ops_table_col_status,
        file_ops_table_col_op_type,
        file_ops_table_col_speed,
        file_ops_table_col_src_path,
        file_ops_table_col_dest_path,
        file_ops_table_col_count
    };

    if (imgui::BeginTable("Activities", file_ops_table_col_count,
        ImGuiTableFlags_Hideable|ImGuiTableFlags_Resizable|ImGuiTableFlags_SizingStretchProp)
    ) {
        imgui::TableSetupColumn("Action", ImGuiTableColumnFlags_NoSort, 0.0f, file_ops_table_col_action);
        imgui::TableSetupColumn("Status", ImGuiTableColumnFlags_NoSort, 0.0f, file_ops_table_col_status);
        imgui::TableSetupColumn("Op", ImGuiTableColumnFlags_NoSort, 0.0f, file_ops_table_col_op_type);
        imgui::TableSetupColumn("Speed", ImGuiTableColumnFlags_NoSort, 0.0f, file_ops_table_col_speed);
        imgui::TableSetupColumn("Src", ImGuiTableColumnFlags_NoSort, 0.0f, file_ops_table_col_src_path);
        imgui::TableSetupColumn("Dst", ImGuiTableColumnFlags_NoSort, 0.0f, file_ops_table_col_dest_path);
        imgui::TableHeadersRow();

        for (auto const &file_op : file_ops_buffer) {
            imgui::TableNextRow();

            time_point_t blank_time = {};
            time_point_t now = current_time();

            auto start_time = file_op.start_time.load();
            auto end_time = file_op.end_time.load();
            auto total_size = file_op.total_file_size.load();
            auto bytes_transferred = file_op.total_bytes_transferred.load();
            auto success = file_op.success;

            if (imgui::TableSetColumnIndex(file_ops_table_col_action)) {
                imgui::SmallButton("Undo");
            }

            if (imgui::TableSetColumnIndex(file_ops_table_col_status)) {
                if (start_time == blank_time) {
                    imgui::TextUnformatted("Queued");
                }
                else if (end_time == blank_time) {
                    f64 percent_completed = ((f64)bytes_transferred / (f64)total_size) * 100.0;
                    imgui::Text("%.1lf %%", percent_completed);
                }
                else if (!success) {
                    auto when_str = compute_when_str(end_time, now);
                    imgui::Text("Fail (%s)", when_str.data());
                }
                else {
                    auto when_str = compute_when_str(end_time, now);
                    imgui::Text("Done (%s)", when_str.data());
                }
            }

            if (imgui::TableSetColumnIndex(file_ops_table_col_op_type)) {
                if      (file_op.op_type == file_operation::type::move  ) imgui::TextUnformatted("mv");
                else if (file_op.op_type == file_operation::type::copy  ) imgui::TextUnformatted("cp");
                else if (file_op.op_type == file_operation::type::remove) imgui::TextUnformatted("rm");
            }

            if (imgui::TableSetColumnIndex(file_ops_table_col_speed)) {
                if (
                    start_time == blank_time // operation not started
                    || file_op.op_type == file_operation::type::remove // delete operation
                    || (end_time != blank_time && !success) // operation failed
                ) {
                    imgui::TextUnformatted("--");
                }
                else {
                    u64 ms = compute_diff_ms(start_time, end_time == time_point_t() ? now : end_time);
                    f64 bytes_per_ms = (f64)bytes_transferred / (f64)ms;
                    f64 bytes_per_sec = bytes_per_ms * 1'000.0;

                    f64 gb = 1'000'000'000.0;
                    f64 mb = 1'000'000.0;
                    f64 kb = 1'000.0;

                    f64 rate = bytes_per_sec;
                    char const *unit = "B";

                    if (bytes_per_sec >= gb) {
                        rate = bytes_per_sec / gb;
                        unit = "GB";
                    }
                    else if (bytes_per_sec >= mb) {
                        rate = bytes_per_sec / mb;
                        unit = "MB";
                    }
                    else if (bytes_per_sec >= kb) {
                        rate = bytes_per_sec / kb;
                        unit = "KB";
                    }

                    imgui::Text("%.1lf %s/s", rate, unit);
                }
            }

            if (imgui::TableSetColumnIndex(file_ops_table_col_src_path)) {
                imgui::TextUnformatted(file_op.src_path.data());
            }

            if (imgui::TableSetColumnIndex(file_ops_table_col_dest_path)) {
                imgui::TextUnformatted(file_op.dest_path.data());
            }
        }

        imgui::EndTable();
    }

    imgui::End();
}

#endif // SWAN_ACTIVITY_WINDOW_CPP
