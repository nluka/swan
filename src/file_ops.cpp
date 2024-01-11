#include "stdafx.hpp"
#include "data_types.hpp"
#include "common_fns.hpp"
#include "imgui_specific.hpp"
#include "path.hpp"

static circular_buffer<file_operation> s_file_ops_buffer(100);

circular_buffer<file_operation> const &global_state::file_ops_buffer() noexcept
{
    return s_file_ops_buffer;
}

file_operation::file_operation(type op_type, u64 file_size, swan_path_t const &src, swan_path_t const &dst) noexcept
    : op_type(op_type)
    , src_path(src)
    , dest_path(dst)
{
    total_file_size.store(file_size);
}

file_operation::file_operation(file_operation const &other) noexcept // for boost::circular_buffer
    : op_type(other.op_type)
    , success(other.success)
    , src_path(other.src_path)
    , dest_path(other.dest_path)
{
    this->total_file_size.store(other.total_file_size.load());
    this->total_bytes_transferred.store(other.total_bytes_transferred.load());
    this->stream_size.store(other.stream_size.load());
    this->stream_bytes_transferred.store(other.stream_bytes_transferred.load());
    this->start_time.store(other.start_time.load());
    this->end_time.store(other.end_time.load());
}

file_operation &file_operation::operator=(file_operation const &other) noexcept // for boost::circular_buffer
{
    this->total_file_size.store(other.total_file_size.load());
    this->total_bytes_transferred.store(other.total_bytes_transferred.load());
    this->stream_size.store(other.stream_size.load());
    this->stream_bytes_transferred.store(other.stream_bytes_transferred.load());

    this->start_time.store(other.start_time.load());
    this->end_time.store(other.end_time.load());

    this->op_type = other.op_type;
    this->success = other.success;
    this->src_path = other.src_path;
    this->dest_path = other.dest_path;

    return *this;
}

void swan_windows::render_file_operations() noexcept
{
    if (!imgui::Begin(swan_windows::get_name(swan_windows::file_operations))) {
        imgui::End();
        return;
    }

    if (imgui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)) {
        global_state::save_focused_window(swan_windows::file_operations);
    }

    [[maybe_unused]] auto &io = imgui::GetIO();

    auto const &file_ops_buffer = global_state::file_ops_buffer();

    enum file_ops_table_col : s32
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

HRESULT progress_sink::PostMoveItem(
    [[maybe_unused]] DWORD flags,
    [[maybe_unused]] IShellItem *src_item,
    [[maybe_unused]] IShellItem *destination,
    LPCWSTR new_name_utf16,
    HRESULT,
    [[maybe_unused]] IShellItem *dst_item)
{
    print_debug_msg("PostMoveItem");
    explorer_window &dst_expl = global_state::explorers()[this->dst_expl_id];

    bool dst_expl_cwd_same = path_loosely_same(dst_expl.cwd, this->dst_expl_cwd_when_operation_started);

    if (dst_expl_cwd_same) {
        swan_path_t new_name_utf8;
        s32 written = utf16_to_utf8(new_name_utf16, new_name_utf8.data(), new_name_utf8.size());
        if (written == 0) {
            // TODO: error
        } else {
            print_debug_msg("new_name_utf8: [%s]", new_name_utf8.data());
            std::scoped_lock lock(dst_expl.entries_to_select_mutex);
            dst_expl.entries_to_select.push_back(new_name_utf8);
        }
    }

    return S_OK;
}

HRESULT progress_sink::PostCopyItem(DWORD, IShellItem *, IShellItem *, LPCWSTR, HRESULT, IShellItem *)
{
    print_debug_msg("PostCopyItem");
    return S_OK;
}

HRESULT progress_sink::FinishOperations(HRESULT) { print_debug_msg("FinishOperations"); return S_OK; }
HRESULT progress_sink::PauseTimer() { print_debug_msg("PauseTimer"); return S_OK; }
HRESULT progress_sink::PostDeleteItem(DWORD, IShellItem *, HRESULT, IShellItem *) { print_debug_msg("PostDeleteItem"); return S_OK; }
HRESULT progress_sink::PostNewItem(DWORD, IShellItem *, LPCWSTR, LPCWSTR, DWORD, HRESULT, IShellItem *) { print_debug_msg("PostNewItem"); return S_OK; }
HRESULT progress_sink::PostRenameItem(DWORD, IShellItem *, LPCWSTR, HRESULT, IShellItem *) { print_debug_msg("PostRenameItem"); return S_OK; }
HRESULT progress_sink::PreCopyItem(DWORD, IShellItem *, IShellItem *, LPCWSTR) { print_debug_msg("PreCopyItem"); return S_OK; }
HRESULT progress_sink::PreDeleteItem(DWORD, IShellItem *) { print_debug_msg("PreDeleteItem"); return S_OK; }
HRESULT progress_sink::PreMoveItem(DWORD, IShellItem *, IShellItem *, LPCWSTR) { print_debug_msg("PreMoveItem"); return S_OK; }
HRESULT progress_sink::PreNewItem(DWORD, IShellItem *, LPCWSTR) { print_debug_msg("PreNewItem"); return S_OK; }
HRESULT progress_sink::PreRenameItem(DWORD, IShellItem *, LPCWSTR) { print_debug_msg("PreRenameItem"); return S_OK; }
HRESULT progress_sink::ResetTimer() { print_debug_msg("ResetTimer"); return S_OK; }
HRESULT progress_sink::ResumeTimer() { print_debug_msg("ResumeTimer"); return S_OK; }
HRESULT progress_sink::StartOperations() { print_debug_msg("StartOperations"); return S_OK; }
HRESULT progress_sink::UpdateProgress(UINT work_total, UINT work_so_far) { print_debug_msg("UpdateProgress %zu/%zu", work_so_far, work_total); return S_OK; }

ULONG progress_sink::AddRef() { return 1; }
ULONG progress_sink::Release() { return 1; }

HRESULT progress_sink::QueryInterface(const IID &riid, void **ppv)
{
    if (riid == IID_IUnknown || riid == IID_IFileOperationProgressSink) {
        *ppv = static_cast<IFileOperationProgressSink*>(this);
        return S_OK;
    }
    *ppv = nullptr;
    return E_NOINTERFACE;
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
    std::vector<char> operations_to_execute,
    std::mutex *init_done_mutex,
    std::condition_variable *init_done_cond,
    bool *init_done,
    std::string *init_error) noexcept
{
    assert(!destination_directory_utf16.empty());
    if (!StrChrW(L"\\/", destination_directory_utf16.back())) {
        destination_directory_utf16 += L'\\';
    }

    auto set_init_error_and_notify = [&](char const *err) {
        std::unique_lock lock(*init_done_mutex);
        *init_done = true;
        *init_error = err;
        init_done_cond->notify_one();
    };

    HRESULT result = {};

    result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(result)) {
        return set_init_error_and_notify("CoInitializeEx(COINIT_APARTMENTTHREADED)");
    }
    SCOPE_EXIT { CoUninitialize(); };

    IFileOperation *file_op = nullptr;

    result = CoCreateInstance(CLSID_FileOperation, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&file_op));
    if (FAILED(result)) {
        return set_init_error_and_notify("CoCreateInstance(CLSID_FileOperation)");
    }
    SCOPE_EXIT { file_op->Release(); };

    result = file_op->SetOperationFlags(FOF_RENAMEONCOLLISION | FOF_NOCONFIRMATION | FOF_ALLOWUNDO);
    if (FAILED(result)) {
        return set_init_error_and_notify("IFileOperation::SetOperationFlags");
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
            s32 written = utf16_to_utf8(destination_directory_utf16.data(), destination_utf8.data(), destination_utf8.size());
            std::string error = {};
            if (written == 0) {
                error = "SHCreateItemFromParsingName and conversion of destination path from UTF-16 to UTF-8";
            } else {
                if (accessible == INVALID_HANDLE_VALUE) {
                    error.append("file or directory is not accessible, maybe it is locked or has been moved/deleted? ");
                }
                error.append("SHCreateItemFromParsingName failed for [").append(destination_utf8.data()).append("]");
            }
            return set_init_error_and_notify(error.c_str());
        }
    }

    if (paths_to_execute_utf16.back() == L'\n') {
        paths_to_execute_utf16.pop_back();
    }

    // attach items (IShellItem) for deletion to IFileOperation
    {
        auto items_to_execute = std::wstring_view(paths_to_execute_utf16.data()) | std::ranges::views::split('\n');
        std::stringstream err = {};
        std::wstring full_path_to_exec_utf16 = {};

        full_path_to_exec_utf16.reserve((global_state::page_size() / 2) - 1);

        u64 i = 0;
        for (auto item_utf16 : items_to_execute) {
            SCOPE_EXIT { ++i; };
            char operation_code = operations_to_execute[i];

            std::wstring_view view(item_utf16.begin(), item_utf16.end());
            full_path_to_exec_utf16 = view;

            // shlwapi doesn't like '/', force them all to '\'
            std::replace(full_path_to_exec_utf16.begin(), full_path_to_exec_utf16.end(), L'/', L'\\');

            swan_path_t item_path_utf8 = path_create("");
            s32 written = 0;

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
                written = utf16_to_utf8(full_path_to_exec_utf16.data(), item_path_utf8.data(), item_path_utf8.size());
                if (written == 0) {
                    err << "SHCreateItemFromParsingName and conversion of delete path from UTF-16 to UTF-8.\n";
                } else {
                    if (accessible == INVALID_HANDLE_VALUE) {
                        err << "File or directory is not accessible, maybe it is locked or has been moved/deleted? ";
                    }
                    err << "SHCreateItemFromParsingName failed for [" << item_path_utf8.data() << "].\n";
                }
                continue;
            }

            SCOPE_EXIT { to_exec->Release(); };

            char const *function = nullptr;
            switch (operation_code) {
                case 'C':
                    result = file_op->CopyItem(to_exec, destination, nullptr, nullptr);
                    function = "CopyItem";
                    break;
                case 'X':
                    result = file_op->MoveItem(to_exec, destination, nullptr, nullptr);
                    function = "MoveItem";
                    break;
                case 'D':
                    result = file_op->DeleteItem(to_exec, nullptr);
                    function = "DeleteItem";
                    break;
            }

            if (FAILED(result)) {
                WCOUT_IF_DEBUG("FAILED: IFileOperation::" << function << " [" << full_path_to_exec_utf16.c_str() << "]\n");
                written = utf16_to_utf8(full_path_to_exec_utf16.data(), item_path_utf8.data(), item_path_utf8.size());
                if (written == 0) {
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
            return set_init_error_and_notify(errors.c_str());
        }
    }

    progress_sink prog_sink;
    prog_sink.dst_expl_id = dst_expl_id;
    prog_sink.dst_expl_cwd_when_operation_started = global_state::explorers()[dst_expl_id].cwd;
    DWORD cookie = {};

    result = file_op->Advise(&prog_sink, &cookie);
    if (FAILED(result)) {
        return set_init_error_and_notify("IFileOperation::Advise(IFileOperationProgressSink *pfops, DWORD *pdwCookie)");
    }
    print_debug_msg("IFileOperation::Advise(%d)", cookie);

    set_init_error_and_notify(""); // init succeeded, no error

    result = file_op->PerformOperations();
    if (FAILED(result)) {
        print_debug_msg("FAILED IFileOperation::PerformOperations()");
    }

    file_op->Unadvise(cookie);
    if (FAILED(result)) {
        print_debug_msg("FAILED IFileOperation::Unadvise(%d)", cookie);
    }
}
