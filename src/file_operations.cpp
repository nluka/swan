#include "stdafx.hpp"
#include "common_functions.hpp"
#include "imgui_dependent_functions.hpp"
#include "path.hpp"

static std::mutex g_completed_file_ops_mutex = {};
static std::deque<completed_file_operation> g_completed_file_ops(1000);
static file_operation_command_buf g_file_op_payload = {};

global_state::completed_file_operations global_state::completed_file_operations_get() noexcept
{
    return { &g_completed_file_ops, &g_completed_file_ops_mutex };
}

file_operation_command_buf &global_state::file_op_cmd_buf() noexcept
{
    return g_file_op_payload;
}

void erase(global_state::completed_file_operations &obj,
           std::deque<completed_file_operation>::iterator first,
           std::deque<completed_file_operation>::iterator last) noexcept
{
    for (auto iter = first; iter != last; ++iter) {
        if (iter->src_icon_GLtexID > 0) delete_icon_texture(iter->src_icon_GLtexID, "completed_file_operation");
        if (iter->dst_icon_GLtexID > 0) delete_icon_texture(iter->dst_icon_GLtexID, "completed_file_operation");
    }
    obj.container->erase(first, last);
}

void pop_back(global_state::completed_file_operations &obj) noexcept
{
    if (obj.container->back().src_icon_GLtexID > 0) delete_icon_texture(obj.container->back().src_icon_GLtexID, "completed_file_operation");
    if (obj.container->back().dst_icon_GLtexID > 0) delete_icon_texture(obj.container->back().dst_icon_GLtexID, "completed_file_operation");
    obj.container->pop_back();
}

u32 global_state::completed_file_operations_calc_next_group_id() noexcept
{
    u32 max_group_id = 0;
    u32 reserved_nil_value = std::numeric_limits<decltype(max_group_id)>::max();

    std::scoped_lock lock(g_completed_file_ops_mutex);

    for (auto const &file_op : g_completed_file_ops) {
        if (file_op.group_id > max_group_id) {
            max_group_id = file_op.group_id;
        }
    }

    if (max_group_id == reserved_nil_value) {
        return 1; // wrap around
    } else {
        return max_group_id + 1;
    }
}

bool global_state::completed_file_operations_save_to_disk(std::scoped_lock<std::mutex> *supplied_lock) noexcept
try {
    std::filesystem::path full_path = global_state::execution_path() / "data\\completed_file_operations.txt";

    std::ofstream out(full_path);

    if (!out) {
        return false;
    }

    auto completed_file_operations = global_state::completed_file_operations_get();

    {
        auto lock = supplied_lock ? std::unique_lock<std::mutex>() : std::unique_lock<std::mutex>(*completed_file_operations.mutex);

        for (auto const &file_op : *completed_file_operations.container) {
            out
                << std::chrono::system_clock::to_time_t(file_op.completion_time) << ' '
                << std::chrono::system_clock::to_time_t(file_op.undo_time) << ' '
                << u32(file_op.group_id) << ' '
                << char(file_op.op_type) << ' '
                << s32(file_op.obj_type) << ' '
                << path_length(file_op.src_path) << ' '
                << file_op.src_path.data() << ' '
                << path_length(file_op.dst_path) << ' '
                << file_op.dst_path.data() << '\n';
        }
    }

    print_debug_msg("SUCCESS");
    return true;
}
catch (std::exception const &except) {
    print_debug_msg("FAILED catch(std::exception) %s", except.what());
    return false;
}
catch (...) {
    print_debug_msg("FAILED catch(...)");
    return false;
}

std::pair<bool, u64> global_state::completed_file_operations_load_from_disk(char dir_separator) noexcept
try {
    auto completed_file_operations = global_state::completed_file_operations_get();

    std::scoped_lock lock(*completed_file_operations.mutex);
    completed_file_operations.container->clear();

    std::filesystem::path full_path = global_state::execution_path() / "data\\completed_file_operations.txt";

    std::ifstream in(full_path);

    if (!in) {
        return { false, 0 };
    }

    std::string line = {};
    line.reserve(global_state::page_size() - 1);

    u64 num_loaded_successfully = 0;

    while (std::getline(in, line)) {
        std::istringstream iss(line);

        u32 stored_group_id = 0;
        char stored_op_type = 0;
        s32 stored_obj_type = 0;
        u64 stored_src_path_len = 0;
        u64 stored_dst_path_len = 0;
        swan_path stored_src_path = {};
        swan_path stored_dst_path = {};

        time_point_system_t stored_time_completion = extract_system_time_from_istream(iss);
        iss.ignore(1);

        time_point_system_t stored_time_undo = extract_system_time_from_istream(iss);
        iss.ignore(1);

        iss >> stored_group_id;
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

        completed_file_operations.container->emplace_back(stored_time_completion, stored_time_undo, file_operation_type(stored_op_type),
                                                          stored_src_path.data(), stored_dst_path.data(), basic_dirent::kind(stored_obj_type), stored_group_id);
        ++num_loaded_successfully;

        line.clear();
    }

    print_debug_msg("SUCCESS loaded %zu records", num_loaded_successfully);
    return { true, num_loaded_successfully };
}
catch (std::exception const &except) {
    print_debug_msg("FAILED catch(std::exception) %s", except.what());
    return { false, 0 };
}
catch (...) {
    print_debug_msg("FAILED catch(...)");
    return { false, 0 };
}

completed_file_operation::completed_file_operation() noexcept
    // : completion_time()
    // , src_path()
    // , dst_path()
    // , op_type()
    // , obj_type()
{
}

completed_file_operation::completed_file_operation(time_point_system_t completion_time, time_point_system_t undo_time, file_operation_type op_type,
                                                   char const *src, char const *dst, basic_dirent::kind obj_type, u32 group_id) noexcept
    : src_icon_GLtexID(0)
    , dst_icon_GLtexID(0)
    , src_icon_size()
    , dst_icon_size()
    , completion_time(completion_time)
    , undo_time(undo_time)
    , group_id(group_id)
    , src_path(path_create(src))
    , dst_path(path_create(dst))
    , op_type(op_type)
    , obj_type(obj_type)
    , selected(false)
{
}

completed_file_operation::completed_file_operation(completed_file_operation const &other) noexcept
    : src_icon_GLtexID(other.src_icon_GLtexID)
    , dst_icon_GLtexID(other.dst_icon_GLtexID)
    , src_icon_size(other.src_icon_size)
    , dst_icon_size(other.dst_icon_size)
    , completion_time(other.completion_time)
    , undo_time(other.undo_time)
    , group_id(other.group_id)
    , src_path(other.src_path)
    , dst_path(other.dst_path)
    , op_type(other.op_type)
    , obj_type(other.obj_type)
    , selected(other.selected)
{
}

completed_file_operation &completed_file_operation::operator=(completed_file_operation const &other) noexcept // for boost::circular_buffer
{
    this->src_icon_GLtexID = other.src_icon_GLtexID;
    this->dst_icon_GLtexID = other.dst_icon_GLtexID;
    this->src_icon_size = other.src_icon_size;
    this->dst_icon_size = other.dst_icon_size;
    this->completion_time = other.completion_time;
    this->undo_time = other.undo_time;
    this->group_id = other.group_id;
    this->src_path = other.src_path;
    this->dst_path = other.dst_path;
    this->op_type = other.op_type;
    this->obj_type = other.obj_type;
    this->selected = other.selected;

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
        wchar_t *metadata_file_name = path_find_filename(recycle_bin_metadata_path_utf16);
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
    swan_path directory_path_in_recycle_bin_utf8,
    swan_path destination_dir_path_utf8,
    swan_path destination_full_path_utf8,
    std::mutex *init_done_mutex,
    std::condition_variable *init_done_cond,
    bool *init_done,
    std::string *init_error) noexcept
{
    auto set_init_error_and_notify = [&](std::string const &err) noexcept {
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

    wchar_t const *restored_name = path_cfind_filename(destination_dir_path_utf16);
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
        initialization_done_cond.wait(lock, [&]() noexcept { return initialization_done; });
    }

    return { initialization_error.empty(), initialization_error };
}

u64 deselect_all(std::deque<completed_file_operation> &completed_operations) noexcept
{
    u64 num_deselected = 0;

    for (auto &cfo : completed_operations) {
        bool prev = cfo.selected;
        bool &curr = cfo.selected;
        curr = false;
        num_deselected += curr != prev;
    }

    return num_deselected;
}

bool swan_windows::render_file_operations(bool &open, bool any_popups_open) noexcept
{
    if (!imgui::Begin(swan_windows::get_name(swan_windows::id::file_operations), &open)) {
        return false;
    }

    auto &io = imgui::GetIO();
    auto const &style = imgui::GetStyle();
    bool window_hovered = imgui::IsWindowHovered(ImGuiFocusedFlags_ChildWindows);
    auto completed_file_operations = global_state::completed_file_operations_get();
    auto &settings = global_state::settings();
    bool settings_change = false;

    // handle keybind actions
    if (!any_popups_open && window_hovered) {
        if (imgui::IsKeyPressed(ImGuiKey_Escape)) {
            std::scoped_lock lock(*completed_file_operations.mutex);
            deselect_all(*completed_file_operations.container);
        }
        else if (io.KeyCtrl && imgui::IsKeyPressed(ImGuiKey_A)) {
            std::scoped_lock lock(*completed_file_operations.mutex);
            for (auto &cfo : *completed_file_operations.container) {
                cfo.selected = true;
            }
        }
        else if (io.KeyCtrl && imgui::IsKeyPressed(ImGuiKey_I)) {
            std::scoped_lock lock(*completed_file_operations.mutex);
            for (auto &cfo : *completed_file_operations.container) {
                flip_bool(cfo.selected);
            }
        }
    }

    std::string dummy_buf = {};
    bool search_text_edited;

    imgui::TableNextColumn();
    {
        imgui::ScopedDisable d(true);
        imgui::ScopedItemFlag no_nav(ImGuiItemFlags_NoNav, true);
        imgui::ScopedItemWidth w(imgui::CalcTextSize("123456789_123456789_123456789_").x);
        search_text_edited = imgui::InputTextWithHint("## completed_file_operations search", ICON_CI_SEARCH " TODO", &dummy_buf);
    }

    imgui::SameLineSpaced(1);
    {
        auto help = render_help_indicator(true);

        if (help.hovered && imgui::BeginTooltip()) {
            imgui::AlignTextToFramePadding();
            imgui::TextUnformatted("[File Operations] Help");
            imgui::Separator();

            imgui::TextUnformatted("- Right click a record to open context menu");
            imgui::TextUnformatted("- Hold Shift + Hover Source/Destination to see full path");

            imgui::EndTooltip();
        }
    }

    imgui::SameLineSpaced(0);
    {
        imgui::ScopedDisable d(completed_file_operations.container->empty());
        imgui::ScopedItemFlag no_nav(ImGuiItemFlags_NoNav, true);

        if (imgui::Button(ICON_CI_CLEAR_ALL "## file_operations")) {
            imgui::OpenConfirmationModalWithCallback(
                /* confirmation_id  = */ swan_id_confirm_completed_file_operations_forget_all,
                /* confirmation_msg = */ "Are you sure you want to delete your ENTIRE file operations history? This action cannot be undone.",
                /* on_yes_callback  = */
                [completed_file_operations]() noexcept {
                    std::scoped_lock lock(*completed_file_operations.mutex);
                    completed_file_operations.container->clear();
                    (void) global_state::completed_file_operations_save_to_disk(&lock);
                    (void) global_state::settings().save_to_disk();
                },
                /* confirmation_enabled = */ &(global_state::settings().confirm_completed_file_operations_forget_all)
            );
        }
        if (imgui::IsItemHovered()) imgui::SetTooltip("Clear %zu records", completed_file_operations.container->size());
    }

    imgui::SameLineSpaced(1);
    {
        imgui::ScopedItemWidth iw(imgui::CalcTextSize("2147483647").x + style.FramePadding.x*2);
        imgui::ScopedItemFlag no_nav(ImGuiItemFlags_NoNav, true);
        settings_change |= imgui::InputInt("Max records", &settings.num_max_file_operations, 0);
        settings.num_max_file_operations = std::clamp(settings.num_max_file_operations, 0, INT32_MAX);
        // TODO get user confirmation to delete records beyond max after value reduction
    }
    imgui::SameLineSpaced(1);
    {
        imgui::ScopedItemFlag no_nav(ImGuiItemFlags_NoNav, true);
        settings_change |= imgui::Checkbox("Full src path", &settings.file_operations_src_path_full);
        imgui::SameLineSpaced(1);
        settings_change |= imgui::Checkbox("Full dst path", &settings.file_operations_dst_path_full);
    }

    enum file_ops_table_col : s32
    {
        file_ops_table_col_group,
        file_ops_table_col_op_type,
        file_ops_table_col_completion_time,
        file_ops_table_col_src_path,
        file_ops_table_col_dst_path,
        file_ops_table_col_count
    };

    s32 table_flags =
        ImGuiTableFlags_SizingStretchProp|
        ImGuiTableFlags_Resizable|
        ImGuiTableFlags_BordersV|
        ImGuiTableFlags_Reorderable|
        ImGuiTableFlags_ScrollY|
        (global_state::settings().tables_alt_row_bg ? ImGuiTableFlags_RowBg : 0)|
        (global_state::settings().table_borders_in_body ? 0 : ImGuiTableFlags_NoBordersInBody)
    ;
    imgui::ScopedItemFlag no_nav(ImGuiItemFlags_NoNav, true);

    if (imgui::BeginTable("completed_file_operations table", file_ops_table_col_count, table_flags)) {
        static std::optional< std::deque<completed_file_operation>::iterator > s_context_menu_target_iter = std::nullopt;
                              std::deque<completed_file_operation>::iterator   remove_single_iter         = completed_file_operations.container->end();

        static std::optional<ImRect> s_context_menu_target_rect = std::nullopt;
        static bool s_context_menu_initiated_on_group_col = false;

        static u64 s_num_selected_when_context_menu_opened = 0;
        static u64 s_num_restorables_selected_when_context_menu_opened = 0;
        static u64 s_num_restorables_in_group_when_context_menu_opened = 0;
        static u64 s_latest_selected_row_idx = u64(-1);

        static ImVec2 s_last_known_icon_size = {};

        imgui::TableSetupColumn("Group", ImGuiTableColumnFlags_NoSort, 0.0f, file_ops_table_col_group);
        imgui::TableSetupColumn("Type", ImGuiTableColumnFlags_NoSort, 0.0f, file_ops_table_col_op_type);
        imgui::TableSetupColumn("Completed", ImGuiTableColumnFlags_NoSort, 0.0f, file_ops_table_col_completion_time);
        imgui::TableSetupColumn("Source Path", ImGuiTableColumnFlags_NoSort|ImGuiTableColumnFlags_NoHide, 0.0f, file_ops_table_col_src_path);
        imgui::TableSetupColumn("Destination Path", ImGuiTableColumnFlags_NoSort, 0.0f, file_ops_table_col_dst_path);
        ImGui::TableSetupScrollFreeze(0, 1);
        imgui::TableHeadersRow();

        std::scoped_lock completed_file_ops_lock(*completed_file_operations.mutex);

        ImGuiListClipper clipper;
        {
            u64 num_rows_to_render = completed_file_operations.container->size();
            assert(num_rows_to_render <= (u64)INT32_MAX);
            clipper.Begin(s32(num_rows_to_render));
        }

        u32 group_block = 0;

        while (clipper.Step())
        for (u64 i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
            auto elem_iter = completed_file_operations.container->begin() + i;
            auto &file_op  = *elem_iter;

            imgui::TableNextRow();

            if (imgui::TableSetColumnIndex(file_ops_table_col_op_type)) {
                char const *icon = get_icon(file_op.obj_type);
                ImVec4 icon_color = get_color(file_op.obj_type);

                char const *desc = nullptr;
                if      (file_op.op_type == file_operation_type::move) desc = "Mov";
                else if (file_op.op_type == file_operation_type::copy) desc = "Cpy";
                else if (file_op.op_type == file_operation_type::del ) desc = "Del";

                imgui::TextUnformatted(desc);
                imgui::SameLine();
                imgui::TextColored(icon_color, icon);
            }

            if (imgui::TableSetColumnIndex(file_ops_table_col_completion_time)) {
                auto when_completed_str = time_diff_str(file_op.completion_time, get_time_system());

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

                    auto when_undone_str = time_diff_str(file_op.undo_time, get_time_system());
                    char const *verb = ICON_CI_DEBUG_STEP_BACK;
                    imgui::Text("%s %s", verb, when_undone_str.data());
                }
            }

            if (imgui::TableSetColumnIndex(file_ops_table_col_src_path)) {
                char const *src_path = settings.file_operations_src_path_full ? file_op.src_path.data() : path_find_filename(file_op.src_path.data());

                if (global_state::settings().win32_file_icons) {
                    if (file_op.src_icon_GLtexID == 0) {
                        std::tie(file_op.src_icon_GLtexID, file_op.src_icon_size) = load_icon_texture(file_op.src_path.data(), 0, "completed_file_operation");
                        if (file_op.src_icon_GLtexID > 0) {
                            s_last_known_icon_size = file_op.src_icon_size;
                        }
                    }
                    auto const &icon_size = file_op.src_icon_GLtexID < 1 ? s_last_known_icon_size : file_op.src_icon_size;

                    if (file_op.op_type == file_operation_type::move) {
                        imgui::Image((ImTextureID)(file_op.src_icon_GLtexID > 0 ? file_op.src_icon_GLtexID : file_op.dst_icon_GLtexID), icon_size, ImVec2(0,0), ImVec2(1,1), ImVec4(1,1,1,.3f));
                    } else if (file_op.op_type == file_operation_type::del) {
                        imgui::Image((ImTextureID)(file_op.src_icon_GLtexID > 0 ? file_op.src_icon_GLtexID : file_op.dst_icon_GLtexID), icon_size, ImVec2(0,0), ImVec2(1,1), ImVec4(1,1,1,.5f));
                        auto icon_rect = imgui::GetItemRect();
                        imgui::GetWindowDrawList()->AddLine(icon_rect.GetTL(), icon_rect.GetBR(), imgui::ImVec4_to_ImU32(error_color(), true), 1);
                    } else {
                        // file_operation_type::copy
                        imgui::Image((ImTextureID)(file_op.src_icon_GLtexID > 0 ? file_op.src_icon_GLtexID : file_op.dst_icon_GLtexID), icon_size);
                    }
                }
                else { // fallback to generic icons
                    char const *icon = get_icon(file_op.obj_type);
                    ImVec4 icon_color = get_color(file_op.obj_type);
                    imgui::TextColored(icon_color, icon);
                }
                imgui::SameLine();

                auto label = make_str_static<1200>("%s##%zu", src_path, i);

                if (imgui::Selectable(label.data(), file_op.selected, ImGuiSelectableFlags_SpanAllColumns)) {
                    bool selection_state_before_activate = file_op.selected;

                    u64 num_deselected = 0;
                    if (!io.KeyCtrl && !io.KeyShift) {
                        // entry was selected but Ctrl was not held, so deselect everything
                        num_deselected = deselect_all(*completed_file_operations.container);
                    }

                    if (num_deselected > 1) {
                        file_op.selected = true;
                    } else {
                        file_op.selected = !selection_state_before_activate;
                    }

                    if (io.KeyShift) {
                        auto [first_idx, last_idx] = imgui::SelectRange(s_latest_selected_row_idx, i);
                        s_latest_selected_row_idx = last_idx;
                        for (u64 j = first_idx; j <= last_idx; ++j) {
                            completed_file_operations.container->operator[](j).selected = true;
                        }
                    } else {
                        s_latest_selected_row_idx = i;
                    }
                }

                if (imgui::IsItemClicked(ImGuiMouseButton_Right)) {
                    s_context_menu_initiated_on_group_col = imgui::TableGetHoveredColumn() == file_ops_table_col_group;
                    s_context_menu_target_rect = imgui::GetItemRect();
                    s_context_menu_target_iter = elem_iter;

                    imgui::OpenPopup("## completed_file_operations context_menu");

                    bool keep_any_selected_state = file_op.selected;

                    for (auto &cfo : *completed_file_operations.container) {
                        cfo.selected = cfo.selected && keep_any_selected_state;

                        bool restorable = cfo.op_type == file_operation_type::del && !cfo.undone() && !path_is_empty(cfo.dst_path);
                        s_num_selected_when_context_menu_opened += u64(cfo.selected);
                        s_num_restorables_selected_when_context_menu_opened += u64(restorable && cfo.selected);
                        s_num_restorables_in_group_when_context_menu_opened += u64(restorable && cfo.group_id == elem_iter->group_id);
                    }
                }
            }
            if (imgui::TableGetHoveredColumn() == file_ops_table_col_src_path && imgui::IsItemHovered() && io.KeyShift) {
                if (imgui::BeginTooltip()) {
                    render_path_with_stylish_separators(file_op.src_path.data(), appropriate_icon(file_op.src_icon_GLtexID, file_op.obj_type));
                    imgui::EndTooltip();
                }
            }

            if (imgui::TableSetColumnIndex(file_ops_table_col_dst_path)) {
                if (global_state::settings().win32_file_icons) {
                    bool is_restored = file_op.undone() && file_op.op_type == file_operation_type::del;
                    if (is_restored) {
                        if (file_op.dst_icon_GLtexID > 0) {
                            delete_icon_texture(file_op.dst_icon_GLtexID);
                            file_op.dst_icon_GLtexID = -1;
                        }
                    }
                    if (file_op.dst_icon_GLtexID == 0 && !is_restored) {
                        // TODO investigate weirdness where restored record has a valid dst_path and returns valid icon (I expect -1).
                        // For now we detect manually and force it to -1
                        std::tie(file_op.dst_icon_GLtexID, file_op.dst_icon_size) = load_icon_texture(file_op.dst_path.data(), 0, "completed_file_operation");
                        if (file_op.dst_icon_GLtexID > 0) {
                            s_last_known_icon_size = file_op.dst_icon_size;
                        }
                    }
                    if (!path_is_empty(file_op.dst_path)) {
                        auto const &icon_size = file_op.dst_icon_GLtexID < 1 ? s_last_known_icon_size : file_op.dst_icon_size;
                        ImGui::Image((ImTextureID)std::max(file_op.dst_icon_GLtexID, s64(0)), icon_size);
                        imgui::SameLine();
                    }
                }
                else { // fallback to generic icons
                    char const *icon = get_icon(file_op.obj_type);
                    ImVec4 icon_color = get_color(file_op.obj_type);
                    imgui::TextColored(icon_color, icon);
                    imgui::SameLine();
                }

                char const *dst_path = settings.file_operations_dst_path_full ? file_op.dst_path.data() : path_find_filename(file_op.dst_path.data());
                imgui::TextUnformatted(dst_path);
            }
            {
                ImRect cell_rect = imgui::TableGetCellBgRect(imgui::GetCurrentTable(), file_ops_table_col_dst_path);
                if (imgui::IsMouseHoveringRect(cell_rect) && io.KeyShift) {
                    if (imgui::BeginTooltip() && !path_is_empty(file_op.dst_path)) {
                        render_path_with_stylish_separators(file_op.dst_path.data(), appropriate_icon(file_op.dst_icon_GLtexID, file_op.obj_type));
                        imgui::EndTooltip();
                    }
                }
            }

            if (imgui::TableSetColumnIndex(file_ops_table_col_group)) {
                bool new_group_block = group_block != file_op.group_id;
                group_block = file_op.group_id;

                bool context_menu_open_on_this_row = (s_context_menu_target_iter.has_value() && &*s_context_menu_target_iter.value() == &file_op);

                if (new_group_block && !context_menu_open_on_this_row) {
                    ImVec2 cell_rect_min = imgui::GetCursorScreenPos() - imgui::GetStyle().CellPadding;
                    f32 cell_width = imgui::GetColumnWidth() + (2 * imgui::GetStyle().CellPadding.x);
                    imgui::TableDrawCellBorderTop(cell_rect_min, cell_width);
                }

                if (file_op.group_id != 0) {
                    imgui::Text("%zu", file_op.group_id);
                }
            }
        }

        if (imgui::IsPopupOpen("## completed_file_operations context_menu")) {
            if (s_num_selected_when_context_menu_opened <= 1) {
                assert(s_context_menu_target_rect.has_value());
                ImVec2 min = s_context_menu_target_rect.value().Min;
                min.x += 1; // to avoid overlap with table left V border
                ImVec2 const &max = s_context_menu_target_rect.value().Max;
                ImGui::GetWindowDrawList()->AddRect(min, max, imgui::ImVec4_to_ImU32(imgui::GetStyleColorVec4(ImGuiCol_NavHighlight), true));
            }
        }

        bool execute_forget_immediately = false;
        bool execute_forget_group_immediately = false;

        static bool s_forget_just_one = false;

        if (imgui::BeginPopup("## completed_file_operations context_menu")) {
            assert(s_context_menu_target_iter.has_value());
            assert(s_context_menu_target_iter.value() != completed_file_operations.container->end());
            completed_file_operation &context_target = *s_context_menu_target_iter.value();

            if (s_context_menu_initiated_on_group_col && context_target.group_id != 0 && s_num_restorables_in_group_when_context_menu_opened > 0) {
                imgui::ScopedDisable d(true);
                if (imgui::Selectable("(G) Restore")) {
                }
            }
            else {
                bool context_target_can_be_undeleted = context_target.op_type == file_operation_type::del && !context_target.undone() && !path_is_empty(context_target.dst_path);
                bool show_undelete_option = context_target_can_be_undeleted && s_num_selected_when_context_menu_opened <= 1; // && s_num_restorables_selected_when_context_menu_opened <= 1;

                if (show_undelete_option && imgui::Selectable("Restore")) {
                    if (s_num_restorables_selected_when_context_menu_opened <= 1) {
                        if (context_target.obj_type == basic_dirent::kind::directory) {
                            print_debug_msg("Restore directory [%s]", context_target.src_path.data());

                            swan_path restore_dir_utf8 = path_create(context_target.src_path.data(), path_extract_location(context_target.src_path.data()).length());

                            auto res = enqueue_undelete_directory(context_target.dst_path.data(), restore_dir_utf8.data(), context_target.src_path.data());

                            if (!res.success) {
                                std::string action = make_str("Undelete directory [%s].", context_target.src_path.data());
                                swan_popup_modals::open_error(action.c_str(), res.error_or_utf8_path.c_str());
                            }
                        }
                        else {
                            print_debug_msg("Restore file [%s]", context_target.src_path.data());

                            auto res = undelete_file(context_target.dst_path.data());

                            if (res.success()) {
                                context_target.undo_time = get_time_system();
                                context_target.selected = false;
                                (void) global_state::completed_file_operations_save_to_disk(&completed_file_ops_lock);
                            }
                            else {
                                std::string action = make_str("Undelete file [%s].", context_target.src_path.data());
                                std::string failure;
                                auto winapi_err = get_last_winapi_error().formatted_message.c_str();

                                if      (!res.step0_convert_hardlink_path_to_utf16) failure = make_str("Failed to convert hardlink path [%s] from UTF-8 to UTF-16.", context_target.dst_path.data());
                                else if (!res.step1_metadata_file_opened          ) failure = make_str("Failed to open metadata file corresponding to [%s], %s", context_target.dst_path.data(), winapi_err);
                                else if (!res.step2_metadata_file_read            ) failure = make_str("Failed to read contents of metadata file corresponding to [%s]", context_target.dst_path.data());
                                else if (!res.step3_new_hardlink_created          ) failure = make_str("Failed to create hardlink [%s], %s The backup hardlink [%s] was probably deleted.", context_target.src_path.data(), winapi_err, context_target.dst_path.data());
                                else if (!res.step4_old_hardlink_deleted          ) failure = make_str("Failed to delete hardlink [%s], %s", context_target.dst_path.data(), winapi_err);
                                else if (!res.step5_metadata_file_deleted         ) failure = make_str("Failed to delete metadata file corresponding to [%s], %s", context_target.dst_path.data(), winapi_err);
                                else                                                assert(false && "Bad code path");

                                swan_popup_modals::open_error(action.c_str(), failure.c_str());

                                if (res.step3_new_hardlink_created) {
                                    // not a complete success but enough to consider the deletion undone, as the last 2 steps are merely cleanup of the recycle bin
                                    context_target.undo_time = get_time_system();
                                    (void) global_state::completed_file_operations_save_to_disk(&completed_file_ops_lock);
                                }
                            }
                        }
                        std::tie(context_target.src_icon_GLtexID, context_target.src_icon_size) = load_icon_texture(context_target.src_path.data(), 0, "completed_file_operation");
                        if (context_target.src_icon_GLtexID > 0) {
                            s_last_known_icon_size = context_target.src_icon_size;
                        }
                    }
                }
            }

            if (!s_context_menu_initiated_on_group_col || context_target.group_id == 0) {
                if (s_num_selected_when_context_menu_opened <= 1) {
                    if (context_target.op_type == file_operation_type::del && context_target.undone()) {
                        if (imgui::Selectable("Find")) {
                            (void) find_in_swan_explorer_0(context_target.src_path.data());
                        }
                    }
                    else if (!path_is_empty(context_target.dst_path)) {
                        if (imgui::Selectable("Find")) {
                            (void) find_in_swan_explorer_0(context_target.dst_path.data());
                        }
                    }
                }
            }

            if (s_context_menu_initiated_on_group_col && context_target.group_id != 0) {
                if (imgui::Selectable("(G) Forget")) {
                    execute_forget_group_immediately = imgui::OpenConfirmationModal(
                        swan_id_confirm_completed_file_operations_forget_group,
                        "Are you sure you want to forget the group? This action cannot be undone.",
                        &global_state::settings().confirm_completed_file_operations_forget_group
                    );
                }
            }
            else {
                if (imgui::Selectable("Forget")) {
                    s_forget_just_one = s_num_selected_when_context_menu_opened <= 1;

                    execute_forget_immediately = imgui::OpenConfirmationModal(
                        swan_id_confirm_completed_file_operations_forget,
                        make_str("Are you sure you want to forget %zu rows? This action cannot be undone.",
                                s_num_selected_when_context_menu_opened).c_str(),
                        &global_state::settings().confirm_completed_file_operations_forget
                    );
                }
            }

            auto copy_menu_label = make_str_static<64>("%s ## completed_file_operations context_menu",
                                                       (s_context_menu_initiated_on_group_col && context_target.group_id != 0) ? "(G) Copy" : "Copy");

            if (imgui::BeginMenu(copy_menu_label.data())) {
                u32 for_group_id = s_context_menu_initiated_on_group_col ? context_target.group_id : 0;

                auto compute_clipboard = [&](std::function<std::string_view (completed_file_operation const &)> extract) noexcept
                {
                    std::string clipboard = {};

                    for (u64 i = 0; i < completed_file_operations.container->size(); ++i) {
                        auto const &cfo = completed_file_operations.container->operator[](i);
                        bool matched = for_group_id == 0 ? cfo.selected : cfo.group_id == for_group_id;
                        if (matched) {
                            std::string_view copy_content = extract(cfo);
                            clipboard.append(copy_content);
                            clipboard += '\n';
                        }
                    }

                    if (clipboard.ends_with('\n')) clipboard.pop_back();

                    return clipboard;
                };

                if (imgui::Selectable("Source name")) {
                    std::string clipboard = compute_clipboard([](completed_file_operation const &cfo) noexcept {
                        char const *file_name = path_cfind_filename(cfo.src_path.data());
                        return std::string_view(file_name);
                    });
                    imgui::SetClipboardText(clipboard.c_str());
                }
                if (imgui::Selectable("Source location")) {
                    std::string clipboard = compute_clipboard([](completed_file_operation const &cfo) noexcept {
                        std::string_view location = path_extract_location(cfo.src_path.data());
                        return location;
                    });
                    imgui::SetClipboardText(clipboard.c_str());
                }
                if (imgui::Selectable("Source full path")) {
                    std::string clipboard = compute_clipboard([](completed_file_operation const &cfo) noexcept {
                        return std::string_view(cfo.src_path.data());
                    });
                    imgui::SetClipboardText(clipboard.c_str());
                }

                if (!path_is_empty(context_target.dst_path)) {
                    imgui::Separator();

                    if (imgui::Selectable("Destination name")) {
                        std::string clipboard = compute_clipboard([](completed_file_operation const &cfo) noexcept {
                            char const *file_name = path_cfind_filename(cfo.dst_path.data());
                            return std::string_view(file_name);
                        });
                        imgui::SetClipboardText(clipboard.c_str());
                    }
                    if (imgui::Selectable("Destination location")) {
                        std::string clipboard = compute_clipboard([](completed_file_operation const &cfo) noexcept {
                            std::string_view location = path_extract_location(cfo.dst_path.data());
                            return location;
                        });
                        imgui::SetClipboardText(clipboard.c_str());
                    }
                    if (imgui::Selectable("Destination full path")) {
                        std::string clipboard = compute_clipboard([](completed_file_operation const &cfo) noexcept {
                            return std::string_view(cfo.dst_path.data());
                        });
                        imgui::SetClipboardText(clipboard.c_str());
                    }
                }

                imgui::EndMenu();
            }

            imgui::EndPopup();
        }
        else {
            // not rendering context popup
            s_num_selected_when_context_menu_opened = 0;
            s_num_restorables_selected_when_context_menu_opened = 0;
            s_num_restorables_in_group_when_context_menu_opened = 0;
            //! do not use these variables beyond this point
        }

        {
            auto status = imgui::GetConfirmationStatus(swan_id_confirm_completed_file_operations_forget);

            if (execute_forget_immediately || status.value_or(false)) {
                if (s_forget_just_one) {
                    auto erase_iter = s_context_menu_target_iter.value();
                    erase(completed_file_operations, erase_iter, erase_iter + 1);
                }
                else {
                    auto begin_iter = completed_file_operations.container->begin();
                    auto end_iter   = completed_file_operations.container->end();

                    auto selected_partition_iter = std::stable_partition(std::execution::par_unseq, begin_iter, end_iter,
                        [](completed_file_operation const &elem) noexcept { return !elem.selected; });

                    for (auto iter = selected_partition_iter; iter != end_iter; ++iter) {
                        erase(completed_file_operations, iter, iter + 1);
                    }
                }
                (void) global_state::completed_file_operations_save_to_disk(&completed_file_ops_lock);
                (void) global_state::settings().save_to_disk(); // persist potential change to confirmation checkbox
            }
        }

        {
            auto status = imgui::GetConfirmationStatus(swan_id_confirm_completed_file_operations_forget_group);

            if (execute_forget_group_immediately || status.value_or(false)) {
                u32 group_id = s_context_menu_target_iter.value()->group_id;

                auto predicate_same_group = [group_id](completed_file_operation const &cfo) noexcept { return cfo.group_id == group_id; };

                auto begin_iter = completed_file_operations.container->begin();
                auto end_iter   = completed_file_operations.container->end();

                auto remove_group_begin_iter = std::find_if    (begin_iter,              end_iter, predicate_same_group);
                auto remove_group_end_iter   = std::find_if_not(remove_group_begin_iter, end_iter, predicate_same_group);

                erase(completed_file_operations, remove_group_begin_iter, remove_group_end_iter);

                (void) global_state::completed_file_operations_save_to_disk(&completed_file_ops_lock);
                (void) global_state::settings().save_to_disk(); // persist potential change to confirmation checkbox
            }
        }

        imgui::EndTable();
    }

    if (settings_change) {
        global_state::settings().save_to_disk();
    }

    return true;
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
            char data_utf8[2048]; cstr_clear(data_utf8);
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
    std::string *init_error,
    char dir_sep_utf8,
    s32 num_max_file_operations) noexcept
{
    assert(!destination_directory_utf16.empty());

    std::replace(destination_directory_utf16.begin(), destination_directory_utf16.end(), L'/', L'\\');

    auto set_init_error_and_notify = [&](std::string const &err) noexcept {
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
        swan_path destination_utf8 = path_create("");

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
    prog_sink.dir_sep_utf8 = dir_sep_utf8;
    prog_sink.num_max_file_operations = num_max_file_operations;

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

            swan_path item_path_utf8 = path_create("");

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

        prog_sink.group_id = global_state::completed_file_operations_calc_next_group_id();
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
