#include "stdafx.hpp"
#include "data_types.hpp"
#include "common_fns.hpp"
#include "imgui_specific.hpp"
#include "path.hpp"

#undef min
#undef max

static std::mutex s_completed_file_ops_mutex = {};
static circular_buffer<completed_file_operation> s_completed_file_ops(1000);

std::pair<circular_buffer<completed_file_operation> *, std::mutex *> global_state::completed_file_ops() noexcept
{
    return std::make_pair(&s_completed_file_ops, &s_completed_file_ops_mutex);
}

bool global_state::save_completed_file_ops_to_disk(std::scoped_lock<std::mutex> *supplied_lock) noexcept
try {
    std::filesystem::path full_path = global_state::execution_path() / "data\\completed_file_ops.txt";

    std::ofstream out(full_path);

    if (!out) {
        return false;
    }

    auto pair = global_state::completed_file_ops();
    auto const &completed_operations = *pair.first;
    auto &mutex = *pair.second;

    {
        auto lock = supplied_lock != nullptr ? std::unique_lock<std::mutex>() : std::unique_lock<std::mutex>(mutex);

        for (auto const &file_op : completed_operations) {
            auto time_t_completion = std::chrono::system_clock::to_time_t(file_op.completion_time);
            std::tm tm_completion = *std::localtime(&time_t_completion);

            auto time_t_undo = std::chrono::system_clock::to_time_t(file_op.undo_time);
            std::tm tm_undo = *std::localtime(&time_t_undo);

            out
                << std::put_time(&tm_completion, "%Y-%m-%d %H:%M:%S") << ' '
                << std::put_time(&tm_undo, "%Y-%m-%d %H:%M:%S") << ' '
                << char(file_op.op_type) << ' '
                << s32(file_op.obj_type) << ' '
                << path_length(file_op.src_path) << ' '
                << file_op.src_path.data() << ' '
                << path_length(file_op.dst_path) << ' '
                << file_op.dst_path.data() << '\n';
        }
    }

    print_debug_msg("SUCCESS global_state::save_completed_file_ops_to_disk");
    return true;
}
catch (...) {
    print_debug_msg("FAILED global_state::save_completed_file_ops_to_disk");
    return false;
}

std::pair<bool, u64> global_state::load_completed_file_ops_from_disk(char dir_separator) noexcept
try {
    std::filesystem::path full_path = global_state::execution_path() / "data\\completed_file_ops.txt";

    std::ifstream in(full_path);

    if (!in) {
        return { false, 0 };
    }

    auto pair = global_state::completed_file_ops();
    auto &completed_operations = *pair.first;
    auto &mutex = *pair.second;

    std::scoped_lock lock(mutex);

    completed_operations.clear();

    std::string line = {};
    line.reserve(global_state::page_size() - 1);

    u64 num_loaded_successfully = 0;

    while (std::getline(in, line)) {
        std::istringstream iss(line);

        char stored_op_type = 0;
        s32 stored_obj_type = 0;
        u64 stored_src_path_len = 0;
        u64 stored_dst_path_len = 0;
        swan_path_t stored_src_path = {};
        swan_path_t stored_dst_path = {};

        std::tm tm_completion = {};
        iss >> std::get_time(&tm_completion, "%Y-%m-%d %H:%M:%S");
        system_time_point_t stored_time_completion = std::chrono::system_clock::from_time_t(std::mktime(&tm_completion));
        iss.ignore(1);

        std::tm tm_undo = {};
        iss >> std::get_time(&tm_undo, "%Y-%m-%d %H:%M:%S");
        system_time_point_t stored_time_undo = std::chrono::system_clock::from_time_t(std::mktime(&tm_undo));
        iss.ignore(1);

        iss >> stored_op_type;
        iss.ignore(1);

        iss >> stored_obj_type;
        iss.ignore(1);

        iss >> (u64 &)stored_src_path_len;
        iss.ignore(1);

        iss.read(stored_src_path.data(), std::min(stored_src_path_len, stored_src_path.max_size() - 1));
        iss.ignore(1);

        iss >> (u64 &)stored_dst_path_len;
        iss.ignore(1);

        iss.read(stored_dst_path.data(), std::min(stored_dst_path_len, stored_dst_path.max_size() - 1));

        path_force_separator(stored_src_path, dir_separator);
        path_force_separator(stored_dst_path, dir_separator);

        completed_operations.push_back();
        completed_operations.back().completion_time = stored_time_completion;
        completed_operations.back().undo_time = stored_time_undo;
        completed_operations.back().src_path = stored_src_path;
        completed_operations.back().dst_path = stored_dst_path;
        completed_operations.back().op_type = file_operation_type(stored_op_type);
        completed_operations.back().obj_type = basic_dirent::kind(stored_obj_type);

        ++num_loaded_successfully;

        line.clear();
    }

    print_debug_msg("SUCCESS global_state::load_completed_file_ops_from_disk, loaded %zu records", num_loaded_successfully);
    return { true, num_loaded_successfully };
}
catch (...) {
    print_debug_msg("FAILED global_state::load_completed_file_ops_from_disk");
    return { false, 0 };
}

completed_file_operation::completed_file_operation() noexcept // for boost::circular_buffer
    : completion_time()
    , src_path()
    , dst_path()
    , op_type()
    , obj_type()
{
}

completed_file_operation::completed_file_operation(system_time_point_t completion_time, file_operation_type op_type, char const *src, char const *dst, basic_dirent::kind obj_type) noexcept // for boost::circular_buffer
    : completion_time(completion_time)
    , src_path(path_create(src))
    , dst_path(path_create(dst))
    , op_type(op_type)
    , obj_type(obj_type)
{
}

completed_file_operation::completed_file_operation(completed_file_operation const &other) noexcept // for boost::circular_buffer
    : completion_time(other.completion_time)
    , src_path(other.src_path)
    , dst_path(other.dst_path)
    , op_type(other.op_type)
    , obj_type(other.obj_type)
{
}

completed_file_operation &completed_file_operation::operator=(completed_file_operation const &other) noexcept // for boost::circular_buffer
{
    this->completion_time = other.completion_time;
    this->src_path = other.src_path;
    this->dst_path = other.dst_path;
    this->op_type = other.op_type;
    this->obj_type = other.obj_type;

    return *this;
}

struct undelete_file_result
{
    bool step0_convert_hardlink_path_to_utf16;
    bool step1_metadata_file_opened;
    bool step2_metadata_file_read;
    bool step3_new_hardlink_created;
    bool step4_old_hardlink_deleted;
    bool step5_metadata_file_deleted;

    bool success() noexcept
    {
        return step0_convert_hardlink_path_to_utf16
            && step1_metadata_file_opened
            && step2_metadata_file_read
            && step3_new_hardlink_created
            && step4_old_hardlink_deleted
            && step5_metadata_file_deleted;
    }
};

/// @brief Restores a "deleted" file from the recycle bin. Implementation closely follows https://superuser.com/a/1736690.
/// @param recycle_bin_hardlink_path_utf8 The full UTF-8 path to the hardlink in the recycling bin, format: $Recycle.Bin/.../RXXXXXX[.ext]
/// @return A structure of booleans indicating which steps were completed successfully. If all values are true, the undelete was successful.
undelete_file_result undelete_file(char const *recycle_bin_hardlink_path_utf8) noexcept
{
    undelete_file_result retval = {};

    wchar_t recycle_bin_hardlink_path_utf16[MAX_PATH];

    if (!utf8_to_utf16(recycle_bin_hardlink_path_utf8, recycle_bin_hardlink_path_utf16, lengthof(recycle_bin_hardlink_path_utf16))) {
        return retval;
    }
    retval.step0_convert_hardlink_path_to_utf16 = true;

    wchar_t recycle_bin_metadata_path_utf16[MAX_PATH];
    (void) StrCpyNW(recycle_bin_metadata_path_utf16, recycle_bin_hardlink_path_utf16, lengthof(recycle_bin_metadata_path_utf16));
    {
        wchar_t *metadata_file_name = get_file_name(recycle_bin_metadata_path_utf16);
        metadata_file_name[1] = L'I'; // $RXXXXXX[.ext] -> $IXXXXXX[.ext]
    }

    HANDLE metadata_file_handle = CreateFileW(recycle_bin_metadata_path_utf16,
                                              GENERIC_READ,
                                              0,
                                              NULL,
                                              OPEN_EXISTING,
                                              FILE_FLAG_SEQUENTIAL_SCAN,
                                              NULL);

    if (metadata_file_handle == INVALID_HANDLE_VALUE) {
        return retval;
    }
    retval.step1_metadata_file_opened = true;

    SCOPE_EXIT { if (metadata_file_handle != INVALID_HANDLE_VALUE) CloseHandle(metadata_file_handle); };

    constexpr u64 num_bytes_header = sizeof(s64);
    constexpr u64 num_bytes_file_size = sizeof(s64);
    constexpr u64 num_bytes_file_deletion_date = sizeof(s64);
    constexpr u64 num_bytes_file_path_length = sizeof(s32);

    char metadata_buffer[num_bytes_header + 2 + // 2 potential junk bytes preceeding header, is my interpretation
                         num_bytes_file_size +
                         num_bytes_file_deletion_date +
                         num_bytes_file_path_length +
                         MAX_PATH] = {};

    DWORD metadata_file_size = GetFileSize(metadata_file_handle, nullptr);
    assert(metadata_file_size <= lengthof(metadata_buffer));

    DWORD bytes_read = 0;
    if (!ReadFile(metadata_file_handle, metadata_buffer, metadata_file_size, &bytes_read, NULL)) {
        return retval;
    }
    assert(bytes_read == metadata_file_size);
    retval.step2_metadata_file_read = true;

    u64 num_bytes_junk = 0;
    num_bytes_junk += u64(metadata_buffer[0] == 0xFF);
    num_bytes_junk += u64(metadata_buffer[1] == 0xFE);

    [[maybe_unused]] s64      *header             = reinterpret_cast<s64 *     >(metadata_buffer + num_bytes_junk);
    [[maybe_unused]] s64      *file_size          = reinterpret_cast<s64 *     >(metadata_buffer + num_bytes_junk + num_bytes_header);
    [[maybe_unused]] FILETIME *file_deletion_date = reinterpret_cast<FILETIME *>(metadata_buffer + num_bytes_junk + num_bytes_header + num_bytes_file_size);
    [[maybe_unused]] s32      *file_path_len      = reinterpret_cast<s32 *     >(metadata_buffer + num_bytes_junk + num_bytes_header + num_bytes_file_size + num_bytes_file_deletion_date);
    [[maybe_unused]] wchar_t  *file_path_utf16    = reinterpret_cast<wchar_t * >(metadata_buffer + num_bytes_junk + num_bytes_header + num_bytes_file_size + num_bytes_file_deletion_date + num_bytes_file_path_length);

    if (!CreateHardLinkW(file_path_utf16, recycle_bin_hardlink_path_utf16, NULL)) {
        return retval;
    }
    retval.step3_new_hardlink_created = true;

    retval.step4_old_hardlink_deleted = DeleteFileW(recycle_bin_hardlink_path_utf16);

    if (CloseHandle(metadata_file_handle)) {
        metadata_file_handle = INVALID_HANDLE_VALUE;
    }
    retval.step5_metadata_file_deleted = DeleteFileW(recycle_bin_metadata_path_utf16);

    return retval;
}

void perform_undelete_directory(
    swan_path_t directory_path_in_recycle_bin_utf8,
    swan_path_t destination_dir_path_utf8,
    swan_path_t destination_full_path_utf8,
    std::mutex *init_done_mutex,
    std::condition_variable *init_done_cond,
    bool *init_done,
    std::string *init_error) noexcept
{
    auto set_init_error_and_notify = [&](std::string const &err) {
        std::unique_lock lock(*init_done_mutex);
        *init_done = true;
        *init_error = err;
        init_done_cond->notify_one();
    };

    wchar_t directory_path_in_recycle_bin_utf16[MAX_PATH];

    if (!utf8_to_utf16(directory_path_in_recycle_bin_utf8.data(), directory_path_in_recycle_bin_utf16, lengthof(directory_path_in_recycle_bin_utf16))) {
        return set_init_error_and_notify("Conversion of directory path (in recycle bin) from UTF-8 to UTF-16.");
    }
    {
        auto begin = directory_path_in_recycle_bin_utf16;
        auto end = directory_path_in_recycle_bin_utf16 + wcslen(directory_path_in_recycle_bin_utf16);
        std::replace(begin, end, L'/', L'\\');
    }

    wchar_t destination_dir_path_utf16[MAX_PATH];

    if (!utf8_to_utf16(destination_dir_path_utf8.data(), destination_dir_path_utf16, lengthof(destination_dir_path_utf16))) {
        return set_init_error_and_notify("Conversion of destination path from UTF-8 to UTF-16.");
    }
    {
        auto begin = destination_dir_path_utf16;
        auto end = destination_dir_path_utf16 + wcslen(destination_dir_path_utf16);
        std::replace(begin, end, L'/', L'\\');
    }

    HRESULT result = {};

    result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(result)) {
        return set_init_error_and_notify(make_str("CoInitializeEx(COINIT_APARTMENTTHREADED), %s", _com_error(result).ErrorMessage()));
    }
    SCOPE_EXIT { CoUninitialize(); };

    IFileOperation *file_op = nullptr;

    result = CoCreateInstance(CLSID_FileOperation, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&file_op));
    if (FAILED(result)) {
        return set_init_error_and_notify(make_str("CoCreateInstance(CLSID_FileOperation), %s", _com_error(result).ErrorMessage()));
    }
    SCOPE_EXIT { file_op->Release(); };

#if 0
    result = file_op->SetOperationFlags(FOF_NOCONFIRMATION);
    if (FAILED(result)) {
        return set_init_error_and_notify(make_str("IFileOperation::SetOperationFlags, %s", _com_error(result).ErrorMessage()));
    }
#endif

    IShellItem *item_to_restore = nullptr;

    result = SHCreateItemFromParsingName(directory_path_in_recycle_bin_utf16, nullptr, IID_PPV_ARGS(&item_to_restore));
    if (FAILED(result)) {
        return set_init_error_and_notify(make_str("SHCreateItemFromParsingName, %s", _com_error(result).ErrorMessage()));
    }
    SCOPE_EXIT { item_to_restore->Release(); };

    IShellItem *destination = nullptr;

    result = SHCreateItemFromParsingName(destination_dir_path_utf16, nullptr, IID_PPV_ARGS(&destination));
    if (FAILED(result)) {
        return set_init_error_and_notify(make_str("SHCreateItemFromParsingName, %s", _com_error(result).ErrorMessage()));
    }
    SCOPE_EXIT { destination->Release(); };

    undelete_directory_progress_sink prog_sink;
    prog_sink.destination_full_path_utf8 = destination_full_path_utf8;
    DWORD cookie = {};

    wchar_t const *restored_name = cget_file_name(destination_dir_path_utf16);
    result = file_op->MoveItem(item_to_restore, destination, restored_name, &prog_sink);
    if (FAILED(result)) {
        return set_init_error_and_notify(make_str("FAILED IFileOperation::MoveItem, %s", _com_error(result).ErrorMessage()));
    }

    result = file_op->Advise(&prog_sink, &cookie);
    if (FAILED(result)) {
        return set_init_error_and_notify(make_str("FAILED IFileOperation::Advise, %s", _com_error(result).ErrorMessage()));
    }

    set_init_error_and_notify(""); // init succeeded, no error

    result = file_op->PerformOperations();
    if (FAILED(result)) {
        _com_error err(result);
        print_debug_msg("FAILED IFileOperation::PerformOperations, %s", err.ErrorMessage());
    }

    file_op->Unadvise(cookie);
    if (FAILED(result)) {
        print_debug_msg("FAILED IFileOperation::Unadvise(%d), %s", cookie, _com_error(result).ErrorMessage());
    }

    // `undone_time` is set in undelete_directory_progress_sink::FinishOperations
}

generic_result enqueue_undelete_directory(char const *directory_path_in_recycle_bin_utf8, char const *destination_dir_path_utf8, char const *desination_full_path) noexcept
{
    /*
        ? Undeleting a directory from the recycle bin (moving the directory from the recycle bin to it's original location) via shlwapi is a blocking operation.
        ? Thus we must call IFileOperation::PerformOperations outside of the main UI thread so the user can continue to interact with the UI during the operation.
        ? There is a constraint however: the IFileOperation object must be initialized on the same thread which will call PerformOperations.
        ? This function will block until the IFileOperation initialization is completed so that we can report any error to the user.
        ? After the worker thread signals that initialization is complete, we proceed with execution and let PerformOperations happen asynchronously.
    */

    bool initialization_done = false;
    std::string initialization_error = {};
    static std::mutex initialization_done_mutex = {};
    static std::condition_variable initialization_done_cond = {};

    global_state::thread_pool().push_task(perform_undelete_directory,
        path_create(directory_path_in_recycle_bin_utf8),
        path_create(destination_dir_path_utf8),
        path_create(desination_full_path),
        &initialization_done_mutex,
        &initialization_done_cond,
        &initialization_done,
        &initialization_error);

    {
        std::unique_lock lock(initialization_done_mutex);
        initialization_done_cond.wait(lock, [&]() { return initialization_done; });
    }

    return { initialization_error.empty(), initialization_error };
}

void swan_windows::render_file_operations(bool &open) noexcept
{
    if (!imgui::Begin(swan_windows::get_name(swan_windows::file_operations), &open)) {
        imgui::End();
        return;
    }

    if (imgui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)) {
        global_state::save_focused_window(swan_windows::file_operations);
    }

    [[maybe_unused]] auto &io = imgui::GetIO();

    enum file_ops_table_col : s32
    {
        file_ops_table_col_group,
        file_ops_table_col_op_type,
        file_ops_table_col_completion_time,
        file_ops_table_col_src_path,
        file_ops_table_col_dst_path,
        file_ops_table_col_count
    };

    if (imgui::BeginTable("completed_file_operations", file_ops_table_col_count,
        ImGuiTableFlags_Resizable|ImGuiTableFlags_SizingStretchProp|
        (global_state::settings().cwd_entries_table_alt_row_bg ? ImGuiTableFlags_RowBg : 0))
    ) {
        static completed_file_operation *right_clicked_row = nullptr;
        u64 remove_idx = u64(-1);

        imgui::TableSetupColumn("Group", ImGuiTableColumnFlags_NoSort, 0.0f, file_ops_table_col_group);
        imgui::TableSetupColumn("Type", ImGuiTableColumnFlags_NoSort, 0.0f, file_ops_table_col_op_type);
        imgui::TableSetupColumn("Completed", ImGuiTableColumnFlags_NoSort, 0.0f, file_ops_table_col_completion_time);
        imgui::TableSetupColumn("Source Path", ImGuiTableColumnFlags_NoSort, 0.0f, file_ops_table_col_src_path);
        imgui::TableSetupColumn("Destination Path", ImGuiTableColumnFlags_NoSort, 0.0f, file_ops_table_col_dst_path);
        imgui::TableHeadersRow();

        auto pair = global_state::completed_file_ops();
        auto &completed_operations = *pair.first;
        auto &mutex = *pair.second;

        std::scoped_lock lock(mutex);

        ImGuiListClipper clipper;
        {
            u64 num_rows_to_render = completed_operations.size();
            assert(num_rows_to_render <= (u64)INT32_MAX);
            clipper.Begin(s32(num_rows_to_render));
        }

        while (clipper.Step())
        for (u64 i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
            auto &file_op = completed_operations[i];

            imgui::TableNextRow();

            if (imgui::TableSetColumnIndex(file_ops_table_col_group)) {
                if (file_op.group_id != 0) {
                    imgui::Text("%zu", file_op.group_id);
                }
            }

            if (imgui::TableSetColumnIndex(file_ops_table_col_op_type)) {
                char const *icon = get_icon(file_op.obj_type);
                ImVec4 icon_color = get_color(file_op.obj_type);

                char const *desc = nullptr;
                if      (file_op.op_type == file_operation_type::move) desc = "Move";
                else if (file_op.op_type == file_operation_type::copy) desc = "Copy";
                else if (file_op.op_type == file_operation_type::del ) desc = "Delete";

                imgui::TextColored(icon_color, icon);
                imgui::SameLine();
                imgui::TextUnformatted(desc);
            }

            if (imgui::TableSetColumnIndex(file_ops_table_col_completion_time)) {
                auto when_completed_str = compute_when_str(file_op.completion_time, current_time_system());

                ImVec2 cursor_start = imgui::GetCursorScreenPos();
                imgui::TextUnformatted(when_completed_str.data());

                if (file_op.undone()) {
                    // strikethru completion time
                    {
                        ImVec2 text_size = imgui::CalcTextSize(when_completed_str.data());
                        f32 line_pos_y = cursor_start.y + (text_size.y / 2);
                        ImVec2 line_p1 = ImVec2(cursor_start.x, line_pos_y);
                        ImVec2 line_p2 = ImVec2(cursor_start.x + text_size.x, line_pos_y);
                        ImGui::GetWindowDrawList()->AddLine(line_p1, line_p2, IM_COL32(255, 0, 0, 255), 1.0f);
                    }

                    imgui::SameLine();

                    auto when_undone_str = compute_when_str(file_op.undo_time, current_time_system());
                    imgui::Text("Undone %s", when_undone_str.data());
                }
            }

            if (imgui::TableSetColumnIndex(file_ops_table_col_src_path)) {
                char buffer[2048];
                (void) snprintf(buffer, lengthof(buffer), "%s##%zu", file_op.src_path.data(), i);

                imgui::Selectable(buffer, &file_op.selected, ImGuiSelectableFlags_SpanAllColumns);

                if (imgui::IsItemClicked(ImGuiMouseButton_Right)) {
                    right_clicked_row = &file_op;
                    imgui::OpenPopup("## completed_file_operations context");
                }
            }
            imgui::RenderTooltipWhenColumnTextTruncated(file_ops_table_col_src_path, file_op.src_path.data());

            if (imgui::TableSetColumnIndex(file_ops_table_col_dst_path)) {
                imgui::TextUnformatted(file_op.dst_path.data());
            }
            imgui::RenderTooltipWhenColumnTextTruncated(file_ops_table_col_dst_path, file_op.dst_path.data());
        }

        if (imgui::BeginPopup("## completed_file_operations context")) {
            assert(right_clicked_row != nullptr);

            if (!right_clicked_row->undone() && right_clicked_row->op_type == file_operation_type::del && imgui::Selectable("Undelete")) {
                if (right_clicked_row->obj_type == basic_dirent::kind::directory) {
                    swan_path_t restore_dir_utf8 = path_create(right_clicked_row->src_path.data(),
                                                               get_everything_minus_file_name(right_clicked_row->src_path.data()).length());

                    auto res = enqueue_undelete_directory(right_clicked_row->dst_path.data(), restore_dir_utf8.data(), right_clicked_row->src_path.data());

                    if (!res.success) {
                        std::string action = make_str("Undelete directory [%s].", right_clicked_row->src_path.data());
                        swan_popup_modals::open_error(action.c_str(), res.error_or_utf8_path.c_str());
                    }
                }
                else {
                    auto res = undelete_file(right_clicked_row->dst_path.data());

                    if (res.success()) {
                        right_clicked_row->undo_time = current_time_system();
                        right_clicked_row->selected = false;
                        (void) global_state::save_completed_file_ops_to_disk(&lock);
                    }
                    else {
                        std::string action = make_str("Undelete file [%s].", right_clicked_row->src_path.data());
                        std::string failure;

                        if      (!res.step0_convert_hardlink_path_to_utf16) failure = make_str("Failed to convert hardlink path [%s] from UTF-8 to UTF-16.", right_clicked_row->dst_path.data());
                        else if (!res.step1_metadata_file_opened          ) failure = make_str("Failed to open metadata file corresponding to [%s], %s", right_clicked_row->dst_path.data(), get_last_winapi_error().formatted_message.c_str());
                        else if (!res.step2_metadata_file_read            ) failure = make_str("Failed to read contents of metadata file corresponding to [%s]", right_clicked_row->dst_path.data());
                        else if (!res.step3_new_hardlink_created          ) failure = make_str("Failed to create hardlink [%s], %s", right_clicked_row->src_path.data(), get_last_winapi_error().formatted_message.c_str());
                        else if (!res.step4_old_hardlink_deleted          ) failure = make_str("Failed to delete hardlink [%s], %s", right_clicked_row->dst_path.data(), get_last_winapi_error().formatted_message.c_str());
                        else if (!res.step5_metadata_file_deleted         ) failure = make_str("Failed to delete metadata file corresponding to [%s], %s", right_clicked_row->dst_path.data(), get_last_winapi_error().formatted_message.c_str());
                        else                                                assert(false && "Bad code path");

                        swan_popup_modals::open_error(action.c_str(), failure.c_str());

                        if (res.step3_new_hardlink_created) {
                            // not a complete success but enough to consider the deletion undone, as the last 2 steps are merely cleanup of the recycle bin
                            right_clicked_row->undo_time = current_time_system();
                            (void) global_state::save_completed_file_ops_to_disk(&lock);
                        }
                    }
                }
            }

            if (imgui::Selectable("Forget")) {
                remove_idx = std::distance(&completed_operations.front(), right_clicked_row);
            }

            imgui::EndPopup();
        }

        if (remove_idx != u64(-1)) {
            completed_operations.erase(completed_operations.begin() + remove_idx);
            (void) global_state::save_completed_file_ops_to_disk(&lock);
        }

        imgui::EndTable();
    }

    imgui::End();
}

void print_SIGDN_values(char const *func_label, char const *item_name, IShellItem *item) noexcept
{
    std::pair<SIGDN, char const *> values[] = {
        { SIGDN_DESKTOPABSOLUTEEDITING, "SIGDN_DESKTOPABSOLUTEEDITING" },
        { SIGDN_DESKTOPABSOLUTEPARSING, "SIGDN_DESKTOPABSOLUTEPARSING" },
        { SIGDN_FILESYSPATH, "SIGDN_FILESYSPATH" },
        { SIGDN_NORMALDISPLAY, "SIGDN_NORMALDISPLAY" },
        { SIGDN_PARENTRELATIVE, "SIGDN_PARENTRELATIVE" },
        { SIGDN_PARENTRELATIVEEDITING, "SIGDN_PARENTRELATIVEEDITING" },
        { SIGDN_PARENTRELATIVEFORADDRESSBAR, "SIGDN_PARENTRELATIVEFORADDRESSBAR" },
        { SIGDN_PARENTRELATIVEFORUI, "SIGDN_PARENTRELATIVEFORUI" },
        { SIGDN_PARENTRELATIVEPARSING, "SIGDN_PARENTRELATIVEPARSING" },
        { SIGDN_URL, "SIGDN_URL" },
    };

    for (auto const &val : values) {
        wchar_t *data = nullptr;
        if (SUCCEEDED(item->GetDisplayName(val.first, &data))) {
            char data_utf8[2048]; init_empty_cstr(data_utf8);
            if (utf16_to_utf8(data, data_utf8, lengthof(data_utf8))) {
                print_debug_msg("%s %s %s = [%s]", func_label, item_name, val.second, data_utf8);
            }
            CoTaskMemFree(data);
        }
    }
}

/// @brief Performs a sequence of file operations.
/// @param destination_directory_utf16 The destination of the operations. For example, the place where we are copying files to.
/// @param paths_to_execute_utf16 Single string of absolute paths to execute an operation against. Each path must be separated by a newline.
/// @param operations_to_execute Vector of chars where each char represents an operation such as 'C' for Copy.
/// Element 0 is the operation assigned to the first path in `paths_to_execute_utf16`, and so on.
/// @param init_done_mutex Mutex for `init_done`.
/// @param init_done_cond Condition variable, signalled when `init_done` is set to true by this function.
/// @param init_done Set to true after initialization is completed.
/// @param init_error Output parameter, where to store initialization error message. If empty, initalization was successful.
void perform_file_operations(
    s32 dst_expl_id,
    std::wstring destination_directory_utf16,
    std::wstring paths_to_execute_utf16,
    std::vector<file_operation_type> operations_to_execute,
    std::mutex *init_done_mutex,
    std::condition_variable *init_done_cond,
    bool *init_done,
    std::string *init_error) noexcept
{
    assert(!destination_directory_utf16.empty());

    std::replace(destination_directory_utf16.begin(), destination_directory_utf16.end(), L'/', L'\\');

    auto set_init_error_and_notify = [&](std::string const &err) {
        std::unique_lock lock(*init_done_mutex);
        *init_done = true;
        *init_error = err;
        init_done_cond->notify_one();
    };

    HRESULT result = {};

    result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(result)) {
        return set_init_error_and_notify(make_str("CoInitializeEx(COINIT_APARTMENTTHREADED), %s", _com_error(result).ErrorMessage()));
    }
    SCOPE_EXIT { CoUninitialize(); };

    IFileOperation *file_op = nullptr;

    result = CoCreateInstance(CLSID_FileOperation, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&file_op));
    if (FAILED(result)) {
        return set_init_error_and_notify(make_str("CoCreateInstance(CLSID_FileOperation), %s", _com_error(result).ErrorMessage()));
    }
    SCOPE_EXIT { file_op->Release(); };

    result = file_op->SetOperationFlags(FOF_RENAMEONCOLLISION | FOF_NOCONFIRMATION | FOF_ALLOWUNDO);
    if (FAILED(result)) {
        return set_init_error_and_notify(make_str("IFileOperation::SetOperationFlags, %s", _com_error(result).ErrorMessage()));
    }

    IShellItem *destination = nullptr;
    {
        swan_path_t destination_utf8 = path_create("");

        result = SHCreateItemFromParsingName(destination_directory_utf16.c_str(), nullptr, IID_PPV_ARGS(&destination));

        if (FAILED(result)) {
            HANDLE accessible = CreateFileW(
                destination_directory_utf16.c_str(),
                FILE_LIST_DIRECTORY,
                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                NULL,
                OPEN_EXISTING,
                FILE_FLAG_BACKUP_SEMANTICS,
                NULL);

            WCOUT_IF_DEBUG("FAILED: SHCreateItemFromParsingName [" << destination_directory_utf16.c_str() << "]\n");

            std::string error = {};

            if (!utf16_to_utf8(destination_directory_utf16.data(), destination_utf8.data(), destination_utf8.size())) {
                error = "SHCreateItemFromParsingName and conversion of destination path from UTF-16 to UTF-8";
            }
            else {
                if (accessible == INVALID_HANDLE_VALUE) {
                    error.append("file or directory is not accessible, maybe it is locked or has been moved/deleted? ");
                }
                error.append("SHCreateItemFromParsingName failed for [").append(destination_utf8.data()).append("]");
            }

            return set_init_error_and_notify(error);
        }
    }

    if (paths_to_execute_utf16.back() == L'\n') {
        paths_to_execute_utf16.pop_back();
    }

    explorer_file_op_progress_sink prog_sink = {};
    prog_sink.dst_expl_id = dst_expl_id;
    prog_sink.dst_expl_cwd_when_operation_started = global_state::explorers()[dst_expl_id].cwd;

    // attach items (IShellItem) for deletion to IFileOperation
    {
        auto items_to_execute = std::wstring_view(paths_to_execute_utf16.data()) | std::ranges::views::split('\n');
        std::stringstream err = {};
        std::wstring full_path_to_exec_utf16 = {};

        full_path_to_exec_utf16.reserve((global_state::page_size() / 2) - 1);

        u64 i = 0;
        for (auto item_utf16 : items_to_execute) {
            SCOPE_EXIT { ++i; };
            file_operation_type op_type = operations_to_execute[i];

            std::wstring_view view(item_utf16.begin(), item_utf16.end());
            full_path_to_exec_utf16 = view;

            // shlwapi doesn't like '/', force them all to '\'
            std::replace(full_path_to_exec_utf16.begin(), full_path_to_exec_utf16.end(), L'/', L'\\');

            swan_path_t item_path_utf8 = path_create("");

            IShellItem *to_exec = nullptr;
            result = SHCreateItemFromParsingName(full_path_to_exec_utf16.c_str(), nullptr, IID_PPV_ARGS(&to_exec));
            if (FAILED(result)) {
                HANDLE accessible = CreateFileW(
                    destination_directory_utf16.c_str(),
                    FILE_LIST_DIRECTORY,
                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    NULL,
                    OPEN_EXISTING,
                    FILE_FLAG_BACKUP_SEMANTICS,
                    NULL);

                WCOUT_IF_DEBUG("FAILED: SHCreateItemFromParsingName [" << full_path_to_exec_utf16.c_str() << "]\n");

                if (!utf16_to_utf8(full_path_to_exec_utf16.data(), item_path_utf8.data(), item_path_utf8.size())) {
                    err << "SHCreateItemFromParsingName and conversion of delete path from UTF-16 to UTF-8.\n";
                }
                else {
                    if (accessible == INVALID_HANDLE_VALUE) {
                        err << "File or directory is not accessible, maybe it is locked or has been moved/deleted? ";
                    }
                    err << "SHCreateItemFromParsingName failed for [" << item_path_utf8.data() << "].\n";
                }

                continue;
            }

            SCOPE_EXIT { to_exec->Release(); };

            char const *function = nullptr;
            switch (op_type) {
                case file_operation_type::copy:
                    result = file_op->CopyItem(to_exec, destination, nullptr, nullptr);
                    function = "CopyItem";
                    break;
                case file_operation_type::move:
                    result = file_op->MoveItem(to_exec, destination, nullptr, nullptr);
                    function = "MoveItem";
                    break;
                case file_operation_type::del:
                    result = file_op->DeleteItem(to_exec, nullptr);
                    function = "DeleteItem";
                    prog_sink.contains_delete_operations = true;
                    break;
            }

            if (FAILED(result)) {
                WCOUT_IF_DEBUG("FAILED: IFileOperation::" << function << " [" << full_path_to_exec_utf16.c_str() << "]\n");

                if (!utf16_to_utf8(full_path_to_exec_utf16.data(), item_path_utf8.data(), item_path_utf8.size())) {
                    err << "IFileOperation::" << function << " and conversion of delete path from UTF-16 to UTF-8.\n";
                } else {
                    err << "IFileOperation::" << function << " [" << item_path_utf8.data() << "].";
                }
            } else {
                // WCOUT_IF_DEBUG("file_op->" << function << " [" << full_path_to_exec_utf16.c_str() << "]\n");
            }
        }

        std::string errors = err.str();
        if (!errors.empty()) {
            errors.pop_back(); // remove trailing '\n'
            return set_init_error_and_notify(errors);
        }
    }

    DWORD cookie = {};
    result = file_op->Advise(&prog_sink, &cookie);
    if (FAILED(result)) {
        return set_init_error_and_notify(make_str("IFileOperation::Advise, %s", _com_error(result).ErrorMessage()));
    }
    print_debug_msg("IFileOperation::Advise(%d), %s", cookie, _com_error(result).ErrorMessage());

    set_init_error_and_notify(""); // init succeeded, no error

    result = file_op->PerformOperations();
    if (FAILED(result)) {
        print_debug_msg("FAILED IFileOperation::PerformOperations, %s", _com_error(result).ErrorMessage());
    }

    file_op->Unadvise(cookie);
    if (FAILED(result)) {
        print_debug_msg("FAILED IFileOperation::Unadvise(%d), %s", cookie, _com_error(result).ErrorMessage());
    }
}
