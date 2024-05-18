#include "stdafx.hpp"
#include "common_functions.hpp"
#include "imgui_dependent_functions.hpp"
#include "path.hpp"
#include "scoped_timer.hpp"
#include "util.hpp"

static IShellLinkW *g_shell_link = nullptr;
static IPersistFile *g_persist_file_interface = nullptr;
static std::array<explorer_window, global_constants::num_explorers> g_explorers = {};
static file_operation_command_buf g_file_op_payload = {};

std::array<explorer_window, global_constants::num_explorers> &global_state::explorers() noexcept { return g_explorers; }

void init_COM_for_explorers(GLFWwindow *window, char const *ini_file_path) noexcept
{
    bool retry = true; // start true to do initial load
    char const *what_failed = nullptr;

    while (true) {
        if (retry) {
            retry = false;
            what_failed = nullptr;

            HRESULT result = CoInitialize(nullptr);

            if (FAILED(result)) {
                CoUninitialize();
                what_failed = "CoInitialize";
            }
            else {
                result = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkW, (LPVOID *)&g_shell_link);

                if (FAILED(result)) {
                    what_failed = "CoCreateInstance(CLSID_ShellLink, ..., CLSCTX_INPROC_SERVER, IID_IShellLinkW, ...)";
                }
                else {
                    result = g_shell_link->QueryInterface(IID_IPersistFile, (LPVOID *)&g_persist_file_interface);

                    if (FAILED(result)) {
                        g_persist_file_interface->Release();
                        CoUninitialize();
                        what_failed = "IUnknown::QueryInterface(IID_IPersistFile, ...)";
                    }
                }
            }
        }

        if (!what_failed) {
            break;
        }

        new_frame(ini_file_path);

        if (imgui::Begin("Startup Error", nullptr, ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_AlwaysAutoResize)) {
            imgui::TextColored(error_color(), "Application is unable to continue, critical initialization failed:");
            imgui::TextUnformatted(what_failed);
            retry = imgui::Button("Retry");
        }
        imgui::End();

        render_frame(window);
    }
}

void clean_COM_for_explorers() noexcept
try {
    g_persist_file_interface->Release();
    g_shell_link->Release();
    CoUninitialize();
}
catch (...) {}

struct cwd_count_info
{
    u64 selected_directories;
    u64 selected_symlinks;
    u64 selected_files;
    u64 filtered_directories;
    u64 filtered_symlinks;
    u64 filtered_files;
    u64 child_dirents;
    u64 child_directories;
    u64 child_symlinks;
    u64 child_files;
    u64 filtered_dirents;
    u64 selected_dirents;
};

static
std::optional<ImRect> render_table_rows_for_cwd_entries(
    explorer_window &expl,
    cwd_count_info const &cnt,
    u64 size_unit_multiplier,
    bool any_popups_open,
    char dir_sep_utf8,
    wchar_t dir_sep_utf16) noexcept;

struct render_dirent_context_menu_result
{
    bool open_bulk_rename_popup;
    bool open_single_rename_popup;
    explorer_window::dirent *single_dirent_to_be_renamed;
    std::optional<ImRect> context_menu_rect;
};
static render_dirent_context_menu_result
render_dirent_context_menu(explorer_window &expl, cwd_count_info const &cnt, swan_settings const &settings) noexcept;

static
void accept_move_dirents_drag_drop(explorer_window &expl) noexcept;

static
void render_footer(explorer_window &expl, cwd_count_info const &cnt, ImGuiStyle &style, bool cwd_exists_after_edit) noexcept;

u64 explorer_window::deselect_all_cwd_entries() noexcept
{
    u64 num_deselected = 0;

    for (auto &dirent : this->cwd_entries) {
        bool prev = dirent.is_selected;
        bool &curr = dirent.is_selected;
        curr = false;
        num_deselected += curr != prev;
    }

    return num_deselected;
}

void explorer_window::select_all_visible_cwd_entries(bool select_dotdot_dir) noexcept
{
    for (auto &dirent : this->cwd_entries) {
        if ( (!select_dotdot_dir && dirent.basic.is_path_dotdot()) || dirent.is_filtered_out ) {
            continue;
        } else {
            dirent.is_selected = true;
        }
    }
}

void explorer_window::invert_selection_on_visible_cwd_entries() noexcept
{
    for (auto &dirent : this->cwd_entries) {
        if (dirent.is_filtered_out) {
            dirent.is_selected = false;
        } else if (!dirent.basic.is_dotdot_dir()) {
            dirent.is_selected = !dirent.is_selected;
        }
    }
}

void explorer_window::set_latest_valid_cwd(swan_path const &new_latest_valid_cwd) noexcept
{
    if (global_state::settings().explorer_clear_filter_on_cwd_change) {
        this->reset_filter();
        this->show_filter_window = false;
    }
    this->latest_valid_cwd = new_latest_valid_cwd;
    path_force_separator(this->latest_valid_cwd, global_state::settings().dir_separator_utf8);
    while (path_pop_back_if(this->latest_valid_cwd, "\\/ "));
}

void explorer_window::uncut() noexcept
{
    for (auto &dirent : this->cwd_entries) {
        dirent.is_cut = false;
    }
}

void explorer_window::reset_filter() noexcept
{
    init_empty_cstr(this->filter_text.data());

    this->filter_show_directories = true;
    this->filter_show_files = true;
    this->filter_show_invalid_symlinks = true;
    this->filter_show_symlink_directories = true;
    this->filter_show_symlink_files = true;

    this->filter_polarity = true;
    this->filter_case_sensitive = false;
    this->filter_mode = explorer_window::filter_mode::contains;
}

static
generic_result add_selected_entries_to_file_op_payload(explorer_window &expl, char const *operation_desc, file_operation_type operation_type) noexcept
{
    std::stringstream err = {};

    for (auto &dirent : expl.cwd_entries) {
        if (!dirent.is_selected || dirent.basic.is_path_dotdot()) {
            continue;
        }

        if (operation_type == file_operation_type::move) {
            if (dirent.is_cut) {
                continue; // prevent same dirent from being cut multiple times
                          // (although multiple copy commands of the same dirent are permitted, intentionally)
            } else {
                dirent.is_cut = true;
            }
        }

        if (operation_type == file_operation_type::copy && dirent.is_cut) {
            // this situation wouldn't make sense, because you can't CopyItem after MoveItem since there's nothing left to copy
            // TODO: maybe indicate something to the user rather than ignoring their request?
            continue;
        }

        swan_path src = expl.cwd;

        if (path_append(src, dirent.basic.path.data(), global_state::settings().dir_separator_utf8, true)) {
            g_file_op_payload.items.push_back({ operation_desc, operation_type, dirent.basic.type, src });
        } else {
            err << "Current working directory path + [" << src.data() << "] exceeds max allowed path length.\n";
        }
    }

    std::string errors = err.str();

    return { errors.empty(), errors };
}

static
generic_result delete_selected_entries(explorer_window &expl) noexcept
{
    auto file_operation_task = [](
        std::wstring working_directory_utf16,
        std::wstring paths_to_delete_utf16,
        std::mutex *init_done_mutex,
        std::condition_variable *init_done_cond,
        bool *init_done,
        std::string *init_error
    ) noexcept {
        assert(!working_directory_utf16.empty());
        if (!StrChrW(L"\\/", working_directory_utf16.back())) {
            working_directory_utf16 += L'\\';
        }

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

        result = file_op->SetOperationFlags(/*FOF_NOCONFIRMATION | */FOF_ALLOWUNDO);
        if (FAILED(result)) {
            return set_init_error_and_notify(make_str("IFileOperation::SetOperationFlags, %s", _com_error(result).ErrorMessage()));
        }

        explorer_file_op_progress_sink prog_sink;
        prog_sink.contains_delete_operations = true;
        prog_sink.dst_expl_id = -1;
        prog_sink.dst_expl_cwd_when_operation_started = path_create("");

        // add items (IShellItem) for exec to IFileOperation
        {
            auto items_to_delete = std::wstring_view(paths_to_delete_utf16.data()) | std::ranges::views::split('\n');
            std::stringstream err = {};
            std::wstring full_path_to_delete_utf16 = {};

            full_path_to_delete_utf16.reserve((global_state::page_size() / 2) - 1);

            u64 i = 0;
            for (auto item_utf16 : items_to_delete) {
                SCOPE_EXIT { ++i; };

                full_path_to_delete_utf16.clear();
                full_path_to_delete_utf16.append(working_directory_utf16);
                std::wstring_view view(item_utf16.begin(), item_utf16.end());
                full_path_to_delete_utf16 += view;

                // shlwapi doesn't like '/', force them all to '\'
                std::replace(full_path_to_delete_utf16.begin(), full_path_to_delete_utf16.end(), L'/', L'\\');

                swan_path item_path_utf8 = path_create("");

                IShellItem *to_delete = nullptr;
                result = SHCreateItemFromParsingName(full_path_to_delete_utf16.c_str(), nullptr, IID_PPV_ARGS(&to_delete));
                if (FAILED(result)) {
                    HANDLE accessible = CreateFileW(
                        full_path_to_delete_utf16.c_str(),
                        FILE_LIST_DIRECTORY,
                        FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
                        NULL,
                        OPEN_EXISTING,
                        FILE_FLAG_BACKUP_SEMANTICS,
                        NULL);

                    SCOPE_EXIT { CloseHandle(accessible); };

                    WCOUT_IF_DEBUG("FAILED: SHCreateItemFromParsingName [" << full_path_to_delete_utf16.c_str() << "]\n");

                    if (!utf16_to_utf8(full_path_to_delete_utf16.data(), item_path_utf8.data(), item_path_utf8.size())) {
                        err << "SHCreateItemFromParsingName and conversion of delete path from UTF-16 to UTF-8.\n";
                    } else {
                        if (accessible == INVALID_HANDLE_VALUE) {
                            err << "File or directory is not accessible, maybe it is locked or has been moved/deleted? ";
                        }
                        err << "FAILED SHCreateItemFromParsingName for [" << item_path_utf8.data() << "]\n";
                    }
                    continue;
                }

                SCOPE_EXIT { to_delete->Release(); };

                result = file_op->DeleteItem(to_delete, nullptr);
                if (FAILED(result)) {
                    WCOUT_IF_DEBUG("FAILED: IFileOperation::DeleteItem [" << full_path_to_delete_utf16.c_str() << "]\n");

                    if (!utf16_to_utf8(full_path_to_delete_utf16.data(), item_path_utf8.data(), item_path_utf8.size())) {
                        err << "IFileOperation::DeleteItem and conversion of delete path from UTF-16 to UTF-8.\n";
                    } else {
                        err << "IFileOperation::DeleteItem [" << item_path_utf8.data() << "]";
                    }
                } else {
                    WCOUT_IF_DEBUG("file_op->DeleteItem [" << full_path_to_delete_utf16.c_str() << "]\n");
                }
            }

            std::string errors = err.str();
            if (!errors.empty()) {
                errors.pop_back(); // remove trailing '\n'
                return set_init_error_and_notify(errors);
            }

            bool compound_operation = i > 1;
            prog_sink.group_id = compound_operation ? global_state::completed_file_operations_calc_next_group_id() : 0;
        }

        DWORD cookie = {};

        result = file_op->Advise(&prog_sink, &cookie);
        if (FAILED(result)) {
            return set_init_error_and_notify(make_str("IFileOperation::Advise, %s", _com_error(result).ErrorMessage()));
        }
        print_debug_msg("IFileOperation::Advise(%d)", cookie);

        set_init_error_and_notify(""); // init succeeded, no error

        result = file_op->PerformOperations();
        if (FAILED(result)) {
            print_debug_msg("FAILED IFileOperation::PerformOperations, %s", _com_error(result).ErrorMessage());
        }

        file_op->Unadvise(cookie);
        if (FAILED(result)) {
            print_debug_msg("FAILED IFileOperation::Unadvise(%d), %s", cookie, _com_error(result).ErrorMessage());
        }
    };

    wchar_t cwd_utf16[MAX_PATH]; init_empty_cstr(cwd_utf16);

    if (!utf8_to_utf16(expl.cwd.data(), cwd_utf16, lengthof(cwd_utf16))) {
        return { false, "Conversion of current working directory path from UTF-8 to UTF-16." };
    }

    std::wstring packed_paths_to_delete_utf16 = {};
    u64 item_count = 0;
    {
        wchar_t item_utf16[MAX_PATH];
        std::stringstream err = {};

        for (auto const &item : expl.cwd_entries) {
            if (!item.is_filtered_out && item.is_selected) {
                init_empty_cstr(item_utf16);

                if (!utf8_to_utf16(item.basic.path.data(), item_utf16, lengthof(item_utf16))) {
                    err << "Conversion of [" << item.basic.path.data() << "] from UTF-8 to UTF-16.\n";
                }

                packed_paths_to_delete_utf16.append(item_utf16).append(L"\n");
                ++item_count;
            }
        }

        // WCOUT_IF_DEBUG("packed_paths_to_delete_utf16:\n" << packed_paths_to_delete_utf16 << '\n');

        if (!packed_paths_to_delete_utf16.empty()) {
            packed_paths_to_delete_utf16.pop_back(); // remove trailing \n
        }

        std::string errors = err.str();
        if (!errors.empty()) {
            return { false, errors };
        }
    }

    bool initialization_done = false;
    std::string initialization_error = {};

    global_state::thread_pool().push_task(file_operation_task,
        cwd_utf16,
        std::move(packed_paths_to_delete_utf16),
        &expl.shlwapi_task_initialization_mutex,
        &expl.shlwapi_task_initialization_cond,
        &initialization_done,
        &initialization_error);

    {
        std::unique_lock lock(expl.shlwapi_task_initialization_mutex);
        expl.shlwapi_task_initialization_cond.wait(lock, [&]() noexcept { return initialization_done; });
    }

    return { initialization_error.empty(), initialization_error };
}

generic_result move_files_into(swan_path const &destination_utf8, explorer_window &expl, move_dirents_drag_drop_payload &payload) noexcept
{
    /*
        ? Moving files via shlwapi is a blocking operation.
        ? Thus we must call IFileOperation::PerformOperations outside of main UI thread so user can continue to interact with UI during file operation.
        ? There is a constraint however: the IFileOperation object must be initialized on same thread which will call PerformOperations.
        ? This function blocks until IFileOperation initialization is complete so that we can report any initialization errors to user.
        ? After worker thread signals that initialization is complete, we proceed with execution and let PerformOperations happen asynchronously.
    */

    SCOPE_EXIT { delete[] payload.absolute_paths_delimited_by_newlines; };

    wchar_t destination_utf16[MAX_PATH]; init_empty_cstr(destination_utf16);

    if (!utf8_to_utf16(destination_utf8.data(), destination_utf16, lengthof(destination_utf16))) {
        return { false, "Conversion of destination directory path from UTF-8 to UTF-16." };
    }

    bool initialization_done = false;
    std::string initialization_error = {};

    global_state::thread_pool().push_task(perform_file_operations,
        expl.id,
        destination_utf16,
        std::wstring(payload.absolute_paths_delimited_by_newlines),
        std::vector<file_operation_type>(payload.num_items, file_operation_type::move),
        &expl.shlwapi_task_initialization_mutex,
        &expl.shlwapi_task_initialization_cond,
        &initialization_done,
        &initialization_error);

    {
        std::unique_lock lock(expl.shlwapi_task_initialization_mutex);
        expl.shlwapi_task_initialization_cond.wait(lock, [&]() noexcept { return initialization_done; });
    }

    return { initialization_error.empty(), initialization_error };
}

static
generic_result handle_drag_drop_onto_dirent(
    explorer_window &expl,
    explorer_window::dirent const &target_dirent,
    ImGuiPayload const *payload_wrapper,
    char dir_sep_utf8) noexcept
{
    auto payload_data = (move_dirents_drag_drop_payload *)payload_wrapper->Data;
    assert(payload_data != nullptr);
    swan_path destination_utf8 = expl.cwd;

    if (target_dirent.basic.is_dotdot_dir()) {
        // we cannot simply append ".." to `destination_utf8` and give that to `move_files_into`,
        // because shlwapi does not accept a path like "C:/some/path/../"
        while (path_pop_back_if_not(destination_utf8, dir_sep_utf8));

        return move_files_into(destination_utf8, expl, *payload_data);
    }
    else {
        if (!path_append(destination_utf8, target_dirent.basic.path.data(), dir_sep_utf8, true)) {
            return { false, make_str("Append current working directory to drop target [%s]", target_dirent.basic.path.data()) };
        } else {
            return move_files_into(destination_utf8, expl, *payload_data);
        }
    }
}

generic_result symlink_data::extract(char const *lnk_file_path_utf8, char const *cwd) noexcept
{
    assert(lnk_file_path_utf8 != nullptr);

    swan_path lnk_file_full_path_utf8;
    wchar_t lnk_file_path_utf16[MAX_PATH]; init_empty_cstr(lnk_file_path_utf16);
    HRESULT com_handle = {};
    LPITEMIDLIST item_id_list = nullptr;

    if (cwd) {
        lnk_file_full_path_utf8 = path_create(cwd);

        if (!path_append(lnk_file_full_path_utf8, lnk_file_path_utf8, global_state::settings().dir_separator_utf8, true)) {
            return { false, "Max path length exceeded when appending symlink name to current working directory path." };
        }
    } else {
        lnk_file_full_path_utf8 = path_create(lnk_file_path_utf8);
    }

    if (!utf8_to_utf16(lnk_file_full_path_utf8.data(), lnk_file_path_utf16, lengthof(lnk_file_path_utf16))) {
        return { false, "Conversion of symlink path from UTF-8 to UTF-16." };
    }

    com_handle = g_persist_file_interface->Load(lnk_file_path_utf16, STGM_READ);

    if (com_handle != S_OK) {
        return { false, "IPersistFile::Load(..., STGM_READ)." };
    }

    com_handle = g_shell_link->GetIDList(&item_id_list);

    if (com_handle != S_OK) {
        auto err = get_last_winapi_error().formatted_message;
        return { false, err + " (IShellLinkW::GetIDList)." };
    }

    if (!SHGetPathFromIDListW(item_id_list, this->target_path_utf16)) {
        auto err = get_last_winapi_error().formatted_message;
        return { false, err + " (SHGetPathFromIDListW)." };
    }

    if (!utf16_to_utf8(this->target_path_utf16, this->target_path_utf8.data(), this->target_path_utf8.size())) {
        return { false, "Conversion of symlink target path from UTF-16 to UTF-8." };
    }

    com_handle = g_shell_link->GetWorkingDirectory(this->working_directory_path_utf16, MAX_PATH);

    if (com_handle != S_OK) {
        return { false, get_last_winapi_error().formatted_message + " (IShellLinkW::GetWorkingDirectory)." };
    }

    com_handle = g_shell_link->GetArguments(this->arguments_utf16, 1024);

    if (com_handle != S_OK) {
        return { false, get_last_winapi_error().formatted_message + " (IShellLinkW::GetArguments)." };
    }

    com_handle = g_shell_link->GetShowCmd(&this->show_cmd);

    if (com_handle != S_OK) {
        return { false, get_last_winapi_error().formatted_message + " (IShellLinkW::GetShowCmd)" };
    }

    return { true, "" }; // success
}

/// @brief Attempts to extract information from a .lnk file and do something with it depending on the target type.
/// @param dirent The .lnk file.
/// @param expl Contextual explorer window.
/// @return If symlink points to a file, attempt ShellExecuteW("open") and return the result.
/// If symlink points to a directory, return the path of the pointed to directory in `error_or_utf8_path`.
/// If data extraction fails, the reason is stated in `error_or_utf8_path`.
static
generic_result open_symlink(explorer_window::dirent const &dirent, explorer_window &expl) noexcept
{
    symlink_data lnk_data = {};
    auto extract_result = lnk_data.extract(dirent.basic.path.data(), expl.cwd.data());
    if (!extract_result.success) {
        return extract_result; // propogate failure to caller
    }

    if (directory_exists(lnk_data.target_path_utf8.data())) {
        // symlink to a directory, tell caller to navigate there
        return { true, lnk_data.target_path_utf8.data() };
    }
    else {
        // symlink to a file, let's open it
        HINSTANCE result = ShellExecuteW(nullptr, L"open",
                                         lnk_data.target_path_utf16,
                                         lnk_data.arguments_utf16,
                                         lnk_data.working_directory_path_utf16,
                                         lnk_data.show_cmd);

        intptr_t err_code = (intptr_t)result;

        if (err_code > HINSTANCE_ERROR) {
            print_debug_msg("[ %d ] ShellExecuteW success", expl.id);
            return { true, lnk_data.target_path_utf8.data() };
        }
        else if (err_code == SE_ERR_NOASSOC) {
            print_debug_msg("[ %d ] ShellExecuteW error: SE_ERR_NOASSOC", expl.id);
            return { false, "No association between file type and program (ShellExecuteW: SE_ERR_NOASSOC)." };
        }
        else if (err_code == SE_ERR_FNF) {
            print_debug_msg("[ %d ] ShellExecuteW error: SE_ERR_FNF", expl.id);
            return { false, "File not found (ShellExecuteW: SE_ERR_FNF)." };
        }
        else {
            print_debug_msg("[ %d ] ShellExecuteW error: unexpected error", expl.id);
            return { false, get_last_winapi_error().formatted_message };
        }
    }
}

/// @brief Sorts `expl.cwd_entries` in place according to `expl.sort_specs`.
/// Entries where `is_filtered_out` is true are forced to a contiguous block at the back of the vector.
/// The result is 2 distinct halves. the first half [expl.cwd_entries.begin(), retval) are entries which are unfiltered (to be shown),
/// sorted according to `expl.sort_specs`. The second half [retval, expl.cwd_entries.end()) are entries which are filtered (not to be shown).
/// @return Iterator to the first filtered entry if there is one, `cwd_entries.end()` otherwise.
static
std::vector<explorer_window::dirent>::iterator
sort_cwd_entries(explorer_window &expl, std::source_location sloc = std::source_location::current()) noexcept
{
    f64 sort_us = 0;
    SCOPE_EXIT { expl.sort_timing_samples.push_back(sort_us); };
    scoped_timer<timer_unit::MICROSECONDS> sort_timer(&sort_us);

    auto &cwd_entries = expl.cwd_entries;

    print_debug_msg("[ %d ] sort_cwd_entries() called from [%s:%d]", expl.id, cget_file_name(sloc.file_name()), sloc.line());

    using dir_ent_t = explorer_window::dirent;

    auto first_filtered_dirent = std::partition(cwd_entries.begin(), cwd_entries.end(), [](dir_ent_t const &dirent) noexcept {
        return !dirent.is_filtered_out;
    });

    std::sort(cwd_entries.begin(), first_filtered_dirent, [&](dir_ent_t const &left, dir_ent_t const &right) noexcept -> bool {
        s64 delta = 0;

        for (auto const &col_sort_spec : expl.column_sort_specs) {
            switch (col_sort_spec.ColumnUserID) {
                default:
                case explorer_window::cwd_entries_table_col_id: {
                    delta = left.basic.id - right.basic.id;
                    break;
                }
                case explorer_window::cwd_entries_table_col_path: {
                    delta = lstrcmpiA(right.basic.path.data(), left.basic.path.data());
                    break;
                }
                case explorer_window::cwd_entries_table_col_object: {
                    s32 precedence_table[(u64)basic_dirent::kind::count] = {
                        10, // directory
                        9,  // symlink_to_directory
                        8,  // file
                        7,  // symlink_to_file
                        6,  // symlink_ambiguous
                        5,  // invalid_symlink
                    };
                    assert((s32)right.basic.type >= 0);
                    delta = precedence_table[(u64)left.basic.type] - precedence_table[(u64)right.basic.type];
                    break;
                }
                case explorer_window::cwd_entries_table_col_type: {
                    delta = lstrcmpiA(cget_file_ext(right.basic.path.data()), cget_file_ext(left.basic.path.data()));
                    break;
                }
                case explorer_window::cwd_entries_table_col_size_pretty:
                case explorer_window::cwd_entries_table_col_size_bytes: {
                    delta = left.basic.size - right.basic.size;
                    break;
                }
                case explorer_window::cwd_entries_table_col_creation_time: {
                    delta = CompareFileTime(&left.basic.creation_time_raw, &right.basic.creation_time_raw);
                    break;
                }
                case explorer_window::cwd_entries_table_col_last_write_time: {
                    delta = CompareFileTime(&left.basic.last_write_time_raw, &right.basic.last_write_time_raw);
                    break;
                }
            }

            if (delta > 0) {
                return col_sort_spec.SortDirection == ImGuiSortDirection_Ascending;
            }
            else if (delta < 0) {
                return col_sort_spec.SortDirection != ImGuiSortDirection_Ascending;
            }
            else { // delta == 0
                continue; // go to next sort spec
            }
        }

        if (delta == 0) {
            return left.basic.id < right.basic.id;
        }
        else {
            return delta;
        }
    });

    return first_filtered_dirent;
}

explorer_window::update_cwd_entries_result explorer_window::update_cwd_entries(
    update_cwd_entries_actions actions,
    std::string_view parent_dir,
    std::source_location sloc) noexcept
{
    update_cwd_entries_result retval = {};

    print_debug_msg("[ %d ] expl.update_cwd_entries(%d) called from [%s:%d]", this->id, actions, cget_file_name(sloc.file_name()), sloc.line());

    this->scroll_to_nth_selected_entry_next_frame = u64(-1);

    update_cwd_entries_timers timers = {};
    SCOPE_EXIT { this->update_cwd_entries_timing_samples.push_back(timers); };

    char dir_sep_utf8 = global_state::settings().dir_separator_utf8;

    {
        scoped_timer<timer_unit::MICROSECONDS> function_timer(&timers.total_us);

        if (actions & query_filesystem) {
            static std::vector<swan_path> s_selected_entries = {};
            s_selected_entries.clear();

            for (auto const &dirent : this->cwd_entries) {
                if (dirent.is_selected) {
                    // this could throw on alloc failure, which will call std::terminate
                    s_selected_entries.push_back(dirent.basic.path);
                }
            }

            this->cwd_entries.clear();

            if (parent_dir != "") {
                bool inside_recycle_bin;

                wchar_t search_path_utf16[512]; init_empty_cstr(search_path_utf16);
                {
                    scoped_timer<timer_unit::MICROSECONDS> searchpath_setup_timer(&timers.searchpath_setup_us);

                    u64 num_trailing_spaces = 0;
                    while (*(&parent_dir.back() - num_trailing_spaces) == ' ') {
                        ++num_trailing_spaces;
                    }
                    swan_path parent_dir_trimmed = {};
                    strncpy(parent_dir_trimmed.data(), parent_dir.data(), parent_dir.size() - num_trailing_spaces);
                    path_force_separator(parent_dir_trimmed, '\\');

                    (void) utf8_to_utf16(parent_dir_trimmed.data(), search_path_utf16, lengthof(search_path_utf16));

                    wchar_t dir_sep_w[] = { (wchar_t)dir_sep_utf8, L'\0' };

                    if (!parent_dir.ends_with(dir_sep_utf8)) {
                        (void) StrCatW(search_path_utf16, dir_sep_w);
                    }
                    (void) StrCatW(search_path_utf16, L"*");

                    inside_recycle_bin = str_starts_with(parent_dir_trimmed.data() + 1, ":\\$Recycle.Bin\\"); // assume drive letter is first char
                }

                // just for debug log
                {
                    char utf8_buffer[2048]; init_empty_cstr(utf8_buffer);

                    if (!utf16_to_utf8(search_path_utf16, utf8_buffer, lengthof(utf8_buffer))) {
                        return retval;
                    }

                    print_debug_msg("[ %d ] querying filesystem, search_path = [%s]", this->id, utf8_buffer);
                }

                #if 1
                {
                    std::scoped_lock lock(select_cwd_entries_on_next_update_mutex);
                    scoped_timer<timer_unit::MICROSECONDS> sort_timer(&timers.entries_to_select_sort);
                    std::sort(select_cwd_entries_on_next_update.begin(), select_cwd_entries_on_next_update.end(), std::greater<swan_path>());
                }
                #endif

                scoped_timer<timer_unit::MICROSECONDS> filesystem_timer(&timers.filesystem_us);

                WIN32_FIND_DATAW find_data;
                HANDLE find_handle = FindFirstFileW(search_path_utf16, &find_data);
                SCOPE_EXIT { FindClose(find_handle); };

                if (find_handle == INVALID_HANDLE_VALUE) {
                    print_debug_msg("[ %d ] find_handle == INVALID_HANDLE_VALUE", this->id);
                    return retval;
                }
                retval.parent_dir_exists = true;

                u32 entry_id = 0;

                do {
                    explorer_window::dirent entry = {};
                    entry.basic.id = entry_id;
                    entry.basic.size = two_u32_to_one_u64(find_data.nFileSizeLow, find_data.nFileSizeHigh);
                    entry.basic.creation_time_raw = find_data.ftCreationTime;
                    entry.basic.last_write_time_raw = find_data.ftLastWriteTime;

                    if (!utf16_to_utf8(find_data.cFileName, entry.basic.path.data(), entry.basic.path.size())) {
                        continue;
                    }

                    if (path_equals_exactly(entry.basic.path, ".")) {
                        continue;
                    }

                    if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                        entry.basic.type = basic_dirent::kind::directory;
                    }
                    else if (!inside_recycle_bin && path_ends_with(entry.basic.path, ".lnk")) {
                        // TODO: this branch is quite slow, there should probably be an option to opt out of checking the type and validity of symlinks

                        entry.basic.type = basic_dirent::kind::invalid_symlink; // default value, if something fails below

                        static std::wstring full_path_utf16 = {};
                        full_path_utf16.clear();
                        full_path_utf16.append(search_path_utf16);
                        full_path_utf16.pop_back(); // remove '*'
                        full_path_utf16.append(find_data.cFileName);

                        // Load the shortcut
                        HRESULT com_handle = g_persist_file_interface->Load(full_path_utf16.c_str(), STGM_READ);
                        if (FAILED(com_handle)) {
                            WCOUT_IF_DEBUG("FAILED IPersistFile::Load [" << full_path_utf16.c_str() << "]\n");
                        }
                        else {
                            // Get the target path
                            wchar_t target_path_utf16[MAX_PATH];
                            com_handle = g_shell_link->GetPath(target_path_utf16, lengthof(target_path_utf16), NULL, SLGP_RAWPATH);
                            if (FAILED(com_handle)) {
                                WCOUT_IF_DEBUG("FAILED IShellLinkW::GetPath [" << full_path_utf16.c_str() << "]\n");
                            }
                            else {
                                if      (PathIsDirectoryW(target_path_utf16)) entry.basic.type = basic_dirent::kind::symlink_to_directory;
                                else if (PathFileExistsW(target_path_utf16))  entry.basic.type = basic_dirent::kind::symlink_to_file;
                                else                                          entry.basic.type = basic_dirent::kind::invalid_symlink;
                            }
                        }
                    }
                    else {
                        entry.basic.type = basic_dirent::kind::file;
                    }

                    if (entry.basic.is_path_dotdot()) {
                        if (global_state::settings().explorer_show_dotdot_dir) {
                            this->cwd_entries.emplace_back(entry);
                            std::swap(this->cwd_entries.back(), this->cwd_entries.front());
                        }
                    } else {
                        for (auto prev_selected_entry = s_selected_entries.begin(); prev_selected_entry != s_selected_entries.end(); ++prev_selected_entry) {
                            bool was_selected_before_refresh = path_equals_exactly(entry.basic.path, *prev_selected_entry);
                            if (was_selected_before_refresh) {
                                entry.is_selected = true;
                                retval.num_entries_selected += 1;
                                std::swap(*prev_selected_entry, s_selected_entries.back());
                                s_selected_entries.pop_back();
                                break;
                            }
                        }

                        {
                            std::scoped_lock lock(select_cwd_entries_on_next_update_mutex);

                            f64 search_us = 0;
                            scoped_timer<timer_unit::MICROSECONDS> search_timer(&search_us);

                            bool found = std::binary_search(select_cwd_entries_on_next_update.begin(), select_cwd_entries_on_next_update.end(), entry.basic.path);
                            if (found) {
                                auto lower_it = std::lower_bound(select_cwd_entries_on_next_update.begin(), select_cwd_entries_on_next_update.end(), entry.basic.path);
                                auto actual_found_thing = select_cwd_entries_on_next_update.begin() + std::distance(select_cwd_entries_on_next_update.begin(), lower_it);
                                std::swap(*actual_found_thing, select_cwd_entries_on_next_update.back());
                                select_cwd_entries_on_next_update.pop_back();
                                entry.is_selected = true;
                                retval.num_entries_selected += 1;
                            }

                            timers.entries_to_select_search += search_us;
                        }

                        // this could throw on alloc failure, which will call std::terminate
                        this->cwd_entries.emplace_back(entry);
                    }

                    ++this->num_file_finds;
                    ++entry_id;
                }
                while (FindNextFileW(find_handle, &find_data));

                this->refresh_message.clear();
                this->refresh_message_tooltip.clear();
                this->last_filesystem_query_time = current_time_precise();
                {
                    std::scoped_lock lock(select_cwd_entries_on_next_update_mutex);
                    this->select_cwd_entries_on_next_update.clear();
                }
            }
        }

        if (actions & filter) {
            scoped_timer<timer_unit::MICROSECONDS> filter_timer(&timers.filter_us);

            this->filter_error.clear();

            bool dirent_type_to_visibility_table[(u64)basic_dirent::kind::count] = {
                this->filter_show_directories, // directory
                this->filter_show_symlink_directories, // symlink_to_directory
                this->filter_show_files, // file
                this->filter_show_symlink_files, // symlink_to_file
                true, // symlink_ambiguous
                this->filter_show_invalid_symlinks // invalid_symlink
            };

            u64 filter_text_len = strlen(this->filter_text.data());

            for (auto &dirent : this->cwd_entries) {
                assert((s32)dirent.basic.type != -1);
                bool this_type_of_dirent_is_visible = dirent_type_to_visibility_table[(u64)dirent.basic.type];

                dirent.is_filtered_out = !this_type_of_dirent_is_visible;
                dirent.highlight_start_idx = 0;
                dirent.highlight_len = 0;

                if (this_type_of_dirent_is_visible && filter_text_len > 0) { // apply textual filter against dirent name
                    char const *dirent_name = dirent.basic.path.data();

                    switch (this->filter_mode) {
                        default:
                        case explorer_window::filter_mode::contains: {
                            auto matcher = this->filter_case_sensitive ? StrStrA : StrStrIA;

                            char const *match_start = matcher(dirent_name, this->filter_text.data());;
                            bool filtered_out = this->filter_polarity != (bool)match_start;
                            dirent.is_filtered_out = filtered_out;

                            if (!filtered_out && filter_polarity == true) {
                                // highlight just the substring
                                dirent.highlight_start_idx = std::distance(dirent_name, match_start);
                                dirent.highlight_len = filter_text_len;
                            }

                            break;
                        }

                        case explorer_window::filter_mode::regex_match: {
                            static std::regex s_filter_regex;
                            try {
                                scoped_timer<timer_unit::MICROSECONDS> regex_ctor_timer(&timers.regex_ctor_us);
                                s_filter_regex = this->filter_text.data();
                            }
                            catch (std::exception const &except) {
                                this->filter_error = except.what();
                                break;
                            }

                            auto match_flags = std::regex_constants::match_default | (std::regex_constants::icase * (this->filter_case_sensitive == 0));

                            bool filtered_out = this->filter_polarity != std::regex_match(dirent_name, s_filter_regex, (std::regex_constants::match_flag_type)match_flags);
                            dirent.is_filtered_out = filtered_out;

                            if (!filtered_out && filter_polarity == true) {
                                // highlight the whole path since we are using std::regex_match
                                dirent.highlight_start_idx = 0;
                                dirent.highlight_len = path_length(dirent.basic.path);
                            }

                            break;
                        }
                    }
                }
            }
        }
    }

    (void) sort_cwd_entries(*this);

    this->frame_count_when_cwd_entries_updated = imgui::GetFrameCount();

    return retval;
}

bool explorer_window::save_to_disk() const noexcept
{
    f64 save_to_disk_us = {};
    SCOPE_EXIT { this->save_to_disk_timing_samples.push_back(save_to_disk_us); };
    scoped_timer<timer_unit::MICROSECONDS> save_to_disk_timer(&save_to_disk_us);

    char file_name[32]; init_empty_cstr(file_name);
    [[maybe_unused]] s32 written = snprintf(file_name, lengthof(file_name), "data\\explorer_%d.txt", this->id);
    assert(written < lengthof(file_name));
    std::filesystem::path full_path = global_state::execution_path() / file_name;

    bool result = true;

    try {
        std::ofstream out(full_path);
        if (!out) {
            result = false;
        } else {
            out << "cwd " << path_length(cwd) << ' ' << cwd.data() << '\n';

            out << "filter " << strlen(filter_text.data()) << ' ' << filter_text.data() << '\n';

            out << "filter_mode "                       << (s32)filter_mode << '\n';
            out << "filter_case_sensitive "             << (s32)filter_case_sensitive << '\n';
            out << "filter_polarity "                   << (s32)filter_polarity << '\n';
            out << "filter_show_directories "           << (s32)filter_show_directories << '\n';
            out << "filter_show_symlink_directories "   << (s32)filter_show_symlink_directories << '\n';
            out << "filter_show_files "                 << (s32)filter_show_files << '\n';
            out << "filter_show_symlink_files "         << (s32)filter_show_symlink_files << '\n';
            out << "filter_show_invalid_symlinks "      << (s32)filter_show_invalid_symlinks << '\n';

            out << "wd_history_pos "                    << wd_history_pos << '\n';
            out << "wd_history.size() "                 << wd_history.size() << '\n';

            for (auto const &item : wd_history) {
                out << path_length(item.path) << ' ';
                out << item.path.data() << ' ';
                out << std::chrono::system_clock::to_time_t(item.time_departed) << ' ';
                out << item.called_from.size() << ' ' << item.called_from.c_str();
                out << '\n';
            }
        }
    }
    catch (...) {
        result = false;
    }

    print_debug_msg("[%s] save attempted, result: %d", file_name, result);
    this->latest_save_to_disk_result = (s8)result;

    return result;
}

bool explorer_window::load_from_disk(char dir_separator) noexcept
{
    assert(this->name != nullptr);

    char file_name[32]; init_empty_cstr(file_name);
    [[maybe_unused]] s32 written = snprintf(file_name, lengthof(file_name), "data\\explorer_%d.txt", this->id);
    assert(written < lengthof(file_name));
    std::filesystem::path full_path = global_state::execution_path() / file_name;

    try {
        std::ifstream in(full_path);
        if (!in) {
            print_debug_msg("FAILED to open file [%s]", file_name);
            return false;
        }

        std::string what = {};
        what.reserve(256);

        auto skip_one_whitespace_char = [](std::ifstream &in_fstream) {
            char whitespace = 0;
            in_fstream.read(&whitespace, 1);
            assert(whitespace == ' ');
        };

        auto read_bool = [&](char const *label, bool &value) noexcept {
            in >> what;
            assert(what == label);

            s32 read_val = 0;
            in >> read_val;

            value = (bool)read_val;
            print_debug_msg("[%s] %s = %d", file_name, label, value);
        };

        {
            in >> what;
            assert(what == "cwd");

            u64 cwd_len = 0;
            in >> cwd_len;
            print_debug_msg("[%s] cwd_len = %zu", file_name, cwd_len);

            skip_one_whitespace_char(in);

            in.read(cwd.data(), cwd_len);
            path_force_separator(cwd, dir_separator);
            print_debug_msg("[%s] cwd = [%s]", file_name, cwd.data());
        }

        {
            in >> what;
            assert(what == "filter");

            u64 filter_len = 0;
            in >> filter_len;
            print_debug_msg("[%s] filter_len = %zu", file_name, filter_len);

            skip_one_whitespace_char(in);

            in.read(filter_text.data(), filter_len);
            print_debug_msg("[%s] filter = [%s]", file_name, filter_text.data());
        }

        {
            in >> what;
            assert(what == "filter_mode");

            in >> (s32 &)filter_mode;
            print_debug_msg("[%s] filter_mode = %d", file_name, filter_mode);
        }

        read_bool("filter_case_sensitive", filter_case_sensitive);
        read_bool("filter_polarity", filter_polarity);
        read_bool("filter_show_directories", filter_show_directories);
        read_bool("filter_show_symlink_directories", filter_show_symlink_directories);
        read_bool("filter_show_files", filter_show_files);
        read_bool("filter_show_symlink_files", filter_show_symlink_files);
        read_bool("filter_show_invalid_symlinks", filter_show_invalid_symlinks);

        {
            in >> what;
            assert(what == "wd_history_pos");

            in >> wd_history_pos;
            print_debug_msg("[%s] wd_history_pos = %zu", file_name, wd_history_pos);
        }

        u64 wd_history_size = 0;
        {
            in >> what;
            assert(what == "wd_history.size()");

            in >> wd_history_size;
            print_debug_msg("[%s] wd_history_size = %zu", file_name, wd_history_size);
        }

        wd_history.resize(wd_history_size);
        for (u64 i = 0; i < wd_history_size; ++i) {
            u64 path_len = 0;
            in >> path_len;
            skip_one_whitespace_char(in);

            in.read(wd_history[i].path.data(), path_len);
            skip_one_whitespace_char(in);

            wd_history[i].time_departed = extract_system_time_from_istream(in);
            skip_one_whitespace_char(in);

            u64 sloc_len = 0;
            in >> sloc_len;
            skip_one_whitespace_char(in);

            wd_history[i].called_from.resize(sloc_len);
            in.read(wd_history[i].called_from.data(), sloc_len);

            path_force_separator(wd_history[i].path, dir_separator);
            print_debug_msg("[%s] history[%zu] = { [%s] [%s] }", file_name, i, wd_history[i].path.data(), wd_history[i].called_from.c_str());
        }
    }
    catch (...) {
        return false;
    }

    return true;
}

void explorer_window::push_history_item(swan_path const &new_latest_entry, std::source_location sloc) noexcept
{
    char dir_sep_utf8 = global_state::settings().dir_separator_utf8;

    swan_path new_latest_entry_clean = new_latest_entry;
    path_force_separator(new_latest_entry_clean, dir_sep_utf8);
    while (path_pop_back_if(new_latest_entry_clean, dir_sep_utf8));
    new_latest_entry_clean = path_reconstruct_canonically(new_latest_entry_clean.data(), dir_sep_utf8);

    bool history_empty = this->wd_history.empty();

    if (!history_empty && path_loosely_same(new_latest_entry_clean, this->wd_history.back().path.data())) {
        return; // avoid pushing adjacent duplicates
    }

    if (!history_empty) {
        u64 num_trailing_history_items_to_del = this->wd_history.size() - this->wd_history_pos - 1u;

        for (u64 i = 0; i < num_trailing_history_items_to_del; ++i) {
            this->wd_history.pop_back();
        }

        if (this->wd_history.size() == explorer_window::MAX_WD_HISTORY_SIZE) {
            this->wd_history.pop_front();
        }
    }

    if (history_empty) {
        this->wd_history_pos = 0;
    } else {
        this->wd_history_pos += 1;
    }

    this->wd_history.push_back({});
    this->wd_history.back().path = new_latest_entry_clean;
    this->wd_history.back().time_departed = current_time_system();
    this->wd_history.back().called_from = make_str_static<256>("%s  %s:%ld", sloc.function_name(), sloc.file_name(), sloc.line()).data();
}

struct ascend_result
{
    bool success;
    swan_path parent_dir;
};

static
ascend_result try_ascend_directory(explorer_window &expl) noexcept
{
    char dir_sep_utf8 = global_state::settings().dir_separator_utf8;
    ascend_result res = {};
    res.parent_dir = expl.cwd;

    // if there is a trailing separator, remove it
    path_pop_back_if(res.parent_dir, dir_sep_utf8);
    // remove anything between end and final separator
    while (path_pop_back_if_not(res.parent_dir, dir_sep_utf8));

    auto [parent_dir_exists, _] = expl.update_cwd_entries(query_filesystem, res.parent_dir.data());
    res.success = parent_dir_exists;
    print_debug_msg("[ %d ] try_ascend_directory parent_dir=[%s] res.success=%d", expl.id, res.parent_dir.data(), res.success);

    if (parent_dir_exists) {
        if (!path_is_empty(expl.cwd)) {
            expl.push_history_item(res.parent_dir);
        }
        expl.filter_error.clear();
        expl.cwd_latest_selected_dirent_idx = explorer_window::NO_SELECTION;
        expl.cwd_latest_selected_dirent_idx_changed = false;
        expl.cwd = res.parent_dir;
        expl.set_latest_valid_cwd(expl.cwd); // this may mutate filter
        (void) expl.update_cwd_entries(filter, res.parent_dir.data());
        (void) expl.save_to_disk();
    }

    return res;
}

struct descend_result
{
    bool success;
    std::string err_msg;
};

static
descend_result try_descend_to_directory(explorer_window &expl, char const *target_utf8) noexcept
{
    char dir_sep_utf8 = global_state::settings().dir_separator_utf8;

    swan_path new_cwd_utf8 = expl.cwd;

    bool prepend_separator = path_length(expl.cwd) > 0;
    if (!path_append(new_cwd_utf8, target_utf8, dir_sep_utf8, prepend_separator)) {
        print_debug_msg("[ %d ] FAILED path_append, new_cwd_utf8 = [%s], append data = [%c%s]", expl.id, new_cwd_utf8.data(), dir_sep_utf8, target_utf8);
        descend_result res;
        res.success = false;
        res.err_msg = "Max path length exceeded when trying to append target to current working directory path.";
        return res;
    }

    wchar_t new_cwd_utf16[MAX_PATH]; init_empty_cstr(new_cwd_utf16);

    if (!utf8_to_utf16(new_cwd_utf8.data(), new_cwd_utf16, lengthof(new_cwd_utf16))) {
        descend_result res;
        res.success = false;
        res.err_msg = "Conversion of new cwd path from UTF-8 to UTF-16.";
        return res;
    }

    wchar_t new_cwd_canonical_utf16[MAX_PATH]; init_empty_cstr(new_cwd_canonical_utf16);

    {
        HRESULT handle = PathCchCanonicalize(new_cwd_canonical_utf16, lengthof(new_cwd_canonical_utf16), new_cwd_utf16);
        if (handle != S_OK) {
            descend_result res;
            res.success = false;
            switch (handle) {
                case E_INVALIDARG: res.err_msg = "PathCchCanonicalize E_INVALIDARG - the cchPathOut value is > PATHCCH_MAX_CCH."; break;
                case E_OUTOFMEMORY: res.err_msg = "PathCchCanonicalize E_OUTOFMEMORY - the function could not allocate a buffer of the necessary size."; break;
                default: res.err_msg = "Unknown PathCchCanonicalize error."; break;
            }
            return res;
        }
    }

    swan_path new_cwd_canoncial_utf8;

    if (!utf16_to_utf8(new_cwd_canonical_utf16, new_cwd_canoncial_utf8.data(), new_cwd_canoncial_utf8.size())) {
        descend_result res;
        res.success = false;
        res.err_msg = "Conversion of new canonical cwd path from UTF-8 to UTF-16.";
        return res;
    }

    auto [cwd_exists, _] = expl.update_cwd_entries(query_filesystem, new_cwd_canoncial_utf8.data());

    if (!cwd_exists) {
        print_debug_msg("[ %d ] target directory not found", expl.id);
        descend_result res;
        res.success = false;
        res.err_msg = make_str("Target directory [%s] not found.", new_cwd_canoncial_utf8.data());
        return res;
    }

    expl.push_history_item(new_cwd_canoncial_utf8);
    expl.cwd = new_cwd_canoncial_utf8;
    expl.set_latest_valid_cwd(expl.cwd); // this may mutate filter
    (void) expl.update_cwd_entries(filter, new_cwd_canoncial_utf8.data());
    expl.cwd_latest_selected_dirent_idx = explorer_window::NO_SELECTION;
    expl.cwd_latest_selected_dirent_idx_changed = false;
    expl.filter_error.clear();
    (void) expl.save_to_disk();

    descend_result res;
    res.success = true;
    res.err_msg = "";

    return res;
}

struct cwd_text_input_callback_user_data
{
    s64 expl_id;
    wchar_t dir_sep_utf16;
    bool edit_occurred;
    char *text_content;
};

static
s32 cwd_text_input_callback(ImGuiInputTextCallbackData *data) noexcept
{
    auto user_data = (cwd_text_input_callback_user_data *)(data->UserData);
    user_data->edit_occurred = false;

    auto is_separator = [](wchar_t ch) noexcept { return ch == L'/' || ch == L'\\'; };

    if (data->EventFlag == ImGuiInputTextFlags_CallbackCharFilter) {
        if (is_separator(data->EventChar)) {
            data->EventChar = user_data->dir_sep_utf16;
        }
    }
    else if (data->EventFlag == ImGuiInputTextFlags_CallbackEdit) {
        print_debug_msg("[ %d ] ImGuiInputTextFlags_CallbackEdit Buf:[%s]", user_data->expl_id, data->Buf);
        user_data->edit_occurred = true;
    }

    return 0;
}

static
void render_debug_info(explorer_window &expl, u64 size_unit_multiplier) noexcept
{
#if 1
    {
        u64 bytes_occupied = expl.cwd_entries.size() * sizeof(swan_path);
        u64 bytes_actually_used = 0;

        for (auto const &dirent : expl.cwd_entries) {
            bytes_actually_used += path_length(dirent.basic.path);
        }

        char buffer1[32]; init_empty_cstr(buffer1);
        format_file_size(bytes_occupied, buffer1, lengthof(buffer1), size_unit_multiplier);

        f64 usage_ratio = ( f64(bytes_actually_used) + (bytes_occupied == 0) ) / ( f64(bytes_occupied) + (bytes_occupied == 0) );
        f64 waste_percent = 100.0 - (usage_ratio * 100.0);
        u64 bytes_wasted = u64( bytes_occupied * (1.0 - usage_ratio) );

        char buffer2[32]; init_empty_cstr(buffer2);
        format_file_size(bytes_wasted, buffer2, lengthof(buffer1), size_unit_multiplier);

        imgui::Text("swan_path_t memory footprint: %s, %3.1lf %% waste -> %s", buffer1, waste_percent, buffer2);
    }
#endif

    imgui::Text("latest_valid_cwd: [%s]", expl.latest_valid_cwd.data());
    imgui::Text("select_cwd_entries_on_next_update.size(): %zu", expl.select_cwd_entries_on_next_update.size());
    imgui::Text("num_file_finds: %zu", expl.num_file_finds);
    imgui::Text("cwd_latest_selected_dirent_idx: %zu", expl.cwd_latest_selected_dirent_idx);
    imgui::Text("latest_save_to_disk_result: %d", expl.latest_save_to_disk_result);

    imgui::Text("entries_to_select_sort: %.1lf us", expl.update_cwd_entries_timing_samples.empty() ? NAN : expl.update_cwd_entries_timing_samples.back().entries_to_select_sort);
    imgui::Text("entries_to_select_search: %.1lf us", expl.update_cwd_entries_timing_samples.empty() ? NAN : expl.update_cwd_entries_timing_samples.back().entries_to_select_search);

#if 0
    {
        char const *labels[] = {
            "searchpath_setup_us",
            "filesystem_us",
            "filter_us",
            "regex_ctor_us",
            "remainder_us",
        };

        constexpr u64 rows = explorer_window::NUM_TIMING_SAMPLES;
        constexpr u64 cols = lengthof(labels);

        f64 matrix[rows][cols] = {};

        for (u64 r = 0; r < rows; ++r) {
            memcpy(&matrix[r][0], &expl.update_cwd_entries_timing_samples[r], 4 * sizeof(f64));
            // matrix[0][r] = expl.update_cwd_entries_timing_samples[r].searchpath_setup_us;
            // matrix[1][r] = expl.update_cwd_entries_timing_samples[r].filesystem_us;
            // matrix[2][r] = expl.update_cwd_entries_timing_samples[r].filter_us;
            // matrix[3][r] = expl.update_cwd_entries_timing_samples[r].regex_ctor_us;

            // remainder
            matrix[4][r] = expl.update_cwd_entries_timing_samples[r].total_us - (
                expl.update_cwd_entries_timing_samples[r].searchpath_setup_us +
                expl.update_cwd_entries_timing_samples[r].filesystem_us +
                expl.update_cwd_entries_timing_samples[r].filter_us +
                expl.update_cwd_entries_timing_samples[r].regex_ctor_us
            );
        }

        if (implot::BeginPlot("update_cwd_entries timings")) {
            implot::PlotBarGroups(labels, matrix, rows, cols, 0.67, 0, ImPlotBarGroupsFlags_Stacked);
            implot::EndPlot();
        }
    }
#endif
}

static
void render_num_cwd_items(cwd_count_info const &cnt) noexcept
{
    if (cnt.filtered_dirents == 0) {
        imgui::Text("%zu items", cnt.child_dirents);
    } else {
        u64 cnt_visible = cnt.child_dirents - cnt.filtered_dirents;
        imgui::Text("%zu (of %zu) items", cnt_visible, cnt.child_dirents);
    }

    if (imgui::IsItemHovered() && imgui::BeginTooltip()) {
        if (cnt.child_dirents == 0) {
            imgui::TextUnformatted("No items in this directory.");
        }
        else {
            // imgui::Text("%zu total entries in this directory.", cnt.child_dirents);
            // imgui::Spacing();

        #if 1
            struct occupancy
            {
                char const *type_name = nullptr;
                u64 my_count = 0;
                u64 total_count = 0;
                ImVec4 type_color = {};

                bool operator>(occupancy const &other) const noexcept { return this->fraction() > other.fraction(); }
                f32  fraction()                        const noexcept { return ( f32(my_count) / f32(total_count) ); }
                f32  percentage()                      const noexcept { return this->fraction() * 100.0f; }
            };

            std::array<occupancy, 3> occup = {{
                { .type_name = "directories",  .my_count = cnt.child_directories,  .total_count = cnt.child_dirents,  .type_color = get_color(basic_dirent::kind::directory) },
                { .type_name = "symlinks   ",  .my_count = cnt.child_symlinks,     .total_count = cnt.child_dirents,  .type_color = get_color(basic_dirent::kind::symlink_ambiguous) },
                { .type_name = "files      ",  .my_count = cnt.child_files,        .total_count = cnt.child_dirents,  .type_color = get_color(basic_dirent::kind::file) },
            }};

            std::sort(occup.begin(), occup.end(), std::greater<occupancy>());

            // f32 values[] = { occup[0].fraction(), occup[1].fraction(), occup[2].fraction() };
            // imgui::PlotHistogram("## occupancy", values, lengthof(values), 0, "", 0, 1, { 100, 200 });

            for (auto const &entity : occup) {
                // if (entity.my_count == 0) {
                //    continue;
                // }
                imgui::ScopedStyle<ImVec4> c(imgui::GetStyle().Colors[ImGuiCol_PlotHistogram], entity.type_color);
                imgui::ProgressBar(entity.fraction(), {});
                imgui::SameLineSpaced(1);
                imgui::Text("%s   %zu ", entity.type_name, entity.my_count);
            }

        #else
            f64 percent_dirs     = (f64(cnt.child_directories) / f64(cnt.child_dirents)) * 100.0;
            f64 percent_symlinks = (f64(cnt.child_symlinks)    / f64(cnt.child_dirents)) * 100.0;
            f64 percent_files    = (f64(cnt.child_files)       / f64(cnt.child_dirents)) * 100.0;

            if (cnt.child_directories > 0) {
                imgui::TextColored(directory_color(), "%zu (%.2lf %%) director%s.", cnt.child_directories, percent_dirs, pluralized(cnt.child_directories, "y", "ies"));
            }
            if (cnt.child_symlinks > 0) {
                imgui::TextColored(symlink_color(), "%zu (%.2lf %%) symlink%s.", cnt.child_symlinks, percent_symlinks, pluralized(cnt.child_symlinks, "", "s"));
            }
            if (cnt.child_files > 0) {
                imgui::TextColored(file_color(), "%zu (%.2lf %%) file%s.", cnt.child_files, percent_files, pluralized(cnt.child_files, "", "s"));
            }
        #endif
        }

        imgui::EndTooltip();
    }
}

static
void render_num_cwd_items_filtered(explorer_window &expl, cwd_count_info const &cnt) noexcept
{
    imgui::Text("(%zu filtered)", cnt.filtered_dirents);

    if (imgui::IsItemClicked()) {
        expl.show_filter_window = true;
    }

    if (imgui::IsItemHovered() && imgui::BeginTooltip()) {
        if (cnt.filtered_directories > 0) {
            imgui::TextColored(directory_color(), "%zu director%s filtered.", cnt.filtered_directories, pluralized(cnt.filtered_directories, "y", "ies"));
        }
        if (cnt.filtered_symlinks > 0) {
            imgui::TextColored(symlink_color(), "%zu symlink%s filtered.", cnt.filtered_symlinks, pluralized(cnt.filtered_symlinks, "", "s"));
        }
        if (cnt.filtered_files > 0) {
            imgui::TextColored(file_color(), "%zu file%s filtered.", cnt.filtered_files, pluralized(cnt.filtered_files, "", "s"));
        }

        if (!expl.show_filter_window) {
            imgui::Spacing();
            imgui::TextUnformatted("Click to expose filter.");
        }

        imgui::EndTooltip();
    }
}

static
void render_num_cwd_items_selected(explorer_window &expl, cwd_count_info const &cnt) noexcept
{
    assert(cnt.child_dirents > 0);

    imgui::Text("(%zu selected)", cnt.selected_dirents);

    if (imgui::IsItemHovered() && imgui::BeginTooltip()) {
        if (cnt.selected_directories > 0) {
            imgui::TextColored(directory_color(), "%zu director%s selected.", cnt.selected_directories, pluralized(cnt.selected_directories, "y", "ies"));
        }
        if (cnt.selected_symlinks > 0) {
            imgui::TextColored(symlink_color(), "%zu symlink%s selected.", cnt.selected_symlinks, pluralized(cnt.selected_symlinks, "", "s"));
        }
        if (cnt.selected_files > 0) {
            imgui::TextColored(file_color(), "%zu file%s selected.", cnt.selected_files, pluralized(cnt.selected_files, "", "s"));
        }

        imgui::Spacing();

        imgui::TextUnformatted("Click to see next selected entry.");

        imgui::EndTooltip();
    }

    if (imgui::IsItemClicked()) {
        bool have_not_yet_spotlighted_in_cwd = expl.nth_last_cwd_dirent_scrolled == u64(-1);
        bool currently_spotlighting_bottom_most_dirent_therefore_need_wrap_to_top = expl.nth_last_cwd_dirent_scrolled == cnt.selected_dirents - 1;
        bool new_dirent_selection_recently = expl.cwd_latest_selected_dirent_idx_changed;

        bool start_spotlight_from_the_top = have_not_yet_spotlighted_in_cwd ||
                                            currently_spotlighting_bottom_most_dirent_therefore_need_wrap_to_top ||
                                            new_dirent_selection_recently;

        if (start_spotlight_from_the_top) {
            expl.nth_last_cwd_dirent_scrolled = 0;
            expl.cwd_latest_selected_dirent_idx_changed = false;
        } else {
            expl.nth_last_cwd_dirent_scrolled += 1;
        }

        // TODO: fix filter controls being hidden by this request
        expl.scroll_to_nth_selected_entry_next_frame = expl.nth_last_cwd_dirent_scrolled;
    }
}

static
void render_file_op_payload_hint() noexcept
{
    {
        imgui::ScopedTextColor tc(warning_color());
        auto label = make_str_static<64>("%s %zu", ICON_CI_COPY, g_file_op_payload.items.size());

        if (imgui::Selectable(label.data(), false, 0, imgui::CalcTextSize(label.data()))) {
            imgui::OpenPopup("File Operation Payload");
        }
    }
    if (imgui::IsItemHovered() && imgui::IsMouseClicked(ImGuiMouseButton_Right)) {
        g_file_op_payload.clear();
    }
    if (imgui::IsItemHovered()) {
        imgui::SetTooltip("File operations ready to be executed.\n"
                          "\n"
                          "[L click]   view operations\n"
                          "[R click]  clear operations\n");
    }
    if (imgui::BeginPopup("File Operation Payload")) {
        imgui::Text("%zu operation%s will be executed on next paste.",
            g_file_op_payload.items.size(),
            g_file_op_payload.items.size() > 1 ? "s" : "");

        imgui::Spacing();
        imgui::Separator();

        if (imgui::BeginTable("file_operation_command_buf", 2)) {
            ImGuiListClipper clipper;
            {
                u64 num_dirents_to_render = g_file_op_payload.items.size();
                assert(num_dirents_to_render <= (u64)INT32_MAX);
                clipper.Begin(s32(num_dirents_to_render));
            }

            while (clipper.Step())
            for (u64 i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                auto const &item = g_file_op_payload.items[i];

                imgui::TableNextColumn();
                imgui::TextUnformatted(item.operation_desc);

                imgui::TableNextColumn();
                imgui::TextColored(get_color(item.type), get_icon(item.type));
                imgui::SameLine();
                imgui::TextUnformatted(item.path.data());
            }

            imgui::EndTable();
        }

        imgui::EndPopup();
    }
}

static
void render_back_to_prev_valid_cwd_button(explorer_window &expl) noexcept
{
    auto io = imgui::GetIO();

    imgui::ScopedDisable disabled(expl.wd_history_pos == 0);

    if (imgui::Button(ICON_CI_CHEVRON_LEFT "## back")) {
        print_debug_msg("[ %d ] back arrow button triggered", expl.id);

        if (io.KeyShift || io.KeyCtrl) {
            expl.wd_history_pos = 0;
        } else {
            expl.wd_history_pos -= 1;
        }

        expl.cwd = expl.wd_history[expl.wd_history_pos].path;
        auto [back_dir_exists, _] = expl.update_cwd_entries(query_filesystem, expl.cwd.data());
        if (back_dir_exists) {
            expl.set_latest_valid_cwd(expl.cwd); // this may mutate filter
            (void) expl.update_cwd_entries(filter, expl.cwd.data());
            (void) expl.save_to_disk();
        }
    }

    if (imgui::IsItemHovered()) {
        imgui::SetTooltip("Back");
    }
}

static
void render_forward_to_next_valid_cwd_button(explorer_window &expl) noexcept
{
    u64 wd_history_last_idx = expl.wd_history.empty() ? 0 : expl.wd_history.size() - 1;
    auto io = imgui::GetIO();

    imgui::ScopedDisable disabled(expl.wd_history_pos == wd_history_last_idx);

    if (imgui::Button(ICON_CI_CHEVRON_RIGHT "## forward")) {
        print_debug_msg("[ %d ] forward arrow button triggered", expl.id);

        if (io.KeyShift || io.KeyCtrl) {
            expl.wd_history_pos = wd_history_last_idx;
        } else {
            expl.wd_history_pos += 1;
        }

        expl.cwd = expl.wd_history[expl.wd_history_pos].path;
        auto [forward_dir_exists, _] = expl.update_cwd_entries(query_filesystem, expl.cwd.data());
        if (forward_dir_exists) {
            expl.set_latest_valid_cwd(expl.cwd); // this may mutate filter
            (void) expl.update_cwd_entries(filter, expl.cwd.data());
        }
    }

    if (imgui::IsItemHovered()) {
        imgui::SetTooltip("Forward");
    }
}

static
bool render_history_browser_popup(explorer_window &expl, bool cwd_exists, [[maybe_unused]] bool show_history_label = true) noexcept
{
    auto cleanup_and_close_popup = []() noexcept {
        imgui::CloseCurrentPopup();
    };

    SCOPE_EXIT { imgui::EndPopup(); };

    imgui::AlignTextToFramePadding();
    imgui::TextUnformatted("History");

    imgui::SameLine();

    {
        imgui::ScopedDisable d(expl.wd_history.empty());

        if (imgui::Button(ICON_CI_CLEAR_ALL "## explorer_window History")) {
            expl.wd_history.clear();
            expl.wd_history_pos = 0;

            if (cwd_exists) {
                expl.push_history_item(expl.cwd);
            }

            expl.save_to_disk();
            cleanup_and_close_popup();
        }
    }
    if (imgui::IsItemHovered()) {
        imgui::SetTooltip("Clear history");
    }

    imgui::Separator();

    if (!expl.wd_history.empty()) {
        ImGuiTableFlags table_flags =
            ImGuiTableFlags_SizingStretchProp|
            ImGuiTableFlags_BordersInnerV|
            ImGuiTableFlags_Resizable
        ;

    #if DEBUG_MODE
        s32 num_columns = 5;
    #else
        s32 num_columns = 4;
    #endif

        if (imgui::BeginTable("history_table", num_columns, table_flags)) {
            u64 i = expl.wd_history.size() - 1;
            u64 i_inverse = 0;

            for (auto iter = expl.wd_history.rbegin(); iter != expl.wd_history.rend(); ++iter, --i, ++i_inverse) {
                imgui::TableNextRow();
                explorer_window::history_item const &hist_item = *iter;

                imgui::TableNextColumn();
                if (i == expl.wd_history_pos) {
                    imgui::TextColored(success_color(), ICON_FA_LONG_ARROW_ALT_RIGHT);
                }

                imgui::TableNextColumn();
                imgui::Text("%3zu ", i_inverse + 1);

                imgui::TableNextColumn();
                {
                    auto label = make_str_static<1200>("%s ## %zu", hist_item.path.data(), i);
                    bool pressed;
                    {
                        imgui::ScopedTextColor tc(directory_color());
                        pressed = imgui::Selectable(label.data(), false, ImGuiSelectableFlags_SpanAllColumns);
                        imgui::RenderTooltipWhenColumnTextTruncated(2, hist_item.path.data());
                    }

                    if (pressed) {
                        expl.wd_history_pos = i;
                        expl.cwd = expl.wd_history[i].path;

                        cleanup_and_close_popup();
                        imgui::EndTable();

                        return true;
                    }
                }

                imgui::TableNextColumn();
                {
                    auto when = compute_when_str(hist_item.time_departed, current_time_system());
                    imgui::TextUnformatted(when.data());
                }

            #if DEBUG_MODE
                imgui::TableNextColumn();
                imgui::TextUnformatted(hist_item.called_from.c_str());
            #endif
            }

            imgui::EndTable();
        }
    }

    ImVec2 popup_pos = imgui::GetWindowPos();
    ImRect popup_rect(popup_pos, ImVec2(popup_pos.x + imgui::GetWindowWidth(), popup_pos.y + imgui::GetWindowHeight()));

    if (imgui::IsWindowFocused() && imgui::IsMouseClicked(ImGuiMouseButton_Left) && !imgui::IsMouseHoveringRect(popup_rect.Min, popup_rect.Max)) {
        cleanup_and_close_popup();
    }

    return false;
}

static
void render_pins_popup(explorer_window &expl) noexcept
{
    imgui::AlignTextToFramePadding();
    imgui::TextUnformatted("Pins");

    imgui::SameLine();

    if (imgui::Button("Manage")) {
        global_state::settings().show.pinned = true;
        (void) global_state::settings().save_to_disk();
        ImGui::SetWindowFocus(swan_windows::get_name(swan_windows::id::pinned));
    }

    imgui::Separator();

    auto const &pins = global_state::pinned_get();

    if (pins.empty()) {
        imgui::TextUnformatted("(empty)");
    }
    else {
        for (auto const &pin : pins) {
            {
                imgui::ScopedTextColor tc(pin.color);
                bool selected = false;

                if (imgui::Selectable(pin.label.c_str(), &selected)) {
                    if (!directory_exists(pin.path.data())) {
                        std::string action = make_str("Open pin [%s].", pin.label.c_str());
                        std::string failed = make_str("Pin path [%s] does not exit.", pin.path.data());
                        swan_popup_modals::open_error(action.c_str(), failed.c_str());
                    }
                    else {
                        auto [pin_is_valid_dir, _] = expl.update_cwd_entries(query_filesystem, pin.path.data());
                        if (pin_is_valid_dir) {
                            expl.cwd = pin.path;
                            expl.push_history_item(pin.path);
                            expl.set_latest_valid_cwd(pin.path); // this may mutate filter
                            (void) expl.update_cwd_entries(filter, pin.path.data());
                            (void) expl.save_to_disk();
                        } else {
                            (void) expl.update_cwd_entries(full_refresh, expl.cwd.data());
                        }
                    }
                }
            }
            if (imgui::IsItemHovered()) {
                imgui::SetTooltip("%s", pin.path.data());
            }
        }
    }
}

static
void render_filter_reset_button(explorer_window &expl) noexcept
{
    if (imgui::Button(ICON_CI_REDO "## clear_filter")) {
        expl.reset_filter();
        (void) expl.update_cwd_entries(filter, expl.cwd.data());
        (void) expl.save_to_disk();
    }
    if (imgui::IsItemHovered()) {
        imgui::SetTooltip("Reset filter settings");
    }
}

static
void render_drives_table(explorer_window &expl, char dir_sep_utf8, u64 size_unit_multiplier) noexcept
{
    static precise_time_point_t s_last_refresh_time = {};
    static drive_list_t s_drives = {};

    // refresh drives occasionally
    {
        precise_time_point_t now = current_time_precise();
        s64 diff_ms = compute_diff_ms(s_last_refresh_time, now);
        if (diff_ms >= 1000) {
            s_drives = query_drive_list();
            s_last_refresh_time = current_time_precise();
        }
    }

    enum drive_table_col_id : s32
    {
        drive_table_col_id_letter,
        drive_table_col_id_name,
        drive_table_col_id_filesystem,
        drive_table_col_id_total_space,
        drive_table_col_id_used_percent,
        drive_table_col_id_free_space,
        drive_table_col_id_count,
    };

    s32 table_flags =
        ImGuiTableFlags_SizingStretchSame|
        ImGuiTableFlags_BordersInnerV|
        ImGuiTableFlags_Reorderable|
        ImGuiTableFlags_Resizable|ImGuiTableFlags_BordersV|
        (global_state::settings().explorer_cwd_entries_table_borders_in_body ? 0 : ImGuiTableFlags_NoBordersInBody)|
        (global_state::settings().explorer_cwd_entries_table_alt_row_bg ? ImGuiTableFlags_RowBg : 0)
    ;

    if (imgui::BeginTable("drives", drive_table_col_id_count, table_flags)) {
        imgui::TableSetupColumn("Drive", ImGuiTableColumnFlags_NoSort, 0.0f, drive_table_col_id_letter);
        imgui::TableSetupColumn("Name", ImGuiTableColumnFlags_NoSort, 0.0f, drive_table_col_id_name);
        imgui::TableSetupColumn("Filesystem", ImGuiTableColumnFlags_NoSort, 0.0f, drive_table_col_id_filesystem);
        imgui::TableSetupColumn("Total Space", ImGuiTableColumnFlags_NoSort, 0.0f, drive_table_col_id_total_space);
        imgui::TableSetupColumn("Usage", ImGuiTableColumnFlags_NoSort, 0.0f, drive_table_col_id_used_percent);
        imgui::TableSetupColumn("Free Space", ImGuiTableColumnFlags_NoSort, 0.0f, drive_table_col_id_free_space);
        ImGui::TableSetupScrollFreeze(0, 1);
        imgui::TableHeadersRow();

        for (auto &drive : s_drives) {
            imgui::TableNextRow();

            if (imgui::TableSetColumnIndex(drive_table_col_id_letter)) {
                imgui::TextColored(get_color(basic_dirent::kind::directory), "%C:", drive.letter);
            }

            if (imgui::TableSetColumnIndex(drive_table_col_id_name)) {
                bool selected = false;

                if (imgui::Selectable(drive.name_utf8[0] == '\0' ? "Local Disk" : drive.name_utf8,
                                        &selected, ImGuiSelectableFlags_SpanAllColumns))
                {
                    char root[] = { drive.letter, ':', dir_sep_utf8, '\0' };
                    expl.cwd = expl.latest_valid_cwd = path_create(root);
                    expl.set_latest_valid_cwd(expl.cwd);
                    expl.push_history_item(expl.cwd);
                    auto [drive_exists, _] = expl.update_cwd_entries(query_filesystem, expl.cwd.data());
                    if (drive_exists) {
                        (void) expl.update_cwd_entries(filter, expl.cwd.data());
                        (void) expl.save_to_disk();
                    } else {
                        // TODO: handle error
                    }
                }
            }

            if (imgui::TableSetColumnIndex(drive_table_col_id_filesystem)) {
                imgui::TextUnformatted(drive.filesystem_name_utf8);
            }

            if (imgui::TableSetColumnIndex(drive_table_col_id_total_space)) {
                char buffer[32]; init_empty_cstr(buffer);
                format_file_size(drive.total_bytes, buffer, lengthof(buffer), size_unit_multiplier);
                imgui::TextUnformatted(buffer);
            }

            if (imgui::TableSetColumnIndex(drive_table_col_id_free_space)) {
                char buffer[32]; init_empty_cstr(buffer);
                format_file_size(drive.available_bytes, buffer, lengthof(buffer), size_unit_multiplier);
                imgui::TextUnformatted(buffer);
            }

            if (imgui::TableSetColumnIndex(drive_table_col_id_used_percent)) {
                u64 used_bytes = drive.total_bytes - drive.available_bytes;
                f32 fraction_used = ( f32(used_bytes) / f32(drive.total_bytes) );
                f32 percent_used = fraction_used * 100.0f;
                imgui::Text("%3.0lf%%", percent_used);
                imgui::SameLine();

                f32 red = fraction_used < 0.5f ? 0 : ( 4 * ((fraction_used - 0.5f) * (fraction_used - 0.5f)) );
                f32 green = 1.5f - fraction_used;
                f32 blue = 0;

                imgui::ScopedColor c(ImGuiCol_PlotHistogram, ImVec4(red, green, blue, 1));
                imgui::ProgressBar(f32(used_bytes) / f32(drive.total_bytes), ImVec2(-1, imgui::CalcTextSize("1").y), "");
            }
        }

        imgui::EndTable();
    }
}

static
void render_filter_text_input(explorer_window &expl, bool window_hovered, bool ctrl_key_down, cwd_count_info const &cnt) noexcept
{
    auto width = std::max(
        imgui::CalcTextSize(expl.filter_text.data()).x + (imgui::GetStyle().FramePadding.x * 2) + 10.f,
        imgui::CalcTextSize("1").x * 20.75f
    );

    ImVec4 low_warning = warning_color(); low_warning.w /= 1.5;
    bool valid_non_empty_cwd = !path_is_empty(expl.cwd) && cnt.child_dirents > 0;
    imgui::ScopedColor b(ImGuiCol_Border, low_warning, valid_non_empty_cwd && cnt.filtered_dirents == cnt.child_dirents);
    imgui::ScopedItemWidth iw(width);

    if (imgui::InputTextWithHint("##filter", ICON_CI_FILTER, expl.filter_text.data(), expl.filter_text.size())) {
        (void) expl.update_cwd_entries(filter, expl.cwd.data());
        (void) expl.save_to_disk();
    }

    if (window_hovered && ctrl_key_down && imgui::IsKeyPressed(ImGuiKey_F)) {
        imgui::ActivateItemByID(imgui::GetID("##filter"));
    }
}

static
void render_filter_type_toggler_buttons(explorer_window &expl) noexcept
{
    std::tuple<basic_dirent::kind, bool &, char const *> button_defs[] = {
        { basic_dirent::kind::directory,             expl.filter_show_directories,          "directories"         },
        { basic_dirent::kind::symlink_to_directory,  expl.filter_show_symlink_directories,  "directory shortcuts" },
        { basic_dirent::kind::file,                  expl.filter_show_files,                "files"               },
        { basic_dirent::kind::symlink_to_file,       expl.filter_show_symlink_files,        "file shortcuts"      },
        { basic_dirent::kind::invalid_symlink,       expl.filter_show_invalid_symlinks,     "invalid shortcuts"   },
    };

    for (auto &button_def : button_defs) {
        basic_dirent::kind type = std::get<0>(button_def);
        bool &show              = std::get<1>(button_def);
        char const *type_str    = std::get<2>(button_def);

        imgui::SameLine(0, imgui::GetStyle().ItemSpacing.x / 2);

        {
            imgui::ScopedColor ct(ImGuiCol_Text, show ? get_color(type) : ImVec4(0.3f, 0.3f, 0.3f, 1));
            imgui::Button(get_icon(type));
        }

        if (imgui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
            imgui::SetTooltip("[      L click]  Toggle %s visibility\n"
                              "[Ctrl  L click]  Solo type\n"
                              "[Shift L click]  Unmute all type", type_str);
        }

        if (imgui::IsItemClicked()) {
            auto &io = imgui::GetIO();
            if (io.KeyCtrl) { // disable all but self
                for (auto &button : button_defs) {
                    std::get<1>(button) = false;
                }
                show = true;
            }
            else if (io.KeyShift) { // enable all
                for (auto &button : button_defs) {
                    std::get<1>(button) = true;
                }
            }
            else {
                flip_bool(show);
            }
            (void) expl.update_cwd_entries(filter, expl.cwd.data());
            (void) expl.save_to_disk();
        }
    }
}

static
void render_filter_mode_toggle(explorer_window &expl) noexcept
{
    static char const *s_filter_modes[] = {
         ICON_CI_WHOLE_WORD, // ICON_FA_FONT,
         ICON_CI_REGEX, // ICON_FA_ASTERISK,
        // "(" ICON_CI_REGEX ")",
    };

    static_assert(lengthof(s_filter_modes) == (u64)explorer_window::filter_mode::count);

    static char const *s_current_mode = nullptr;
    s_current_mode = s_filter_modes[expl.filter_mode];

    auto label = make_str_static<64>("%s##%zu", s_current_mode, expl.filter_mode);

    if (imgui::Button(label.data())) {
        inc_or_wrap<u64>((u64 &)expl.filter_mode, 0, u64(explorer_window::filter_mode::count) - 1);
        (void) expl.update_cwd_entries(filter, expl.cwd.data());
        (void) expl.save_to_disk();
    }

    if (imgui::IsItemHovered()) {
        char const *mode = nullptr;
        switch (expl.filter_mode) {
            case explorer_window::filter_mode::contains: mode = "CONTAINS"; break;
            case explorer_window::filter_mode::regex_match: mode = "REGEXP_MATCH"; break;
            // case explorer_window::filter_mode::regex_find: mode = "REGEXP_FIND"; break;
            default: break;
        }
        imgui::SetTooltip("Mode: %s\n", mode);
    }
}

static
void render_filter_case_sensitivity_button(explorer_window &expl) noexcept
{
    {
        imgui::ScopedStyle<f32> s(imgui::GetStyle().Alpha, expl.filter_case_sensitive ? 1 : 0.4f);

        if (imgui::Button(ICON_CI_CASE_SENSITIVE)) { // ICON_FA_CROSSHAIRS
            flip_bool(expl.filter_case_sensitive);
            (void) expl.update_cwd_entries(filter, expl.cwd.data());
            (void) expl.save_to_disk();
        }
    }
    if (imgui::IsItemHovered()) {
        imgui::SetTooltip("Case sensitive filter %s\n", expl.filter_case_sensitive ? "ON" : "OFF");
    }
}

static
void render_filter_polarity_button(explorer_window &expl) noexcept
{
    if (imgui::Button(expl.filter_polarity ? (ICON_CI_EYE "## filter_polarity") : (ICON_CI_EYE_CLOSED "## filter_polarity"))) {
        flip_bool(expl.filter_polarity);
        (void) expl.update_cwd_entries(filter, expl.cwd.data());
        (void) expl.save_to_disk();
    }
    if (imgui::IsItemHovered()) {
        imgui::SetTooltip("Polarity: %s matches\n", expl.filter_polarity ? "SHOW" : "HIDE");
    }
}

static
void render_button_pin_cwd(explorer_window &expl, bool cwd_exists_before_edit) noexcept
{
    u64 pin_idx;
    {
        scoped_timer<timer_unit::MICROSECONDS> check_if_pinned_timer(&expl.check_if_pinned_us);
        pin_idx = global_state::pinned_find_idx(expl.cwd);
    }
    bool already_pinned = pin_idx != std::string::npos;

    auto label = make_str_static<8>("%s", already_pinned ? ICON_CI_STAR_FULL : ICON_CI_STAR_EMPTY);

    imgui::ScopedDisable disabled(!cwd_exists_before_edit && !already_pinned);

    if (imgui::Button(label.data())) {
        if (already_pinned) {
            imgui::OpenConfirmationModalWithCallback(
                /* confirmation_id  = */ swan_id_confirm_explorer_unpin_directory,
                /* confirmation_msg = */
                [pin_idx]() noexcept {
                    auto const &pin = global_state::pinned_get()[pin_idx];

                    imgui::TextUnformatted("Are you sure you want to delete the following pin?");
                    imgui::Spacing(2);

                    imgui::SameLineSpaced(1); // indent
                    imgui::TextColored(pin.color, pin.label.c_str());
                    imgui::Spacing();

                    imgui::TextUnformatted("This action cannot be undone.");
                },
                /* on_yes_callback = */
                [pin_idx, &expl]() noexcept {
                    scoped_timer<timer_unit::MICROSECONDS> unpin_timer(&expl.unpin_us);
                    global_state::pinned_remove(pin_idx);
                    (void) global_state::settings().save_to_disk();
                },
                /* confirmation_enabled = */ &(global_state::settings().confirm_explorer_unpin_directory)
            );
        }
        else {
            swan_popup_modals::open_new_pin(expl.cwd, false);
        }
        bool result = global_state::pinned_save_to_disk();
        print_debug_msg("pinned_save_to_disk: %d", result);
    }
    if (imgui::IsItemHovered()) {
        imgui::SetTooltip("%s current working directory", already_pinned ? "Unpin" : "Pin");
    }
}

static
bool render_history_browser_button() noexcept
{
    if (imgui::Button(ICON_CI_HISTORY "##expl.wd_history")) {
        return true;
    }
    if (imgui::IsItemHovered()) {
        imgui::SetTooltip("Open history");
    }
    return false;
}

static
void render_cwd_clicknav(explorer_window &expl, bool cwd_exists, char) noexcept
{
    if (!cwd_exists || path_is_empty(expl.cwd)) {
        return;
    }

    static std::vector<char const *> s_slices = {};
    s_slices.clear();

    swan_path sliced_path = expl.cwd;
    {
        char const *slice = strtok(sliced_path.data(), "\\/");
        while (slice != nullptr) {
            s_slices.push_back(slice);
            slice = strtok(nullptr, "\\/");
        }
    }

    auto cd_to_slice = [&](char const *slice) noexcept {
        char const *slice_end = slice;
        while (*slice_end != '\0') {
            ++slice_end;
        }

        u64 len = slice_end - sliced_path.data();

        char backup_ch = '\0';

        if (len == path_length(expl.cwd)) {
            print_debug_msg("[ %d ] cd_to_slice: slice == cwd, not updating cwd|history", expl.id);
        }
        else {
            backup_ch = expl.cwd[len];
            expl.cwd[len] = '\0';
            expl.push_history_item(expl.cwd);
        }

        auto [exists, _] = expl.update_cwd_entries(query_filesystem, expl.cwd.data());
        if (exists) {
            expl.set_latest_valid_cwd(expl.cwd); // this may mutate filter
            (void) expl.update_cwd_entries(filter, expl.cwd.data());
            (void) expl.save_to_disk();
        }
        else {
            // restore state
            (void) expl.update_cwd_entries(full_refresh, expl.cwd.data());
            if (backup_ch) {
                expl.cwd[len] = backup_ch;
            }
        }

        imgui::CloseCurrentPopup();
    };

    char const *separator = ICON_CI_TRIANGLE_RIGHT;
    imgui::ScopedStyle<f32> s(imgui::GetStyle().ItemSpacing.x, 2);

    for (u64 i = 0; i < s_slices.size() - 1; ++i) {
        char const *slice = s_slices[i];
        auto label = make_str_static<1200>("%s" "## clicknav %zu", slice, i);

        if (imgui::Button(label.data())) {
            cd_to_slice(slice);
        }

        imgui::SameLine();
        imgui::TextDisabled(separator);
        imgui::SameLine();
    }
    imgui::Button(s_slices.back());
}

static
void render_more_controls_button(explorer_window &expl, bool cwd_exists_before_edit, char dir_sep_utf8) noexcept
{
    if (imgui::Button(ICON_CI_ELLIPSIS)) { // ICON_FA_ELLIPSIS_H
        imgui::OpenPopup("more_controls_popup");
    }
    ImRect button_rect(imgui::GetItemRectMin(), imgui::GetItemRectMax());

    imgui::SetNextWindowPos(ImVec2(button_rect.GetBL().x, button_rect.GetBL().y + imgui::GetStyle().ItemSpacing.y), ImGuiCond_Appearing);

    bool open_history_browser = false;

    if (imgui::BeginPopup("more_controls_popup")) {
        open_history_browser = render_history_browser_button();

        imgui::SameLine();

        render_button_pin_cwd(expl, cwd_exists_before_edit);

        if (cwd_exists_before_edit) {
            imgui::SameLine();
            imgui::TextDisabled(ICON_CI_KEBAB_VERTICAL);
            imgui::SameLine();
            render_cwd_clicknav(expl, cwd_exists_before_edit, dir_sep_utf8);
        }

        imgui::EndPopup();
    }

    if (open_history_browser) {
        imgui::OpenPopup("History");
    }
}

static
void render_up_to_cwd_parent_button(explorer_window &expl, bool cwd_exists_before_edit) noexcept
{
    imgui::ScopedDisable disabled(!cwd_exists_before_edit);

    if (imgui::Button(ICON_CI_ARROW_UP "## up")) { // ICON_FA_ARROW_UP
        print_debug_msg("[ %d ] (..) button triggered", expl.id);

        auto result = try_ascend_directory(expl);

        if (!result.success) {
            auto cwd_len = path_length(expl.cwd);

            bool cwd_is_drive = ( cwd_len == 2 && IsCharAlphaA(expl.cwd[0]) && expl.cwd[1] == ':'                               )
                             || ( cwd_len == 3 && IsCharAlphaA(expl.cwd[0]) && expl.cwd[1] == ':' && strchr("\\/", expl.cwd[2]) );

            if (cwd_is_drive) {
                path_clear(expl.cwd);
            } else {
                std::string action = make_str("Ascend to directory [%s]", result.parent_dir.data());
                char const *failed = "Directory not found.";
                swan_popup_modals::open_error(action.c_str(), failed);
            }
        }
    }

    if (imgui::IsItemHovered()) {
        imgui::SetTooltip("Up to parent directory");
    }
}

static
void render_help_icon() noexcept
{
    imgui::AlignTextToFramePadding();
    imgui::TextUnformatted("(?)");

    if (imgui::IsItemHovered()) {
        if (imgui::BeginTooltip()) {
            imgui::ScopedStyle<ImVec2> s(imgui::GetStyle().CellPadding, { 15.f, 6.f });

            if (imgui::BeginTable("help_icon_table", 3, ImGuiTableFlags_SizingFixedFit)) {
                struct tooltip_table_row
                {
                    char const *keybind = nullptr;
                    char const *action = nullptr;
                    char const *description = nullptr;
                };

                tooltip_table_row const table_row_data[] = {
                    { "Ctrl F", "(F)ilter", "Toggle controls to filter (hide/find) entries in the current working directory." },
                    { "Ctrl Shift F", "(F)inder", "Open current working directory in Finder." },
                    { "Ctrl Shift A F", "(A)dd to (F)inder", "Add current working directory to Finder." },
                    { "Ctrl C", "(C)opy selected entries to clipboard", "Add copy operation to the clipboard for each selected entry in the current working directory." },
                    { "Ctrl X", "Cut selected entries to clipboard", "Add move operation to the clipboard for each selected entry in the current working directory." },
                    { "Ctrl V", "Paste", "Execute file operations from the clipboard into the current working directory." },
                    { "Del", "(Del)ete selected entries", "Initiate revertable delete operation for all selected entries in the current working directory, including filtered ones." },
                    { "Escape", "Deselect all entries", "Deselect all entries in the current working directory, including filtered ones." },
                    { "Ctrl A", "Select (a)ll visible entries", "Select all visible entries in the current working directory." },
                    { "Ctrl I", "(I)nvert selection", "Invert selection state of all visible entries in the current working directory." },
                    { "Ctrl R, F2", "(R)ename selected entries", "Open bulk rename modal if multiple entries are selected, or the simpler single rename modal if only one entry is selected." },
                    { "Ctrl N F", "(N)ew (f)ile", "Open modal for creating a new empty file." },
                    { "Ctrl N D", "(N)ew (d)irectory", "Open modal for creating a new empty directory." },
                    { "Ctrl H", "Open (h)istory", "Open history modal where you can view and navigate to previous working directories." },
                    { "Ctrl P", "(P)in current working directory", "Open modal to pin the current working directory, or modify the existing pin." },
                    { "Ctrl O", "(O)pen pins", "Open modal where pinned directories can be accessed." },
                };

                imgui::TableSetupColumn("[Keybind]", {}, {}, 0);
                imgui::TableSetupColumn("[Action]", {}, {}, 1);
                imgui::TableSetupColumn("[Description]", {}, {}, 2);
                imgui::TableHeadersRow();

                for (auto const &table_row : table_row_data) {
                    imgui::TableNextRow();

                    imgui::TableNextColumn();
                    imgui::Text(" %s", table_row.keybind);

                    imgui::TableNextColumn();
                    imgui::Text(" %s", table_row.action);

                    imgui::TableNextColumn();
                    imgui::Text(" %s ", table_row.description);
                }

                imgui::EndTable();
            }

            imgui::Spacing();

            imgui::TextUnformatted("[General navigation]");
            imgui::TextUnformatted(" - Left click an entry to toggle it's selection state (all other entries will be deselected).");
            imgui::TextUnformatted(" - Shift left click an entry to select a range starting from previously selected entry, or the first entry if none have been selected yet.");
            imgui::TextUnformatted(" - Ctrl left click an entry to toggle it's selection state without altering other selections.");
            imgui::TextUnformatted(" - Double left click a file to open it.");
            imgui::TextUnformatted(" - Double left click a directory to enter it.");
            imgui::TextUnformatted(" - Double left click a symlink to open or enter it.");
            imgui::TextUnformatted(" - Right click an entry to open the context menu for selected entries, or the hovered entry if none are selected.");

            imgui::Spacing();

            imgui::TextUnformatted("[Terminology]");
            imgui::TextUnformatted(" - Directory: Known as a folder in the Windows File Explorer. Contains other entries.");
            imgui::TextUnformatted(" - File: Anything that isn't a directory or symlink. Contains data.");
            imgui::TextUnformatted(" - Symlink: Known as a shortcut in the Windows File Explorer. Points to an entry, possibly in a different directory.");
            imgui::TextUnformatted(" - Entry: Generic term for a directory, file, or symlink.");

            imgui::Spacing();

            imgui::EndTooltip();
        }
    }
}

#if 0
enum cwd_mode : s32
{
    cwd_mode_text_input = 0,
    cwd_mode_clicknav,
    cwd_mode_count
};
#endif

struct render_cwd_text_input_result
{
    bool is_hovered;
};

static
render_cwd_text_input_result render_cwd_text_input(explorer_window &expl,
                                                   bool &cwd_exists_after_edit,
                                                   char dir_sep_utf8,
                                                   wchar_t dir_sep_utf16,
                                                   f32 avail_width_subtract_amt,
                                                   bool cwd_exists_before_edit) noexcept
{
    render_cwd_text_input_result retval;

    static swan_path s_cwd_input = {};
    s_cwd_input = expl.cwd;

    bool is_input_text_enter_pressed = false;

    cwd_text_input_callback_user_data user_data = { expl.id, dir_sep_utf16, false, s_cwd_input.data() };

    {
        ImVec4 low_warning = warning_color(); low_warning.w /= 1.5;
        imgui::ScopedColor b(ImGuiCol_Border, low_warning, !path_is_empty(expl.cwd) && !cwd_exists_before_edit);
        imgui::ScopedAvailWidth w(avail_width_subtract_amt);

        auto label = make_str_static<64>("## cwd expl_%d", expl.id);

        is_input_text_enter_pressed = imgui::InputText(label.data(), s_cwd_input.data(), s_cwd_input.size(),
            ImGuiInputTextFlags_CallbackCharFilter|ImGuiInputTextFlags_CallbackEdit|ImGuiInputTextFlags_EnterReturnsTrue,
            cwd_text_input_callback, (void *)&user_data);

        retval.is_hovered = imgui::IsItemHovered();
    }

    if (user_data.edit_occurred) {
        bool path_functionally_diff = !path_loosely_same(expl.cwd, s_cwd_input);

        expl.cwd = path_squish_adjacent_separators(s_cwd_input);
        path_force_separator(expl.cwd, dir_sep_utf8);

        if (path_functionally_diff) {
            auto [cwd_exists, _] = expl.update_cwd_entries(query_filesystem, expl.cwd.data());
            cwd_exists_after_edit = cwd_exists;

            if (cwd_exists_after_edit && !path_is_empty(expl.cwd)) {
                expl.cwd = path_reconstruct_canonically(expl.cwd.data(), dir_sep_utf8);
                expl.push_history_item(expl.cwd);
                expl.set_latest_valid_cwd(expl.cwd); // this may mutate filter
                (void) expl.update_cwd_entries(filter, expl.cwd.data());
            }
        }

        (void) expl.save_to_disk();
    }

    return retval;
}

void swan_windows::render_explorer(explorer_window &expl, bool &open, finder_window &finder, bool any_popups_open) noexcept
{
    ImVec4 window_bg_color = imgui::GetStyle().Colors[ImGuiCol_WindowBg];
    if (expl.highlight) {
        expl.highlight = false;
        imgui::GetStyle().Colors[ImGuiCol_WindowBg] = ImVec4(0.15f, 0.06f, 0.06f, 1);
    }
    SCOPE_EXIT { imgui::GetStyle().Colors[ImGuiCol_WindowBg] = window_bg_color; };

    imgui::SetNextWindowSize({ 1280, 720 }, ImGuiCond_Appearing);

    {
        bool is_explorer_visible = imgui::Begin(expl.name, &open, ImGuiWindowFlags_NoCollapse);
        if (!is_explorer_visible) {
            imgui::End();
            return;
        }
    }

    auto &io = imgui::GetIO();
    auto &style = imgui::GetStyle();
    ImVec2 base_window_pos = imgui::GetCursorScreenPos();
    bool cwd_exists_before_edit = directory_exists(expl.cwd.data());
    bool cwd_exists_after_edit = cwd_exists_before_edit;
    char dir_sep_utf8 = global_state::settings().dir_separator_utf8;
    wchar_t dir_sep_utf16 = global_state::settings().dir_separator_utf16;
    u64 size_unit_multiplier = global_state::settings().size_unit_multiplier;
    bool open_single_rename_popup = false;
    bool open_bulk_rename_popup = false;
    bool window_hovered = imgui::IsWindowHovered(ImGuiFocusedFlags_ChildWindows);
    bool window_focused = imgui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);

    static explorer_window::dirent const *s_dirent_to_be_renamed = nullptr;

    if (window_focused) {
        assert(expl.id >= 0);
        global_state::focused_window_set(swan_windows::id( (s32)swan_windows::id::explorer_0 + expl.id ));
    }

    auto label_child = make_str_static<64>("%s##main_child", expl.name);
    if (imgui::BeginChild(label_child.data(), {}, false)) {

    if (!any_popups_open) {
        // handle most keybinds here. The redundant checks for `window_focused` and `io.KeyCtrl` are intentional,
        // a tiny performance penalty for much easier to read code.

        if (window_focused && imgui::IsKeyPressed(ImGuiKey_Delete)) {
            u64 num_entries_selected = std::count_if(expl.cwd_entries.begin(), expl.cwd_entries.end(),
                                                     [](explorer_window::dirent const &e) noexcept { return e.is_selected; });

            if (num_entries_selected > 0) {
                imgui::OpenConfirmationModalWithCallback(
                    /* confirmation_id  = */ swan_id_confirm_explorer_execute_delete,
                    /* confirmation_msg = */ make_str("Are you sure you want to delete the %zu selected file(s) from Explorer %d?", num_entries_selected, expl.id+1).c_str(),
                    /* on_yes_callback  = */
                    [&]() noexcept {
                        auto result = delete_selected_entries(expl);

                        if (!result.success) {
                            u64 num_failed = std::count(result.error_or_utf8_path.begin(), result.error_or_utf8_path.end(), '\n');
                            std::string action = make_str("Delete %zu items.", num_failed);
                            char const *failed = result.error_or_utf8_path.c_str();
                            swan_popup_modals::open_error(action.c_str(), failed);
                        }
                        (void) global_state::settings().save_to_disk();
                    },
                    /* confirmation_enabled = */ &(global_state::settings().confirm_explorer_delete_via_keybind)
                );
            }
        }
        else if (window_hovered && (imgui::IsKeyPressed(ImGuiKey_F2) || (io.KeyCtrl && imgui::IsKeyPressed(ImGuiKey_R)))) {
            u64 num_entries_selected = std::count_if(expl.cwd_entries.begin(), expl.cwd_entries.end(),
                                                     [](explorer_window::dirent const &e) noexcept { return e.is_selected; });

            if (num_entries_selected == 0) {
                // swan_popup_modals::open_error("Keybind for rename was pressed.", "Nothing is selected.");
            }
            else if (num_entries_selected == 1) {
                auto selected_dirent = std::find_if(expl.cwd_entries.begin(), expl.cwd_entries.end(),
                                                    [](explorer_window::dirent const &e) noexcept { return e.is_selected; });
                open_single_rename_popup = true;
                s_dirent_to_be_renamed = &(*selected_dirent);
            }
            else if (num_entries_selected > 1) {
                open_bulk_rename_popup = true;
            }
        }
        else if (window_hovered && io.KeyCtrl && imgui::IsKeyPressed(ImGuiKey_H)) {
            imgui::OpenPopup("History");
        }
        else if (window_hovered && io.KeyCtrl && imgui::IsKeyDown(ImGuiKey_N) && imgui::IsKeyPressed(ImGuiKey_F)) {
            swan_popup_modals::open_new_file(expl.cwd.data(), expl.id);
        }
        else if (window_hovered && io.KeyCtrl && imgui::IsKeyDown(ImGuiKey_N) && imgui::IsKeyPressed(ImGuiKey_D)) {
            swan_popup_modals::open_new_directory(expl.cwd.data(), expl.id);
        }
        else if (window_hovered && io.KeyCtrl && io.KeyShift && imgui::IsKeyPressed(ImGuiKey_F)) {
            if (cwd_exists_before_edit) {
                if (finder.search_task.active_token.load() == true) {
                    finder.search_task.cancellation_token.store(true);
                }

                if (!imgui::IsKeyDown(ImGuiKey_A)) {
                    finder.search_directories.clear();
                }
                finder.search_directories.push_back({ cwd_exists_before_edit, expl.cwd });

                init_empty_cstr(finder.search_value.data());
                finder.focus_search_value_input = true;

                global_state::settings().show.finder = true;
                (void) global_state::settings().save_to_disk();

                imgui::SetWindowFocus(swan_windows::get_name(swan_windows::id::finder));
            }
        }
        else if (window_hovered && io.KeyCtrl && imgui::IsKeyPressed(ImGuiKey_F)) {
            flip_bool(expl.show_filter_window);
        }
        else if (window_hovered && io.KeyCtrl && imgui::IsKeyPressed(ImGuiKey_P)) {
            u64 pin_idx;
            {
                scoped_timer<timer_unit::MICROSECONDS> check_if_pinned_timer(&expl.check_if_pinned_us);
                pin_idx = global_state::pinned_find_idx(expl.cwd);
            }
            bool already_pinned = pin_idx != std::string::npos;

            if (already_pinned) {
                swan_popup_modals::open_edit_pin(&global_state::pinned_get()[pin_idx]);
            } else {
                swan_popup_modals::open_new_pin(expl.cwd, false);
            }
        }
        else if (window_hovered && io.KeyCtrl && imgui::IsKeyPressed(ImGuiKey_O)) {
            imgui::OpenPopup("## pins");
        }
    }

    if (imgui::BeginPopup("## pins")) {
        render_pins_popup(expl);
        imgui::EndPopup();
    }

    // refresh logic start
    {
        auto refresh = [&](update_cwd_entries_actions actions, std::source_location sloc = std::source_location::current()) noexcept {
            auto [cwd_exists, _] = expl.update_cwd_entries(actions, expl.cwd.data(), sloc);
            cwd_exists_before_edit = cwd_exists_after_edit = cwd_exists;
        };

        if (expl.update_request_from_outside != nil) {
            refresh(expl.update_request_from_outside);
            expl.update_request_from_outside = nil;
        }
        else if (global_state::settings().explorer_refresh_mode != swan_settings::explorer_refresh_mode_manual && cwd_exists_before_edit) {
            auto issue_read_dir_changes = [&]() noexcept {
                wchar_t cwd_utf16[MAX_PATH];

                if (!utf8_to_utf16(expl.cwd.data(), cwd_utf16, lengthof(cwd_utf16))) {
                    expl.refresh_message = ICON_CI_ERROR " Auto refresh disabled";
                    expl.refresh_message_tooltip = "Something went wrong when trying to subscribe to directory changes. Sorry!";
                    return;
                }

                expl.read_dir_changes_handle = CreateFileW(
                    cwd_utf16,
                    FILE_LIST_DIRECTORY,
                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    NULL,
                    OPEN_EXISTING,
                    FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
                    NULL);

                if (expl.read_dir_changes_handle == INVALID_HANDLE_VALUE) {
                    print_debug_msg("[ %d ] CreateFileW FAILED: INVALID_HANDLE_VALUE", expl.id);
                } else {
                    auto success = ReadDirectoryChangesW(
                        expl.read_dir_changes_handle,
                        reinterpret_cast<void *>(expl.read_dir_changes_buffer.data()),
                        (s32)expl.read_dir_changes_buffer.size(),
                        FALSE, // watch subtree
                        FILE_NOTIFY_CHANGE_CREATION|FILE_NOTIFY_CHANGE_DIR_NAME|FILE_NOTIFY_CHANGE_FILE_NAME|FILE_NOTIFY_CHANGE_LAST_WRITE|FILE_NOTIFY_CHANGE_SIZE,
                        &expl.read_dir_changes_buffer_bytes_written,
                        &expl.read_dir_changes_overlapped,
                        nullptr);

                    expl.read_dir_changes_target = expl.cwd;

                    if (success) {
                        print_debug_msg("[ %d ] SUCCESS ReadDirectoryChangesW [%s]", expl.id, expl.cwd.data());
                    } else {
                        print_debug_msg("[ %d ] FAILED ReadDirectoryChangesW: %s", expl.id, get_last_winapi_error().formatted_message.c_str());
                    }
                }
            };

            if (expl.read_dir_changes_refresh_request_time != precise_time_point_t() &&
                compute_diff_ms(expl.last_filesystem_query_time, current_time_precise()) >= 250)
            {
                refresh(full_refresh);
                expl.read_dir_changes_refresh_request_time = precise_time_point_t();
            }
            else if (expl.read_dir_changes_handle != INVALID_HANDLE_VALUE && !path_loosely_same(expl.cwd, expl.read_dir_changes_target)) {
                // cwd changed while waiting for signal from ReadDirectoryChangesW,
                // therefore need to reissue ReadDirectoryChangesW (will be done on next frame)

                //? which one?
                CancelIo(expl.read_dir_changes_handle);
                CloseHandle(expl.read_dir_changes_handle);
                expl.read_dir_changes_handle = INVALID_HANDLE_VALUE;
            }
            else if (expl.read_dir_changes_handle != INVALID_HANDLE_VALUE) {
                BOOL overlap_check = GetOverlappedResult(expl.read_dir_changes_handle,
                                                         &expl.read_dir_changes_overlapped,
                                                         &expl.read_dir_changes_buffer_bytes_written,
                                                         FALSE);

                if (!overlap_check && GetLastError() == ERROR_IO_INCOMPLETE) {
                    // ReadDirectoryChangesW in flight but not yet signalled, thus no changes and no refresh needed
                    // print_debug_msg("[ %d ] GetOverlappedResult FAILED: %d %s", expl.id, GetLastError(), get_last_error_string().c_str());
                } else {
                    if (global_state::settings().explorer_refresh_mode == swan_settings::explorer_refresh_mode_automatic) {
                        print_debug_msg("[ %d ] ReadDirectoryChangesW signalled a change && refresh mode == automatic, refreshing...");
                        issue_read_dir_changes();
                        if (expl.read_dir_changes_refresh_request_time == precise_time_point_t()) {
                            // no refresh pending, submit request to refresh
                            expl.read_dir_changes_refresh_request_time = current_time_precise();
                        }
                    } else { // explorer_options::refresh_mode::notify
                        print_debug_msg("[ %d ] ReadDirectoryChangesW signalled a change && refresh mode == notify, notifying...");

                        expl.refresh_message = ICON_FA_EXCLAMATION_TRIANGLE " Outdated" ;

                        expl.refresh_message_tooltip = "Directory content has changed since it was last updated.\n"
                                                       "To see the changes, click to refresh.\n"
                                                       "Alternatively, you can set refresh mode to 'Automatic' in [Settings] > Explorer > Refresh mode.";
                        issue_read_dir_changes();
                    }
                }
            }
            else { // expl.read_dir_changes_handle == INVALID_HANDLE_VALUE
                issue_read_dir_changes();
            }
        }
    }
    // refresh logic end

    if (global_state::settings().show_debug_info) {
        auto window_name = make_str_static<64>(" Explorer %d Debug Info ", expl.id + 1);
        if (imgui::Begin(window_name.data(), nullptr)) {
            render_debug_info(expl, size_unit_multiplier);
        }
        imgui::End();
    }

    // header controls
    {
        render_more_controls_button(expl, cwd_exists_before_edit, dir_sep_utf8);

        imgui::SameLine(0, style.ItemSpacing.x / 2);

        render_back_to_prev_valid_cwd_button(expl);

        imgui::SameLine(0, style.ItemSpacing.x / 2);

        render_forward_to_next_valid_cwd_button(expl);

        imgui::SameLine(0, style.ItemSpacing.x / 2);

        render_up_to_cwd_parent_button(expl, cwd_exists_before_edit);

        imgui::SameLine();

        // render_history_browser_button();

        // imgui::SameLine();

        f32 avail_width_subtract_amt = imgui::CalcTextSize("(?)").x + imgui::GetStyle().ItemSpacing.x; // for help icon
        render_cwd_text_input(expl, cwd_exists_after_edit, dir_sep_utf8, dir_sep_utf16, avail_width_subtract_amt, cwd_exists_before_edit);

        imgui::SameLine();

        render_help_icon();
    }

#if 0
    if (imgui::BeginPopup("history_popup")) {
        swan_path backup = expl.cwd;
        bool history_item_clicked = render_history_browser_popup(expl, cwd_exists_before_edit);

        if (history_item_clicked) {
            auto [history_item_exists, _] = expl.update_cwd_entries(query_filesystem, expl.cwd.data());
            if (history_item_exists) {
                expl.set_latest_valid_cwd(expl.cwd); // this may mutate filter
                (void) expl.update_cwd_entries(filter, expl.cwd.data());
                (void) expl.save_to_disk();
            } else {
                std::string action = make_str("Navigate to history item [%s]", expl.cwd.data());
                char const *failed = "Path not found, maybe it was renamed or deleted?";
                swan_popup_modals::open_error(action.c_str(), failed);

                expl.cwd = backup;
                (void) expl.update_cwd_entries(full_refresh, expl.cwd.data());
            }
        }
    }
#endif

    cwd_count_info cnt = {};
    {
        for (auto &dirent : expl.cwd_entries) {
            static_assert(u64(false) == 0);
            static_assert(u64(true)  == 1);

            [[maybe_unused]] char const *path = dirent.basic.path.data();

            bool is_path_dotdot = dirent.basic.is_path_dotdot();

            cnt.filtered_directories += u64(dirent.is_filtered_out && dirent.basic.is_directory());
            cnt.filtered_symlinks    += u64(dirent.is_filtered_out && dirent.basic.is_symlink());
            cnt.filtered_files       += u64(dirent.is_filtered_out && dirent.basic.is_file());

            cnt.child_dirents     += 1; // u64(!is_path_dotdot);
            cnt.child_directories += u64(dirent.basic.is_directory());
            cnt.child_symlinks    += u64(dirent.basic.is_symlink());
            cnt.child_files       += u64(dirent.basic.is_file());

            if (!dirent.is_filtered_out && dirent.is_selected) {
                cnt.selected_directories += u64(dirent.is_selected && dirent.basic.is_directory() && !is_path_dotdot);
                cnt.selected_symlinks    += u64(dirent.is_selected && dirent.basic.is_symlink());
                cnt.selected_files       += u64(dirent.is_selected && dirent.basic.is_file());
            }
        }

        cnt.filtered_dirents = cnt.filtered_directories + cnt.filtered_symlinks + cnt.filtered_files;
        cnt.selected_dirents = cnt.selected_directories + cnt.selected_symlinks + cnt.selected_files;
    }

    if (expl.show_filter_window) {
        // imgui::Spacing();

        f32 half_spacing = style.ItemSpacing.x / 2;

        render_filter_reset_button(expl);
        imgui::SameLine(0, half_spacing);
        render_filter_polarity_button(expl);
        imgui::SameLine(0, half_spacing);
        render_filter_case_sensitivity_button(expl);
        imgui::SameLine(0, half_spacing);
        render_filter_mode_toggle(expl);

        imgui::SameLine();

        render_filter_text_input(expl, window_hovered, io.KeyCtrl, cnt);

        imgui::SameLineSpaced(1);
        render_filter_type_toggler_buttons(expl);

        if (expl.filter_error != "") {
            imgui::PushTextWrapPos(imgui::GetColumnWidth());
            imgui::TextColored(error_color(), "%s", expl.filter_error.c_str());
            imgui::PopTextWrapPos();
        }

        imgui::Spacing();
    }

    ImVec2 cwd_entries_table_size = {};
    ImVec2 cwd_entries_table_min = {};
    ImVec2 cwd_entries_table_max = {};
    bool drives_table_rendered = false;

    if (path_is_empty(expl.cwd)) {
        render_drives_table(expl, dir_sep_utf8, size_unit_multiplier);
        drives_table_rendered = true;
    }
    else {


        // distance from top of cwd_entries table to top border of dirent we are trying to scroll to
        std::optional<f32> scrolled_to_dirent_offset_y = std::nullopt;

        if (expl.scroll_to_nth_selected_entry_next_frame != u64(-1)) {
            u64 target = expl.scroll_to_nth_selected_entry_next_frame;
            u64 counter = 0;

            auto scrolled_to_dirent = std::find_if(expl.cwd_entries.begin(), expl.cwd_entries.end(), [&counter, target](explorer_window::dirent &e) noexcept {
                // stop spotlighting the previous dirents, looks better when rapidly advancing the spotlighted dirent.
                // without it multiple dirents can be spotlighted at the same time which is visually distracting and possible confusing.
                e.spotlight_frames_remaining = 0;

                return e.is_selected && (counter++) == target;
            });

            expl.reset_filter();
            expl.show_filter_window = false;

            if (scrolled_to_dirent != expl.cwd_entries.end()) {
                scrolled_to_dirent->spotlight_frames_remaining = u32(imgui::GetIO().Framerate) / 3;
                scrolled_to_dirent_offset_y = ImGui::GetTextLineHeightWithSpacing() * f32(std::distance(expl.cwd_entries.begin(), scrolled_to_dirent));

                // stop spotlighting any dirents ahead which could linger if we just wrapped the spotlight back to the top,
                // looks better this way when rapidly advancing the spotlighted dirent.
                // without it multiple dirents can be spotlighted at the same time which is visually distracting and possibly confusing.
                std::for_each(scrolled_to_dirent + 1, expl.cwd_entries.end(),
                              [](explorer_window::dirent &e) noexcept { e.spotlight_frames_remaining = 0; });
            }

            expl.scroll_to_nth_selected_entry_next_frame = u64(-1);
        }

        if (imgui::BeginChild("cwd_entries_child",
            ImVec2(0, imgui::GetContentRegionAvail().y - imgui::CalcTextSize("items").y - imgui::GetStyle().WindowPadding.y - 5*2)),
            ImGuiWindowFlags_NoNav|ImGuiWindowFlags_NoInputs)
        {
            if (scrolled_to_dirent_offset_y.has_value()) {
                f32 current_scroll_offset_y = imgui::GetScrollY();
                f32 window_height = imgui::GetWindowContentRegionMax().y - imgui::GetWindowContentRegionMin().y;
                f32 visible_region_top_y = current_scroll_offset_y;
                f32 visible_region_bottom_y = current_scroll_offset_y + window_height;
                f32 scroll_dirent_top_y = scrolled_to_dirent_offset_y.value() + imgui::TableGetHeaderRowHeight();
                f32 scroll_dirent_bottom_y = scroll_dirent_top_y + ImGui::GetTextLineHeightWithSpacing();

                if (scroll_dirent_top_y >= visible_region_top_y && scroll_dirent_bottom_y <= visible_region_bottom_y) {
                    // already completely in view, don't scroll to avoid ugliness caused by 1 frame delay in fulfillment of imgui::SetScrollY
                } else {
                    f32 half_window_height = window_height / 2.0f;
                    f32 half_dirent_row_height = ImGui::GetTextLineHeightWithSpacing() / 2.0f;
                    f32 scroll_target_y = scrolled_to_dirent_offset_y.value() + imgui::TableGetHeaderRowHeight() - half_window_height + half_dirent_row_height;
                    imgui::SetScrollY(scroll_target_y); // TODO: doesn't work with ScrollY table flag
                }
            }

            s32 table_flags =
                ImGuiTableFlags_SizingStretchProp|
                ImGuiTableFlags_Hideable|
                ImGuiTableFlags_Resizable|
                ImGuiTableFlags_Reorderable|
                ImGuiTableFlags_Sortable|
                ImGuiTableFlags_BordersV|
                // ImGuiTableFlags_ScrollY| //! breaks scroll_to_nth_selected_entry_next_frame logic
                ImGuiTableFlags_SortMulti|
                // ImGuiTableFlags_SortTristate|
                (global_state::settings().explorer_cwd_entries_table_alt_row_bg ? ImGuiTableFlags_RowBg : 0)|
                (global_state::settings().explorer_cwd_entries_table_borders_in_body ? 0 : ImGuiTableFlags_NoBordersInBody)
            ;

            if (!expl.cwd_entries.empty() && cnt.filtered_dirents < expl.cwd_entries.size() && imgui::BeginTable("cwd_entries", explorer_window::cwd_entries_table_col_count, table_flags)) {
                s32 const col_flags_sortable_prefer_asc = ImGuiTableColumnFlags_DefaultSort|ImGuiTableColumnFlags_PreferSortAscending;
                s32 const col_flags_sortable_prefer_desc = ImGuiTableColumnFlags_DefaultSort|ImGuiTableColumnFlags_PreferSortDescending;

                imgui::TableSetupColumn("#", ImGuiTableColumnFlags_NoSort, 0.0f, explorer_window::cwd_entries_table_col_number);
                imgui::TableSetupColumn("ID", col_flags_sortable_prefer_asc, 0.0f, explorer_window::cwd_entries_table_col_id);
                imgui::TableSetupColumn("Name", col_flags_sortable_prefer_asc|ImGuiTableColumnFlags_NoHide, 0.0f, explorer_window::cwd_entries_table_col_path);
                imgui::TableSetupColumn("Object", col_flags_sortable_prefer_asc, 0.0f, explorer_window::cwd_entries_table_col_object);
                imgui::TableSetupColumn("Type", col_flags_sortable_prefer_asc, 0.0f, explorer_window::cwd_entries_table_col_type);
                imgui::TableSetupColumn("Size", col_flags_sortable_prefer_desc, 0.0f, explorer_window::cwd_entries_table_col_size_pretty);
                imgui::TableSetupColumn("Bytes", col_flags_sortable_prefer_desc, 0.0f, explorer_window::cwd_entries_table_col_size_bytes);
                imgui::TableSetupColumn("Created", col_flags_sortable_prefer_asc, 0.0f, explorer_window::cwd_entries_table_col_creation_time);
                imgui::TableSetupColumn("Modified", col_flags_sortable_prefer_asc, 0.0f, explorer_window::cwd_entries_table_col_last_write_time);
                ImGui::TableSetupScrollFreeze(0, 1);
                imgui::TableHeadersRow();

                //? ImGui does not allow you to hold a ImGuiTableSortSpecs pointer over multiple frames,
                //? so we make a copy of it for ourselves because we would like to use it in later frames.
                //? luckily it's a trivial and compact object so there is little implication making a copy each frame.
                ImGuiTableSortSpecs *table_sort_specs = imgui::TableGetSortSpecs();
                if (table_sort_specs != nullptr) {
                    expl.column_sort_specs = expl.copy_column_sort_specs(table_sort_specs);
                }

                std::vector<explorer_window::dirent>::iterator first_filtered_cwd_dirent;

                if (table_sort_specs != nullptr && table_sort_specs->SpecsDirty) {
                    table_sort_specs->SpecsDirty = false;
                    first_filtered_cwd_dirent = sort_cwd_entries(expl);
                } else {
                    f64 find_first_filtered_cwd_dirent_us = 0;
                    SCOPE_EXIT { expl.find_first_filtered_cwd_dirent_timing_samples.push_back(find_first_filtered_cwd_dirent_us); };
                    scoped_timer<timer_unit::MICROSECONDS> timer(&find_first_filtered_cwd_dirent_us);

                    // no point in binary search here, cost of linear traversal is tiny even for huge collection
                    first_filtered_cwd_dirent = std::find_if(expl.cwd_entries.begin(), expl.cwd_entries.end(),
                                                             [](explorer_window::dirent const &ent) noexcept { return ent.is_filtered_out; });
                }

                // opens "Context" popup if a rendered dirent is right clicked
                auto context_menu_target_name_rect = render_table_rows_for_cwd_entries(expl, cnt, size_unit_multiplier, any_popups_open, dir_sep_utf8, dir_sep_utf16);

                auto result = render_dirent_context_menu(expl, cnt, global_state::settings());
                open_bulk_rename_popup   |= result.open_bulk_rename_popup;
                open_single_rename_popup |= result.open_single_rename_popup;
                if (result.single_dirent_to_be_renamed) {
                    s_dirent_to_be_renamed = result.single_dirent_to_be_renamed;
                }

                if (result.context_menu_rect.has_value() && context_menu_target_name_rect.has_value() && cnt.selected_dirents <= 1) {
                    ImVec2 surrounding_rect_padding = { 5.f, 5.f };
                    context_menu_target_name_rect.value().Min -= surrounding_rect_padding;
                    context_menu_target_name_rect.value().Max += surrounding_rect_padding;

                    // (void) imgui::DrawBestLineBetweenContextMenuAndTarget(context_menu_target_name_rect.value(), result.context_menu_rect.value().GetTL(), ImVec4(255, 0, 0, 200));
                    (void) imgui::DrawBestLineBetweenRectCorners(context_menu_target_name_rect.value(), result.context_menu_rect.value(), ImVec4(255, 255, 255, 100), true, false, 2.f, 2.f);
                }

                imgui::EndTable();
            }

            cwd_entries_table_size = imgui::GetItemRectSize();
            cwd_entries_table_min = imgui::GetItemRectMin();
            cwd_entries_table_max = imgui::GetItemRectMax();
        }

        imgui::ScopedStyle<f32> s2(style.ItemSpacing.y, 0); // to remove spacing between end of table and footer

        imgui::EndChild();

        accept_move_dirents_drag_drop(expl);
    }
    ImVec2 cwd_entries_child_size = imgui::GetItemRectSize();
    ImVec2 cwd_entries_child_min = imgui::GetItemRectMin();
    ImVec2 cwd_entries_child_max = imgui::GetItemRectMax();

    ImVec2 footer_rect_min(cwd_entries_child_min.x, cwd_entries_table_max.y);
    ImVec2 footer_rect_max = cwd_entries_child_max;

    bool mouse_hovering_cwd_entries_child = imgui::IsMouseHoveringRect(cwd_entries_child_min, cwd_entries_child_max);
    bool mouse_hovering_footer = imgui::IsMouseHoveringRect(footer_rect_min, footer_rect_max);

    if (!any_popups_open && (mouse_hovering_cwd_entries_child || mouse_hovering_footer)) {
        auto handle_file_op_failure = [](char const *operation, generic_result const &result) noexcept {
            if (!result.success) {
                u64 num_failed = std::count(result.error_or_utf8_path.begin(), result.error_or_utf8_path.end(), '\n');
                std::string action = make_str("%s %zu items.", operation, num_failed);
                char const *failed = result.error_or_utf8_path.c_str();
                swan_popup_modals::open_error(action.c_str(), failed);
            }
        };

        if (imgui::IsKeyPressed(ImGuiKey_Escape)) {
            expl.deselect_all_cwd_entries();
        }
        else if (io.KeyCtrl && imgui::IsKeyPressed(ImGuiKey_A)) {
            expl.select_all_visible_cwd_entries();
        }
        else if (window_hovered && io.KeyCtrl && imgui::IsKeyPressed(ImGuiKey_X)) {
            g_file_op_payload.clear();
            auto result = add_selected_entries_to_file_op_payload(expl, "Cut", file_operation_type::move);
            handle_file_op_failure("cut", result);
        }
        else if (window_hovered && io.KeyCtrl && imgui::IsKeyPressed(ImGuiKey_C)) {
            g_file_op_payload.clear();
            auto result = add_selected_entries_to_file_op_payload(expl, "Copy", file_operation_type::copy);
            handle_file_op_failure("copy", result);
        }
        else if (window_hovered && io.KeyCtrl && imgui::IsKeyPressed(ImGuiKey_V) && !g_file_op_payload.items.empty()) {
            g_file_op_payload.execute(expl);
        }
        else if (io.KeyCtrl && imgui::IsKeyPressed(ImGuiKey_I)) {
            expl.invert_selection_on_visible_cwd_entries();
        }
    #if 0 // TODO: pick a better key or check that no text inputs are focused
        else if (imgui::IsKeyPressed(ImGuiKey_Backspace)) {
            if (io.KeyCtrl) {
                // go forward in history

                u64 wd_history_last_idx = expl.wd_history.empty() ? 0 : expl.wd_history.size() - 1;

                if (expl.wd_history_pos != wd_history_last_idx) {
                    if (io.KeyShift) {
                        expl.wd_history_pos = wd_history_last_idx;
                    } else {
                        expl.wd_history_pos += 1;
                    }

                    expl.cwd = expl.wd_history[expl.wd_history_pos];
                    auto [forward_dir_exists, _] = expl.update_cwd_entries(query_filesystem, expl.cwd.data());
                    if (forward_dir_exists) {
                        expl.set_latest_valid_cwd(expl.cwd); // this may mutate filter
                        (void) expl.update_cwd_entries(filter, expl.cwd.data());
                    }
                }
            }
            else {
                // go backward in history

                if (expl.wd_history_pos > 0) {
                    if (io.KeyShift) {
                        expl.wd_history_pos = 0;
                    } else {
                        expl.wd_history_pos -= 1;
                    }

                    expl.cwd = expl.wd_history[expl.wd_history_pos];
                    auto [back_dir_exists, _] = expl.update_cwd_entries(query_filesystem, expl.cwd.data());
                    if (back_dir_exists) {
                        expl.set_latest_valid_cwd(expl.cwd); // this may mutate filter
                        (void) expl.update_cwd_entries(filter, expl.cwd.data());
                        (void) expl.save_to_disk();
                    }
                }
            }
        }
    #endif
    }
    if (cwd_exists_after_edit && imgui::IsMouseClicked(ImGuiMouseButton_Right) && mouse_hovering_footer) {
        imgui::OpenPopup("## cwd_entries_child_leftovers context_menu");
    }
    if (imgui::BeginPopup("## cwd_entries_child_leftovers context_menu")) {
        if (!g_file_op_payload.items.empty() && !path_is_empty(expl.cwd) && imgui::Selectable("Paste")) {
            auto result = g_file_op_payload.execute(expl);
            // TODO: why is result unused?
        }

        if (imgui::Selectable("New file")) {
            swan_popup_modals::open_new_file(expl.cwd.data(), expl.id);
        }
        if (imgui::Selectable("New directory")) {
            swan_popup_modals::open_new_directory(expl.cwd.data(), expl.id);
        }

        imgui::EndPopup();
    }

    if (!drives_table_rendered) {
        render_footer(expl, cnt, style, cwd_exists_after_edit);

        if (!path_is_empty(expl.cwd) && cwd_exists_after_edit && !g_file_op_payload.items.empty()) {
            imgui::OpenPopupOnItemClick("## explorer_footer_context", ImGuiPopupFlags_MouseButtonRight);

            if (imgui::BeginPopup("## explorer_footer_context")) {
                if (imgui::Selectable("Paste")) {
                    auto result = g_file_op_payload.execute(expl);
                    // TODO: why is result unused?
                }
                imgui::EndPopup();
            }
        }
    }

    if (open_single_rename_popup) {
        swan_popup_modals::open_single_rename(expl, *s_dirent_to_be_renamed, [&expl]() noexcept {
            /* on rename finished: */
            if (global_state::settings().explorer_refresh_mode == swan_settings::explorer_refresh_mode_manual) {
                (void) expl.update_cwd_entries(full_refresh, expl.cwd.data());
            }
        });
    }
    if (open_bulk_rename_popup) {
        swan_popup_modals::open_bulk_rename(expl, [&]() noexcept {
            /* on rename finished: */
            if (global_state::settings().explorer_refresh_mode == swan_settings::explorer_refresh_mode_manual) {
                (void) expl.update_cwd_entries(full_refresh, expl.cwd.data());
            }
        });
    }

    imgui::SetNextWindowPos(base_window_pos, ImGuiCond_Appearing);
    imgui::SetNextWindowSize(imgui::GetContentRegionAvail(), ImGuiCond_Appearing);

    if (imgui::BeginPopupModal("History", nullptr, ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize)) {
        swan_path backup = expl.cwd;
        bool history_item_clicked = render_history_browser_popup(expl, cwd_exists_after_edit, false);

        if (history_item_clicked) {
            auto [history_item_exists, _] = expl.update_cwd_entries(query_filesystem, expl.cwd.data());
            if (history_item_exists) {
                expl.set_latest_valid_cwd(expl.cwd); // this may mutate filter
                (void) expl.update_cwd_entries(filter, expl.cwd.data());
                (void) expl.save_to_disk();
            } else {
                std::string action = make_str("Navigate to history item [%s]", expl.cwd.data());
                char const *failed = "Path not found, maybe it was renamed or deleted?";
                swan_popup_modals::open_error(action.c_str(), failed);

                expl.cwd = backup;
                (void) expl.update_cwd_entries(full_refresh, expl.cwd.data());
            }
        }
    }

    }
    imgui::EndChild();

    if (imgui::BeginDragDropTarget()) {
        auto payload_wrapper = imgui::AcceptDragDropPayload(typeid(pin_drag_drop_payload).name(), 0, 0, 0, ImVec2(style.WindowPadding.x, style.WindowPadding.y));

        if (payload_wrapper != nullptr) {
            assert(payload_wrapper->DataSize == sizeof(pin_drag_drop_payload));
            auto payload_data = (pin_drag_drop_payload *)payload_wrapper->Data;
            auto const &pin = global_state::pinned_get()[payload_data->pin_idx];

            swan_path initial_cwd = expl.cwd;
            expl.cwd = path_create("");

            auto result = try_descend_to_directory(expl, pin.path.data());

            if (!result.success) {
                std::string action = make_str("Open pin [%s] in Explorer %d.", pin.path.data(), expl.id+1);
                char const *failed = result.err_msg.c_str();
                swan_popup_modals::open_error(action.c_str(), failed);

                expl.cwd = initial_cwd;
                (void) expl.update_cwd_entries(full_refresh, expl.cwd.data()); // restore entries cleared by try_descend_to_directory
            }
        }

        imgui::EndDragDropTarget();
    }

    imgui::End();
}

void file_operation_command_buf::clear() noexcept
{
    g_file_op_payload.items.clear();

    auto &all_explorers = global_state::explorers();
    for (auto &expl : all_explorers) {
        expl.uncut();
    }
}

generic_result file_operation_command_buf::execute(explorer_window &expl) noexcept
{
    wchar_t cwd_utf16[2048]; init_empty_cstr(cwd_utf16);

    if (!utf8_to_utf16(expl.cwd.data(), cwd_utf16, lengthof(cwd_utf16))) {
        return { false, "Conversion of current working directory path from UTF-8 to UTF-16." };
    }

    std::wstring packed_paths_to_exec_utf16 = {};
    std::vector<file_operation_type> operations_to_exec = {};
    {
        wchar_t item_utf16[MAX_PATH];
        std::stringstream err = {};

        operations_to_exec.reserve(this->items.size());

        for (auto const &item : this->items) {
            init_empty_cstr(item_utf16);

            if (!utf8_to_utf16(item.path.data(), item_utf16, lengthof(item_utf16))) {
                err << "Conversion of [" << item.path.data() << "] from UTF-8 to UTF-16.\n";
            } else {
                packed_paths_to_exec_utf16.append(item_utf16).append(L"\n");
                operations_to_exec.push_back(item.operation_type);
            }
        }

        // WCOUT_IF_DEBUG("packed_paths_to_exec_utf16:\n" << packed_paths_to_exec_utf16 << '\n');

        std::string errors = err.str();
        if (!errors.empty()) {
            return { false, errors };
        }
    }

    bool initialization_done = false;
    std::string initialization_error = {};

    global_state::thread_pool().push_task(perform_file_operations,
        expl.id,
        cwd_utf16,
        std::move(packed_paths_to_exec_utf16),
        std::move(operations_to_exec),
        &expl.shlwapi_task_initialization_mutex,
        &expl.shlwapi_task_initialization_cond,
        &initialization_done,
        &initialization_error);

    {
        std::unique_lock lock(expl.shlwapi_task_initialization_mutex);
        expl.shlwapi_task_initialization_cond.wait(lock, [&]() noexcept { return initialization_done; });
    }

    if (initialization_error.empty()) {
        this->items.clear();
    }

    return { initialization_error.empty(), initialization_error };
}

static
std::optional<ImRect> render_table_rows_for_cwd_entries(
    explorer_window &expl,
    cwd_count_info const &cnt,
    u64 size_unit_multiplier,
    bool any_popups_open,
    char dir_sep_utf8,
    wchar_t dir_sep_utf16) noexcept
{
    std::optional<ImRect> context_menu_target_name_rect = std::nullopt;

    auto io = imgui::GetIO();

    ImGuiListClipper clipper;
    {
        u64 num_dirents_to_render = expl.cwd_entries.size() - cnt.filtered_dirents;
        assert(num_dirents_to_render <= (u64)INT32_MAX);
        clipper.Begin(s32(num_dirents_to_render));
    }

    while (clipper.Step()) {
        for (u64 i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
            auto &dirent = expl.cwd_entries[i];
            [[maybe_unused]] char *path = dirent.basic.path.data();

            imgui::TableNextRow();

            if (imgui::TableSetColumnIndex(explorer_window::cwd_entries_table_col_number)) {
                auto number = make_str_static<32>("%zu", i + 1);
                imgui::TextUnformatted(number.data());
                imgui::RenderTooltipWhenColumnTextTruncated(explorer_window::cwd_entries_table_col_number, number.data());
            }

            if (imgui::TableSetColumnIndex(explorer_window::cwd_entries_table_col_id)) {
                auto id = make_str_static<32>("%zu", dirent.basic.id);
                imgui::TextUnformatted(id.data());
                imgui::RenderTooltipWhenColumnTextTruncated(explorer_window::cwd_entries_table_col_id, id.data());
            }

            if (imgui::TableSetColumnIndex(explorer_window::cwd_entries_table_col_path)) {
                char const *icon = nullptr;
                if (dirent.basic.is_file() && global_state::settings().explorer_file_extension_icons) {
                    file_name_extension_splitter splitter(path);
                    icon = get_icon_for_extension(splitter.ext);
                } else {
                    icon = dirent.basic.kind_icon();
                }

                // render colored icon
                {
                    ImVec4 color = dirent.spotlight_frames_remaining > 0 ? error_color() : get_color(dirent.basic.type);
                    f32 &alpha = color.w;
                    alpha = 1.0f - (f32(dirent.is_cut) * 0.75f);
                    imgui::TextColored(color, icon);
                }

                imgui::SameLine();

                ImVec2 path_text_rect_min = imgui::GetCursorScreenPos();

                ImVec2 name_TL = imgui::GetCursorScreenPos();
                ImVec2 name_BR = {};

                {
                    imgui::ScopedTextColor tc_spotlight(dirent.spotlight_frames_remaining > 0 ? error_color() : imgui::GetStyle().Colors[ImGuiCol_Text]);
                    // imgui::ScopedStyle<ImVec4> tc_context(imgui::GetStyle().Colors[ImGuiCol_Text], error_color(), dirent.context_menu_active);

                    auto label = make_str_static<1200>("%s##dirent%zu", path, i);
                    if (imgui::Selectable(label.data(), dirent.is_selected, ImGuiSelectableFlags_SpanAllColumns|ImGuiSelectableFlags_AllowDoubleClick)) {
                        bool selection_before_deselect = dirent.is_selected;

                        u64 num_deselected = 0;
                        if (!io.KeyCtrl && !io.KeyShift) {
                            // entry was selected but Ctrl was not held, so deselect everything
                            num_deselected = expl.deselect_all_cwd_entries(); // this will alter dirent.is_selected
                        }

                        if (num_deselected > 1) {
                            dirent.is_selected = true;
                        } else {
                            dirent.is_selected = !selection_before_deselect;
                        }

                        if (io.KeyShift) {
                            u64 old = expl.cwd_latest_selected_dirent_idx;
                            auto [first_idx, last_idx] = imgui::SelectRange(expl.cwd_latest_selected_dirent_idx, i);
                            expl.cwd_latest_selected_dirent_idx = last_idx;
                            if (old == u64(-1) && expl.cwd_latest_selected_dirent_idx != old) {
                                expl.cwd_latest_selected_dirent_idx_changed = true;
                            }

                            // print_debug_msg("[ %d ] shift click, [%zu, %zu]", expl.id, first_idx, last_idx);

                            for (u64 j = first_idx; j <= last_idx; ++j) {
                                auto &dirent_ = expl.cwd_entries[j];
                                if (!dirent_.basic.is_path_dotdot()) {
                                    dirent_.is_selected = true;
                                }
                            }
                        }
                        else { // not shift click, check for double click

                            static swan_path s_last_click_path = {};
                            swan_path const &current_click_path = dirent.basic.path;

                            if (imgui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && !io.KeyCtrl && path_equals_exactly(current_click_path, s_last_click_path)) {
                                if (dirent.basic.is_directory()) {
                                    print_debug_msg("[ %d ] double clicked directory [%s]", expl.id, dirent.basic.path.data());

                                    if (dirent.basic.is_path_dotdot()) {
                                        auto result = try_ascend_directory(expl);

                                        if (!result.success) {
                                            std::string action = make_str("Ascend to directory [%s].", result.parent_dir.data());
                                            char const *failed = "Directory not found.";
                                            swan_popup_modals::open_error(action.c_str(), failed);

                                            (void) expl.update_cwd_entries(full_refresh, expl.cwd.data()); // restore entries cleared by try_ascend_directory
                                        }

                                        return std::nullopt;
                                    }
                                    else {
                                        char const *target = dirent.basic.path.data();
                                        auto result = try_descend_to_directory(expl, target);

                                        if (!result.success) {
                                            std::string action = make_str("Descend to directory [%s].", target);
                                            char const *failed = result.err_msg.c_str();
                                            swan_popup_modals::open_error(action.c_str(), failed);

                                            (void) expl.update_cwd_entries(full_refresh, expl.cwd.data()); // restore entries cleared by try_descend_to_directory
                                        }

                                        return std::nullopt;
                                    }
                                }
                                else if (dirent.basic.is_symlink()) {
                                    print_debug_msg("[ %d ] double clicked symlink [%s]", expl.id, dirent.basic.path.data());

                                    auto res = open_symlink(dirent, expl);

                                    if (res.success) {
                                        if (dirent.basic.is_symlink_to_directory()) {
                                            char const *target_dir_path = res.error_or_utf8_path.c_str();

                                            expl.cwd = path_create(target_dir_path);
                                            path_force_separator(expl.cwd, dir_sep_utf8);

                                            expl.set_latest_valid_cwd(expl.cwd); // this may mutate filter
                                            expl.push_history_item(expl.cwd);
                                            (void) expl.update_cwd_entries(full_refresh, expl.cwd.data());
                                            (void) expl.save_to_disk();
                                        }
                                        else if (dirent.basic.is_symlink_to_file()) {
                                            char const *full_file_path = res.error_or_utf8_path.c_str();
                                            u64 recent_file_idx = global_state::recent_files_find_idx(full_file_path);

                                            if (recent_file_idx == u64(-1)) {
                                                global_state::recent_files_add("Opened", full_file_path);
                                            }
                                            else { // already in recent
                                                global_state::recent_files_move_to_front(recent_file_idx, "Opened");
                                            }
                                            (void) global_state::recent_files_save_to_disk();
                                        }
                                    } else {
                                        std::string action = make_str("Open symlink [%s].", dirent.basic.path.data());
                                        char const *failed = res.error_or_utf8_path.c_str();
                                        swan_popup_modals::open_error(action.c_str(), failed);
                                    }
                                }
                                else {
                                    print_debug_msg("[ %d ] double clicked file [%s]", expl.id, dirent.basic.path.data());

                                    auto res = open_file(dirent.basic.path.data(), expl.cwd.data());

                                    if (res.success) {
                                        char const *full_file_path = res.error_or_utf8_path.c_str();
                                        u64 recent_file_idx = global_state::recent_files_find_idx(full_file_path);

                                        if (recent_file_idx == u64(-1)) {
                                            global_state::recent_files_add("Opened", full_file_path);
                                        }
                                        else { // already in recent
                                            global_state::recent_files_move_to_front(recent_file_idx, "Opened");
                                        }
                                        (void) global_state::recent_files_save_to_disk();
                                    } else {
                                        std::string action = make_str("Open file [%s].", dirent.basic.path.data());
                                        char const *failed = res.error_or_utf8_path.c_str();
                                        swan_popup_modals::open_error(action.c_str(), failed);
                                    }
                                }
                            }
                            else if (dirent.basic.is_path_dotdot()) {
                                print_debug_msg("[ %d ] selected [%s]", expl.id, dirent.basic.path.data());
                            }

                            s_last_click_path = current_click_path;
                            expl.cwd_latest_selected_dirent_idx = i;
                            expl.cwd_latest_selected_dirent_idx_changed = true;
                        }

                    } // imgui::Selectable

                    name_BR = name_TL + imgui::CalcTextSize(dirent.basic.path.data());

                    {
                        f32 offset_for_icon_and_space = imgui::CalcTextSize(icon).x + imgui::CalcTextSize(" ").x;
                        imgui::RenderTooltipWhenColumnTextTruncated(explorer_window::cwd_entries_table_col_path, path, offset_for_icon_and_space);
                    }
                }

                if (dirent.highlight_len > 0) {
                    imgui::HighlightTextRegion(path_text_rect_min, dirent.basic.path.data(), dirent.highlight_start_idx, dirent.highlight_len, ImVec4(255, 100, 0, 60));
                }

                if (dirent.basic.is_path_dotdot()) {
                    dirent.is_selected = false; // do no allow [..] to be selected
                }

                if (imgui::IsItemClicked(ImGuiMouseButton_Right) && !any_popups_open && !dirent.basic.is_path_dotdot()) {
                    print_debug_msg("[ %d ] right clicked [%s]", expl.id, dirent.basic.path.data());
                    imgui::OpenPopup("## explorer context_menu");
                    expl.context_menu_target = &dirent;
                    dirent.context_menu_active = true;
                }

                if (dirent.context_menu_active) {
                    context_menu_target_name_rect.emplace(name_TL, name_BR);
                }

            } // path column

            if (!dirent.basic.is_path_dotdot() && imgui::BeginDragDropSource()) {
                auto cwd_to_utf16 = [&]() noexcept {
                    std::wstring retval;

                    wchar_t cwd_utf16[MAX_PATH];

                    if (!utf8_to_utf16(expl.cwd.data(), cwd_utf16, lengthof(cwd_utf16))) {
                        retval = {};
                    } else {
                        retval = cwd_utf16;
                    }

                    return retval;
                };

                auto add_payload_item = [&](
                    move_dirents_drag_drop_payload &payload,
                    std::wstring &paths,
                    std::wstring const &cwd_utf16,
                    char const *item_utf8) noexcept
                {
                    ++payload.num_items;

                    paths += cwd_utf16;
                    if (paths.back() != dir_sep_utf16) {
                        paths += dir_sep_utf16;
                    }

                    wchar_t item_utf16[MAX_PATH];

                    if (utf8_to_utf16(item_utf8, item_utf16, lengthof(item_utf16))) {
                        paths += item_utf16;
                        paths += L'\n';
                    }
                };

                auto compute_and_submit_drag_drop_payload = [&]() noexcept {
                    std::wstring cwd_utf16 = cwd_to_utf16();
                    assert(!cwd_utf16.empty());
                    move_dirents_drag_drop_payload payload = {};
                    std::wstring paths = {};

                    payload.src_explorer_id = expl.id;

                    if (dirent.is_selected) {
                        for (auto const &dirent_ : expl.cwd_entries) {
                            if (!dirent_.is_filtered_out && dirent_.is_selected) {
                                add_payload_item(payload, paths, cwd_utf16, dirent_.basic.path.data());
                            }
                        }
                    }
                    else {
                        add_payload_item(payload, paths, cwd_utf16, dirent.basic.path.data());
                        global_state::move_dirents_payload_set() = true;
                    }

                    wchar_t *paths_data = new wchar_t[paths.size() + 1];
                    StrCpyNW(paths_data, paths.c_str(), s32(paths.size() + 1));
                    payload.absolute_paths_delimited_by_newlines = paths_data;

                    imgui::SetDragDropPayload("move_dirents_drag_drop_payload", (void *)&payload, sizeof(payload), ImGuiCond_Once);

                    global_state::move_dirents_payload_set() = true;
                    // WCOUT_IF_DEBUG("payload.src_explorer_id = " << payload.src_explorer_id << '\n');
                    // WCOUT_IF_DEBUG("payload.absolute_paths_delimited_by_newlines:\n" << payload.absolute_paths_delimited_by_newlines << '\n');
                };

                if (!global_state::move_dirents_payload_set()) {
                    compute_and_submit_drag_drop_payload();
                } else {
                    // don't recompute payload until someone accepts or it gets dropped
                }

                auto payload_wrapper = imgui::GetDragDropPayload();
                if (payload_wrapper != nullptr && streq(payload_wrapper->DataType, "move_dirents_drag_drop_payload")) {
                    auto payload_data = reinterpret_cast<move_dirents_drag_drop_payload *>(payload_wrapper->Data);
                    u64 num_items = payload_data->num_items;
                    imgui::Text("Move %zu item%s", num_items, num_items == 1 ? "" : "s");
                }

                imgui::EndDragDropSource();
            }

            if (!dirent.basic.is_file() && imgui::BeginDragDropTarget()) {
                auto payload_wrapper = imgui::GetDragDropPayload();

                if (payload_wrapper != nullptr && streq(payload_wrapper->DataType, "move_dirents_drag_drop_payload")) {
                    payload_wrapper = imgui::AcceptDragDropPayload("move_dirents_drag_drop_payload");
                    if (payload_wrapper != nullptr) {
                        handle_drag_drop_onto_dirent(expl, dirent, payload_wrapper, dir_sep_utf8);
                    }
                }

                imgui::EndDragDropTarget();
            }

            // free memory if the payload was not accepted and the user dropped it
            if (imgui::IsMouseReleased(ImGuiMouseButton_Left) && !imgui::IsDragDropPayloadBeingAccepted()) {
                global_state::move_dirents_payload_set() = false;

                auto payload_wrapper = imgui::GetDragDropPayload();

                if (payload_wrapper != nullptr && streq(payload_wrapper->DataType, "move_dirents_drag_drop_payload")) {
                    payload_wrapper = imgui::AcceptDragDropPayload("move_dirents_drag_drop_payload");
                    if (payload_wrapper != nullptr) {
                        auto payload_data = reinterpret_cast<move_dirents_drag_drop_payload *>(payload_wrapper->Data);
                        assert(payload_data != nullptr);
                        delete[] payload_data->absolute_paths_delimited_by_newlines;
                    }
                }
            }

            if (imgui::TableSetColumnIndex(explorer_window::cwd_entries_table_col_object)) {
                char const *object_desc = dirent.basic.kind_short_cstr();
                imgui::TextUnformatted(object_desc);
                imgui::RenderTooltipWhenColumnTextTruncated(explorer_window::cwd_entries_table_col_object, object_desc);
            }

            if (imgui::TableSetColumnIndex(explorer_window::cwd_entries_table_col_type)) {
                std::array<char, 64> type_text;
                if (dirent.basic.is_directory()) {
                    type_text = { "Directory" };
                } else {
                    if (std::strchr(dirent.basic.path.data(), '.')) {
                        char const *extension = cget_file_ext(dirent.basic.path.data());
                        type_text = get_type_text_for_extension(extension);
                    } else {
                        type_text = { "File" };
                    }
                }
                imgui::TextUnformatted(type_text.data());
                imgui::RenderTooltipWhenColumnTextTruncated(explorer_window::cwd_entries_table_col_type, type_text.data());
            }

            if (imgui::TableSetColumnIndex(explorer_window::cwd_entries_table_col_size_pretty)) {
                if (!dirent.basic.is_directory()) {
                    std::array<char, 32> pretty_size_text = {};
                    format_file_size(dirent.basic.size, pretty_size_text.data(), pretty_size_text.size(), size_unit_multiplier);
                    imgui::TextUnformatted(pretty_size_text.data());
                    imgui::RenderTooltipWhenColumnTextTruncated(explorer_window::cwd_entries_table_col_size_pretty, pretty_size_text.data());
                }
            }

            if (imgui::TableSetColumnIndex(explorer_window::cwd_entries_table_col_size_bytes)) {
                if (!dirent.basic.is_directory()) {
                    auto size_text = make_str_static<32>("%zu", dirent.basic.size);
                    imgui::TextUnformatted(size_text.data());
                    imgui::RenderTooltipWhenColumnTextTruncated(explorer_window::cwd_entries_table_col_size_bytes, size_text.data());
                }
            }

            if (imgui::TableSetColumnIndex(explorer_window::cwd_entries_table_col_creation_time)) {
                auto [result, buffer] = filetime_to_string(&dirent.basic.creation_time_raw);
                imgui::TextUnformatted(buffer.data());
                imgui::RenderTooltipWhenColumnTextTruncated(explorer_window::cwd_entries_table_col_creation_time, buffer.data());
            }

            if (imgui::TableSetColumnIndex(explorer_window::cwd_entries_table_col_last_write_time)) {
                auto [result, buffer] = filetime_to_string(&dirent.basic.last_write_time_raw);
                imgui::TextUnformatted(buffer.data());
                imgui::RenderTooltipWhenColumnTextTruncated(explorer_window::cwd_entries_table_col_last_write_time, buffer.data());
            }

            dirent.spotlight_frames_remaining -= 1 * (dirent.spotlight_frames_remaining > 0);
        }
    }

    return context_menu_target_name_rect;
}

static
render_dirent_context_menu_result
render_dirent_context_menu(explorer_window &expl, cwd_count_info const &cnt, swan_settings const &settings) noexcept
{
    auto io = imgui::GetIO();
    render_dirent_context_menu_result retval = {};

    if (imgui::BeginPopup("## explorer context_menu")) {
        if (cnt.selected_dirents <= 1) {
            assert(expl.context_menu_target != nullptr);

            // imgui::TextColored(get_color(expl.context_menu_target->basic.type), "%s", expl.context_menu_target->basic.path.data());
            // imgui::Separator();

            // bool is_directory = expl.context_menu_target->basic.is_directory();

            if ((path_ends_with(expl.context_menu_target->basic.path, ".exe") || path_ends_with(expl.context_menu_target->basic.path, ".bat"))
                && imgui::Selectable("Run as administrator"))
            {
                auto res = open_file(expl.context_menu_target->basic.path.data(), expl.cwd.data(), true);

                if (res.success) {
                    char const *full_file_path = res.error_or_utf8_path.c_str();
                    u64 recent_file_idx = global_state::recent_files_find_idx(full_file_path);

                    if (recent_file_idx == u64(-1)) {
                        global_state::recent_files_add("Opened", full_file_path);
                    }
                    else { // already in recent
                        global_state::recent_files_move_to_front(recent_file_idx, "Opened");
                    }
                    (void) global_state::recent_files_save_to_disk();
                } else {
                    std::string action = make_str("Open file as administrator [%s].", expl.context_menu_target->basic.path.data());
                    char const *failed = res.error_or_utf8_path.c_str();
                    swan_popup_modals::open_error(action.c_str(), failed);
                }
            }

            if (expl.context_menu_target->basic.is_symlink_to_file() && imgui::Selectable("Open file location")) {
                symlink_data lnk_data = {};
                auto extract_result = lnk_data.extract(expl.context_menu_target->basic.path.data(), expl.cwd.data());

                if (!extract_result.success) {
                    std::string action = make_str("Open file location of [%s].", expl.context_menu_target->basic.path.data());
                    char const *failed = extract_result.error_or_utf8_path.c_str();
                    swan_popup_modals::open_error(action.c_str(), failed);
                } else {
                    // no error checking because symlink_data::extract has already validated things

                    std::string_view parent_dir = get_everything_minus_file_name(lnk_data.target_path_utf8.data());
                    expl.cwd = path_create(parent_dir.data(), parent_dir.size());

                    swan_path select_name_utf8 = path_create(get_file_name(lnk_data.target_path_utf8.data()));
                    {
                        std::scoped_lock lock(expl.select_cwd_entries_on_next_update_mutex);
                        expl.select_cwd_entries_on_next_update.clear();
                        expl.select_cwd_entries_on_next_update.push_back(select_name_utf8);
                    }

                    expl.push_history_item(expl.cwd);
                    (void) expl.update_cwd_entries(full_refresh, expl.cwd.data());
                    (void) expl.save_to_disk();

                    expl.scroll_to_nth_selected_entry_next_frame = 0;
                }
            }

            {
                static progressive_task<std::optional<generic_result>> task = {};

                if (imgui::Selectable("Open with...")) {
                    global_state::thread_pool().push_task([&expl]() {
                        task.active_token.store(true);

                        auto res = open_file_with(expl.context_menu_target->basic.path.data(), expl.cwd.data());

                        std::scoped_lock lock(task.result_mutex);
                        task.result = res;
                    });
                }

                if (task.active_token.load() == true) {
                    std::scoped_lock lock(task.result_mutex);

                    if (task.result.has_value()) {
                        task.active_token.store(false);
                        auto const &res = task.result.value();
                        bool failed = !res.success && res.error_or_utf8_path != "The operation was canceled by the user.";

                        if (failed) {
                            std::string action = make_str("Open [%s] with...", expl.context_menu_target->basic.path.data());
                            std::string const &failure = res.error_or_utf8_path;
                            swan_popup_modals::open_error(action.c_str(), failure.c_str());
                        }
                    }
                }
            }

            if (imgui::Selectable("Copy name")) {
                imgui::SetClipboardText(expl.context_menu_target->basic.path.data());
            }
            if (imgui::Selectable("Copy full path")) {
                swan_path full_path = path_create(expl.cwd.data());
                if (!path_append(full_path, expl.context_menu_target->basic.path.data(), settings.dir_separator_utf8, true)) {
                    std::string action = make_str("Copy full path of [%s].", expl.context_menu_target->basic.path.data());
                    char const *failed = "Max path length exceeded when appending name to current working directory path.";
                    swan_popup_modals::open_error(action.c_str(), failed);
                } else {
                    imgui::SetClipboardText(full_path.data());
                }
            }
            if (imgui::Selectable("Copy size (bytes)")) {
                imgui::SetClipboardText(std::to_string(expl.context_menu_target->basic.size).c_str());
            }
            if (imgui::Selectable("Copy size (pretty)")) {
                char buffer[32]; init_empty_cstr(buffer);
                format_file_size(expl.context_menu_target->basic.size, buffer, lengthof(buffer), settings.size_unit_multiplier);
                imgui::SetClipboardText(buffer);
            }

            if (imgui::Selectable("Reveal in File Explorer")) {
                swan_path full_path = expl.cwd;
                if (!path_append(full_path, expl.context_menu_target->basic.path.data(), L'\\', true)) {
                    std::string action = make_str("Reveal [%s] in File Explorer.", expl.context_menu_target->basic.path.data());
                    char const *failed = "Append name to current working directory exceeds maximum path length.";
                    swan_popup_modals::open_error(action.c_str(), failed);
                }
                else {
                    auto res = reveal_in_windows_file_explorer(full_path);
                    if (!res.success) {
                        std::string action = make_str("Reveal [%s] in File Explorer.", expl.context_menu_target->basic.path.data());
                        char const *failed = res.error_or_utf8_path.c_str();
                        swan_popup_modals::open_error(action.c_str(), failed);
                    }
                }
            }

            imgui::Separator();

            auto handle_failure = [&](char const *operation, generic_result const &result) noexcept {
                if (!result.success) {
                    std::string action = make_str("%s [%s].", operation, expl.context_menu_target->basic.path.data());
                    char const *failed = result.error_or_utf8_path.c_str();
                    swan_popup_modals::open_error(action.c_str(), failed);
                }
            };

            if (imgui::Selectable("Cut##single")) {
                if (!io.KeyShift) {
                    g_file_op_payload.clear();
                }
                expl.deselect_all_cwd_entries();
                expl.context_menu_target->is_selected = true;
                auto result = add_selected_entries_to_file_op_payload(expl, "Cut", file_operation_type::move);
                handle_failure("cut", result);
            }
            if (imgui::Selectable("Copy##single")) {
                if (!io.KeyShift) {
                    g_file_op_payload.clear();
                }

                expl.deselect_all_cwd_entries();
                expl.context_menu_target->is_selected = true;

                auto result = add_selected_entries_to_file_op_payload(expl, "Copy", file_operation_type::copy);
                handle_failure("copy", result);
            }

            imgui::Separator();

            if (imgui::Selectable("Delete##single")) {
                expl.deselect_all_cwd_entries();
                expl.context_menu_target->is_selected = true;

                imgui::OpenConfirmationModalWithCallback(
                    /* confirmation_id  = */ swan_id_confirm_explorer_execute_delete,
                    /* confirmation_msg = */ make_str("Are you sure you want to delete the %zu selected file(s) from Explorer %d?", 1, expl.id+1).c_str(),
                    /* on_yes_callback  = */
                    [&]() noexcept {
                        auto result = delete_selected_entries(expl);

                        if (!result.success) {
                            u64 num_failed = std::count(result.error_or_utf8_path.begin(), result.error_or_utf8_path.end(), '\n');
                            std::string action = make_str("Delete %zu items.", num_failed);
                            char const *failed = result.error_or_utf8_path.c_str();
                            swan_popup_modals::open_error(action.c_str(), failed);
                        }
                        (void) global_state::settings().save_to_disk();
                    },
                    /* confirmation_enabled = */ &(global_state::settings().confirm_explorer_delete_via_context_menu)
                );
            }
            if (imgui::Selectable("Rename##single")) {
                retval.open_single_rename_popup = true;
                retval.single_dirent_to_be_renamed = expl.context_menu_target;
            }
        }
        else { // right click when > 1 dirents selected
            if (imgui::Selectable("Copy names")) {
                std::string clipboard = {};

                for (auto const &dirent : expl.cwd_entries) {
                    if (dirent.is_selected && !dirent.is_filtered_out) {
                        clipboard += dirent.basic.path.data();
                        clipboard += '\n';
                    }
                }

                imgui::SetClipboardText(clipboard.c_str());
            }
            if (imgui::Selectable("Copy full paths")) {
                std::string clipboard = {};

                for (auto const &dirent : expl.cwd_entries) {
                    if (dirent.is_selected && !dirent.is_filtered_out) {
                        swan_path full_path = path_create(expl.cwd.data());
                        if (!path_append(full_path, dirent.basic.path.data(), settings.dir_separator_utf8, true)) {
                            std::string action = make_str("Copy full path of [%s].", dirent.basic.path.data());
                            char const *failed = "Max path length exceeded when appending name to current working directory path.";
                            swan_popup_modals::open_error(action.c_str(), failed);
                            break;
                        }
                        clipboard += full_path.data();
                        clipboard += '\n';
                    }
                }

                imgui::SetClipboardText(clipboard.c_str());
            }

            imgui::Separator();

            auto handle_failure = [](char const *operation, generic_result const &result) noexcept {
                if (!result.success) {
                    u64 num_failed = std::count(result.error_or_utf8_path.begin(), result.error_or_utf8_path.end(), '\n');
                    std::string action = make_str("%s %zu items.", operation, num_failed);
                    char const *failed = result.error_or_utf8_path.c_str();
                    swan_popup_modals::open_error(action.c_str(), failed);
                }
            };

            if (imgui::Selectable("Cut##multi")) {
                if (!io.KeyShift) {
                    g_file_op_payload.clear();
                }
                auto result = add_selected_entries_to_file_op_payload(expl, "Cut", file_operation_type::move);
                handle_failure("Cut", result);
            }
            if (imgui::Selectable("Copy##multi")) {
                if (!io.KeyShift) {
                    g_file_op_payload.clear();
                }
                auto result = add_selected_entries_to_file_op_payload(expl, "Copy", file_operation_type::copy);
                handle_failure("Copy", result);
            }

            imgui::Separator();

            if (imgui::Selectable("Delete##multi")) {
                auto result = delete_selected_entries(expl);
                handle_failure("Delete", result);
            }
            if (imgui::Selectable("Bulk Rename")) {
                retval.open_bulk_rename_popup = true;
            }
        }

        ImVec2 popup_pos = imgui::GetWindowPos();
        ImVec2 popup_size = imgui::GetWindowSize();
        retval.context_menu_rect.emplace(popup_pos, popup_pos + popup_size);

        imgui::EndPopup();
    }
    else {
        if (expl.context_menu_target != nullptr) {
            expl.context_menu_target->context_menu_active = false;
        }
    }

    return retval;
}

explorer_window::cwd_entries_column_sort_specs_t
explorer_window::copy_column_sort_specs(ImGuiTableSortSpecs const *table_sort_specs) noexcept
{
    assert(table_sort_specs != nullptr);

    explorer_window::cwd_entries_column_sort_specs_t column_sort_specs_ret = {};

    for (s32 i = 0; i < table_sort_specs->SpecsCount; ++i) {
        auto column_sort_spec = table_sort_specs->Specs[i];
        column_sort_specs_ret.push_back(column_sort_spec);
    }

    return column_sort_specs_ret;
}

static
void accept_move_dirents_drag_drop(explorer_window &expl) noexcept
{
    if (imgui::BeginDragDropTarget()) {
        auto payload_wrapper = imgui::GetDragDropPayload();

        if (payload_wrapper != nullptr &&
            streq(payload_wrapper->DataType, "move_dirents_drag_drop_payload") &&
            reinterpret_cast<move_dirents_drag_drop_payload *>(payload_wrapper->Data)->src_explorer_id != expl.id)
        {
            payload_wrapper = imgui::AcceptDragDropPayload("move_dirents_drag_drop_payload");
            if (payload_wrapper != nullptr) {
                auto payload_data = (move_dirents_drag_drop_payload *)payload_wrapper->Data;
                assert(payload_data != nullptr);
                s32 src_explorer_id = payload_data->src_explorer_id;
                if (src_explorer_id != expl.id) {
                    auto result = move_files_into(expl.cwd, expl, *payload_data);
                    if (!result.success) {
                        // TODO: handle error
                    }
                }
            }
        }

        imgui::EndDragDropTarget();
    }
}

static
void render_footer(explorer_window &expl, cwd_count_info const &cnt, ImGuiStyle &style, bool cwd_exists_after_edit) noexcept
{
    if (imgui::BeginChild("explorer_footer", { 0, imgui::CalcTextSize("1").y + style.WindowPadding.y + 5*2 }), ImGuiWindowFlags_NoNav) {
        imgui::ScopedStyle<f32> s2(style.ItemSpacing.y, 5);
        imgui::Spacing(2);
        imgui::AlignTextToFramePadding();

        if (expl.refresh_message != "") {
            imgui::TextColored(warning_color(), "%s", expl.refresh_message.c_str());
            if (imgui::IsItemHovered()) {
                imgui::SetTooltip(expl.refresh_message_tooltip.c_str());
            }
            if (imgui::IsItemClicked(ImGuiMouseButton_Left)) {
                (void) expl.update_cwd_entries(full_refresh, expl.cwd.data());
            }
            imgui::SameLineSpaced(1);
        }

        if (!path_is_empty(expl.cwd)) {
            if (!cwd_exists_after_edit) {
                imgui::TextColored(warning_color(), "Directory not found");
            }
            else if (expl.cwd_entries.empty()) {
                imgui::TextColored(warning_color(), "Empty directory");
            }
            else {
                render_num_cwd_items(cnt);
            }

            if (cwd_exists_after_edit && cnt.child_dirents > 0 && cnt.filtered_dirents == cnt.child_dirents) {
                imgui::SameLineSpaced(1);
                imgui::TextColored(warning_color(), "All items filtered");
            }
            else if (expl.filter_error == "" && cnt.filtered_dirents > 0) {
                imgui::SameLineSpaced(1);
                render_num_cwd_items_filtered(expl, cnt);
            }

            if (cnt.selected_dirents > 0) {
                imgui::SameLineSpaced(1);
                render_num_cwd_items_selected(expl, cnt);
            }

            if (!g_file_op_payload.items.empty()) {
                imgui::SameLineSpaced(1);
                render_file_op_payload_hint();
            }
        }
    }

    imgui::EndChild();
}
