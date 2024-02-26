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

bool global_state::save_completed_file_ops_to_disk() noexcept
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
        std::scoped_lock lock(mutex);

        for (auto const &file_op : completed_operations) {
            auto time_t = std::chrono::system_clock::to_time_t(file_op.completion_time);
            std::tm tm = *std::localtime(&time_t);

            out
                << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << ' '
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

        std::tm tm = {};
        iss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
        system_time_point_t stored_time = std::chrono::system_clock::from_time_t(std::mktime(&tm));
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
        completed_operations.back().completion_time = stored_time;
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
        // file_ops_table_col_action,
        file_ops_table_col_group,
        file_ops_table_col_op_type,
        file_ops_table_col_completion_time,
        file_ops_table_col_src_path,
        file_ops_table_col_dst_path,
        file_ops_table_col_count
    };

    if (imgui::BeginTable("Activities", file_ops_table_col_count,
        ImGuiTableFlags_Resizable|ImGuiTableFlags_SizingStretchProp|
        (global_state::settings().cwd_entries_table_alt_row_bg ? ImGuiTableFlags_RowBg : 0))
    ) {
        // imgui::TableSetupColumn("Action", ImGuiTableColumnFlags_NoSort, 0.0f, file_ops_table_col_action);
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

        for (u64 i = 0; i < completed_operations.size(); ++i) {
        // for (auto const &file_op : completed_operations) {
            auto &file_op = completed_operations[i];

            imgui::TableNextRow();

            // if (imgui::TableSetColumnIndex(file_ops_table_col_action)) {
            //     imgui::ScopedDisable d(true);
            //     imgui::SmallButton("Undo");
            // }

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
                imgui::TextUnformatted(when_completed_str.data());
            }

            if (imgui::TableSetColumnIndex(file_ops_table_col_src_path)) {
                char buffer[2048];
                (void) snprintf(buffer, lengthof(buffer), "%s##%zu", file_op.src_path.data(), i);

                imgui::Selectable(buffer, &file_op.selected, ImGuiSelectableFlags_SpanAllColumns);
                // imgui::TextUnformatted(file_op.src_path.data());

                if (imgui::IsItemClicked()) {
                    // TODO: find most appropriate explorer and spotlight the item
                }
            }
            imgui::RenderTooltipWhenColumnTextTruncated(file_ops_table_col_src_path, file_op.src_path.data());

            if (imgui::TableSetColumnIndex(file_ops_table_col_dst_path)) {
                imgui::TextUnformatted(file_op.dst_path.data());
                if (imgui::IsItemClicked()) {
                    // TODO: find most appropriate explorer and spotlight the item
                }
            }
            imgui::RenderTooltipWhenColumnTextTruncated(file_ops_table_col_dst_path, file_op.dst_path.data());
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

HRESULT progress_sink::StartOperations() { print_debug_msg("StartOperations"); return S_OK; }
HRESULT progress_sink::PauseTimer() { print_debug_msg("PauseTimer"); return S_OK; }
HRESULT progress_sink::ResetTimer() { print_debug_msg("ResetTimer"); return S_OK; }
HRESULT progress_sink::ResumeTimer() { print_debug_msg("ResumeTimer"); return S_OK; }

HRESULT progress_sink::PostNewItem(DWORD, IShellItem *, LPCWSTR, LPCWSTR, DWORD, HRESULT, IShellItem *) { print_debug_msg("PostNewItem"); return S_OK; }
HRESULT progress_sink::PostRenameItem(DWORD, IShellItem *, LPCWSTR, HRESULT, IShellItem *) { print_debug_msg("PostRenameItem"); return S_OK; }

HRESULT progress_sink::PreCopyItem(DWORD, IShellItem *, IShellItem *, LPCWSTR) { print_debug_msg("PreCopyItem"); return S_OK; }
HRESULT progress_sink::PreDeleteItem(DWORD, IShellItem *) { print_debug_msg("PreDeleteItem"); return S_OK; }
HRESULT progress_sink::PreMoveItem(DWORD, IShellItem *, IShellItem *, LPCWSTR) { print_debug_msg("PreMoveItem"); return S_OK; }
HRESULT progress_sink::PreNewItem(DWORD, IShellItem *, LPCWSTR) { print_debug_msg("PreNewItem"); return S_OK; }
HRESULT progress_sink::PreRenameItem(DWORD, IShellItem *, LPCWSTR) { print_debug_msg("PreRenameItem"); return S_OK; }

HRESULT progress_sink::PostMoveItem(
    [[maybe_unused]] DWORD flags,
    [[maybe_unused]] IShellItem *src_item,
    [[maybe_unused]] IShellItem *destination,
    LPCWSTR new_name_utf16,
    HRESULT result,
    [[maybe_unused]] IShellItem *dst_item)
{
    if (!SUCCEEDED(result)) {
        return S_OK;
    }

    SFGAOF attributes = {};
    if (FAILED(src_item->GetAttributes(SFGAO_FOLDER|SFGAO_LINK, &attributes))) {
        print_debug_msg("FAILED IShellItem::GetAttributes(SFGAO_FOLDER|SFGAO_LINK)");
        return S_OK;
    }

    wchar_t *src_path_utf16 = nullptr;
    swan_path_t src_path_utf8;

    if (FAILED(src_item->GetDisplayName(SIGDN_FILESYSPATH, &src_path_utf16))) {
        return S_OK;
    }
    SCOPE_EXIT { CoTaskMemFree(src_path_utf16); };

    if (!utf16_to_utf8(src_path_utf16, src_path_utf8.data(), src_path_utf8.max_size())) {
        return S_OK;
    }

    wchar_t *dst_path_utf16 = nullptr;
    swan_path_t dst_path_utf8;

    if (FAILED(dst_item->GetDisplayName(SIGDN_FILESYSPATH, &dst_path_utf16))) {
        return S_OK;
    }
    SCOPE_EXIT { CoTaskMemFree(dst_path_utf16); };

    if (!utf16_to_utf8(dst_path_utf16, dst_path_utf8.data(), dst_path_utf8.max_size())) {
        return S_OK;
    }

    print_debug_msg("PostMoveItem [%s] -> [%s]", src_path_utf8.data(), dst_path_utf8.data());

    swan_path_t new_name_utf8;

    if (!utf16_to_utf8(new_name_utf16, new_name_utf8.data(), new_name_utf8.size())) {
        return S_OK;
    }

    explorer_window &dst_expl = global_state::explorers()[this->dst_expl_id];

    bool dst_expl_cwd_same = path_loosely_same(dst_expl.cwd, this->dst_expl_cwd_when_operation_started);

    if (dst_expl_cwd_same) {
        print_debug_msg("PostMoveItem [%s]", new_name_utf8.data());
        std::scoped_lock lock(dst_expl.select_cwd_entries_on_next_update_mutex);
        dst_expl.select_cwd_entries_on_next_update.push_back(new_name_utf8);
    }

    {
        auto pair = global_state::completed_file_ops();
        auto &completed_file_ops = *pair.first;
        auto &mutex = *pair.second;

        path_force_separator(src_path_utf8, global_state::settings().dir_separator_utf8);
        path_force_separator(dst_path_utf8, global_state::settings().dir_separator_utf8);

        std::scoped_lock lock(mutex);

        completed_file_ops.push_front();
        completed_file_ops.front().completion_time = current_time_system();
        completed_file_ops.front().src_path = src_path_utf8;
        completed_file_ops.front().dst_path = dst_path_utf8;
        completed_file_ops.front().op_type = file_operation_type::move;

        if (attributes & SFGAO_LINK) {
            completed_file_ops.front().obj_type = basic_dirent::kind::symlink_ambiguous;
        } else {
            completed_file_ops.front().obj_type = attributes & SFGAO_FOLDER ? basic_dirent::kind::directory : basic_dirent::kind::file;
        }
    }

    return S_OK;
}

HRESULT progress_sink::PostDeleteItem(DWORD, IShellItem *item, HRESULT result, IShellItem *item_newly_created)
{
    if (FAILED(result)) {
        return S_OK;
    }

    // print_SIGDN_values("PostDeleteItem", "item", item);
    // print_SIGDN_values("PostDeleteItem", "item_newly_created", item_newly_created);

    // Extract deleted item path, UTF16
    wchar_t *deleted_item_path_utf16 = nullptr;
    if (FAILED(item->GetDisplayName(SIGDN_FILESYSPATH, &deleted_item_path_utf16))) {
        return S_OK;
    }
    SCOPE_EXIT { CoTaskMemFree(deleted_item_path_utf16); };

    // Convert deleted item path to UTF8
    swan_path_t deleted_item_path_utf8;
    if (!utf16_to_utf8(deleted_item_path_utf16, deleted_item_path_utf8.data(), deleted_item_path_utf8.max_size())) {
        return S_OK;
    }

    // Extract recycle bin item path, UTF16
    wchar_t *recycle_bin_hardlink_path_utf16 = nullptr;
    if (FAILED(item_newly_created->GetDisplayName(SIGDN_FILESYSPATH, &recycle_bin_hardlink_path_utf16))) {
        return S_OK;
    }
    SCOPE_EXIT { CoTaskMemFree(recycle_bin_hardlink_path_utf16); };

    // Convert recycle bin item path to UTF8
    swan_path_t recycle_bin_item_path_utf8;
    if (!utf16_to_utf8(recycle_bin_hardlink_path_utf16, recycle_bin_item_path_utf8.data(), recycle_bin_item_path_utf8.max_size())) {
        return S_OK;
    }

    SFGAOF attributes = {};
    if (FAILED(item->GetAttributes(SFGAO_FOLDER|SFGAO_LINK, &attributes))) {
        print_debug_msg("FAILED IShellItem::GetAttributes(SFGAO_FOLDER|SFGAO_LINK)");
        return S_OK;
    }

    {
        auto pair = global_state::completed_file_ops();
        auto &completed_file_ops = *pair.first;
        auto &mutex = *pair.second;

        std::scoped_lock lock(mutex);

        completed_file_ops.push_front();
        completed_file_ops.front().completion_time = current_time_system();
        completed_file_ops.front().src_path = deleted_item_path_utf8;
        completed_file_ops.front().dst_path = recycle_bin_item_path_utf8;
        completed_file_ops.front().op_type = file_operation_type::del;

        if (attributes & SFGAO_LINK) {
            completed_file_ops.front().obj_type = basic_dirent::kind::symlink_ambiguous;
        } else {
            completed_file_ops.front().obj_type = attributes & SFGAO_FOLDER ? basic_dirent::kind::directory : basic_dirent::kind::file;
        }
    }

    print_debug_msg("PostDeleteItem [%s] [%s]", deleted_item_path_utf8.data(), recycle_bin_item_path_utf8.data());

    return S_OK;
}

HRESULT progress_sink::PostCopyItem(DWORD, IShellItem *src_item, IShellItem *, LPCWSTR new_name_utf16, HRESULT result, IShellItem *dst_item)
{
    if (FAILED(result)) {
        return S_OK;
    }

    SFGAOF attributes = {};
    if (FAILED(src_item->GetAttributes(SFGAO_FOLDER|SFGAO_LINK, &attributes))) {
        print_debug_msg("FAILED IShellItem::GetAttributes(SFGAO_FOLDER|SFGAO_LINK)");
        return S_OK;
    }

    wchar_t *src_path_utf16 = nullptr;
    swan_path_t src_path_utf8;

    if (FAILED(src_item->GetDisplayName(SIGDN_PARENTRELATIVEEDITING, &src_path_utf16))) {
        return S_OK;
    }
    SCOPE_EXIT { CoTaskMemFree(src_path_utf16); };

    if (!utf16_to_utf8(src_path_utf16, src_path_utf8.data(), src_path_utf8.max_size())) {
        return S_OK;
    }

    wchar_t *dst_path_utf16 = nullptr;
    swan_path_t dst_path_utf8;

    if (FAILED(dst_item->GetDisplayName(SIGDN_PARENTRELATIVEEDITING, &dst_path_utf16))) {
        return S_OK;
    }
    SCOPE_EXIT { CoTaskMemFree(dst_path_utf16); };

    if (!utf16_to_utf8(dst_path_utf16, dst_path_utf8.data(), dst_path_utf8.max_size())) {
        return S_OK;
    }

    print_debug_msg("PostCopyItem [%s] -> [%s]", src_path_utf8.data(), dst_path_utf8.data());

    swan_path_t new_name_utf8;

    if (!utf16_to_utf8(new_name_utf16, new_name_utf8.data(), new_name_utf8.size())) {
        return S_OK;
    }

    {
        auto pair = global_state::completed_file_ops();
        auto &completed_file_ops = *pair.first;
        auto &mutex = *pair.second;

        path_force_separator(src_path_utf8, global_state::settings().dir_separator_utf8);
        path_force_separator(dst_path_utf8, global_state::settings().dir_separator_utf8);

        std::scoped_lock lock(mutex);

        completed_file_ops.push_front();
        completed_file_ops.front().completion_time = current_time_system();
        completed_file_ops.front().src_path = src_path_utf8;
        completed_file_ops.front().dst_path = dst_path_utf8;
        completed_file_ops.front().op_type = file_operation_type::copy;

        if (attributes & SFGAO_LINK) {
            completed_file_ops.front().obj_type = basic_dirent::kind::symlink_ambiguous;
        } else {
            completed_file_ops.front().obj_type = attributes & SFGAO_FOLDER ? basic_dirent::kind::directory : basic_dirent::kind::file;
        }
    }

    return S_OK;
}

HRESULT progress_sink::UpdateProgress(UINT work_total, UINT work_so_far)
{
    print_debug_msg("UpdateProgress %zu/%zu", work_so_far, work_total);
    return S_OK;
}

HRESULT progress_sink::FinishOperations(HRESULT)
{
    print_debug_msg("FinishOperations");

    if (this->contains_delete_operations) {
        {
            auto new_buffer = circular_buffer<recent_file>(global_constants::MAX_RECENT_FILES);

            auto pair = global_state::recent_files();
            auto &recent_files = *pair.first;
            auto &mutex = *pair.second;

            std::scoped_lock lock(mutex);

            for (auto const &recent_file : recent_files) {
                wchar_t recent_file_path_utf16[MAX_PATH];

                if (utf8_to_utf16(recent_file.path.data(), recent_file_path_utf16, lengthof(recent_file_path_utf16))) {
                    if (PathFileExistsW(recent_file_path_utf16)) {
                        new_buffer.push_back(recent_file);
                    }
                }
            }

            recent_files = new_buffer;
        }

        global_state::save_recent_files_to_disk();
    }

    (void) global_state::save_completed_file_ops_to_disk();

    return S_OK;
}

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
    std::vector<file_operation_type> operations_to_execute,
    std::mutex *init_done_mutex,
    std::condition_variable *init_done_cond,
    bool *init_done,
    std::string *init_error) noexcept
{
    assert(!destination_directory_utf16.empty());

    std::replace(destination_directory_utf16.begin(), destination_directory_utf16.end(), L'/', L'\\');

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

            return set_init_error_and_notify(error.c_str());
        }
    }

    if (paths_to_execute_utf16.back() == L'\n') {
        paths_to_execute_utf16.pop_back();
    }

    progress_sink prog_sink = {};
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
            return set_init_error_and_notify(errors.c_str());
        }
    }

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
