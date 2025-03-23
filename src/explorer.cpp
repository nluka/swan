#include "stdafx.hpp"
#include "common_functions.hpp"
#include "imgui_dependent_functions.hpp"
#include "path.hpp"
#include "scoped_timer.hpp"
#include "util.hpp"
#include "explorer_drop_source.hpp"

static IShellLinkW *g_shell_link = nullptr;
static IPersistFile *g_persist_file_interface = nullptr;
static std::array<explorer_window, global_constants::num_explorers> g_explorers = {};

std::array<explorer_window, global_constants::num_explorers> &global_state::explorers() noexcept { return g_explorers; }

void init_explorer_COM_GLFW_OpenGL3(GLFWwindow *window, char const *ini_file_path) noexcept
{
    bool retry = true; // start true to do initial load
    char const *what_failed = nullptr;

    while (true) {
        if (retry) {
            retry = false;
            what_failed = nullptr;

            HRESULT result_ole = OleInitialize(nullptr);

            if (FAILED(result_ole)) {
                what_failed = "OleInitialize";
                // apparently OleUninitialize() is only necessary for successful calls to OleInitialize()
            }

            HRESULT result_co = CoInitialize(nullptr);

            if (FAILED(result_co)) {
                CoUninitialize();
                what_failed = "CoInitialize";
            }
            else {
                result_co = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkW, (LPVOID *)&g_shell_link);

                if (FAILED(result_co)) {
                    what_failed = "CoCreateInstance(CLSID_ShellLink, ..., CLSCTX_INPROC_SERVER, IID_IShellLinkW, ...)";
                }
                else {
                    result_co = g_shell_link->QueryInterface(IID_IPersistFile, (LPVOID *)&g_persist_file_interface);

                    if (FAILED(result_co)) {
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

        BeginFrame_GLFW_OpenGL3(ini_file_path);

        if (imgui::Begin("Startup Error", nullptr, ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_AlwaysAutoResize)) {
            imgui::TextColored(error_color(), "Application is unable to continue, critical initialization failed:");
            imgui::TextUnformatted(what_failed);
            retry = imgui::Button("Retry");
        }
        imgui::End();

        EndFrame_GLFW_OpenGL3(window);
    }
}

init_explorer_COM_Win32_DX11_result init_explorer_COM_Win32_DX11() noexcept
{
    init_explorer_COM_Win32_DX11_result retval = {};

    u64 const max_attempts = 10;

    for (retval.num_attempts_made = 1; retval.num_attempts_made < max_attempts; ++retval.num_attempts_made) {
        retval.what_failed = nullptr;

        HRESULT result_ole = OleInitialize(nullptr);

        if (FAILED(result_ole)) {
            retval.what_failed = "OleInitialize";
            continue;
            // apparently OleUninitialize() is only necessary for successful calls to OleInitialize()
        }

        HRESULT result_co = CoInitialize(nullptr);

        if (FAILED(result_co)) {
            retval.what_failed = "CoInitialize";
            CoUninitialize();
            continue;
        }

        result_co = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkW, (LPVOID *)&g_shell_link);

        if (FAILED(result_co)) {
            retval.what_failed = "CoCreateInstance(CLSID_ShellLink ... CLSCTX_INPROC_SERVER, IID_IShellLinkW ...)";
            continue;
        }

        result_co = g_shell_link->QueryInterface(IID_IPersistFile, (LPVOID *)&g_persist_file_interface);

        if (FAILED(result_co)) {
            retval.what_failed = "IUnknown::QueryInterface(IID_IPersistFile, ...)";
            g_persist_file_interface->Release();
            CoUninitialize();
            continue;
        }
    }

    retval.success = retval.what_failed == nullptr;
    return retval;
}

void cleanup_explorer_COM() noexcept
try {
    g_persist_file_interface->Release();
    g_shell_link->Release();
    CoUninitialize();
    OleUninitialize();
}
catch (std::exception const &except) {
    print_debug_msg("FAILED catch(std::exception) %s", except.what());
}
catch (...) {
    print_debug_msg("FAILED catch(...)");
}

struct cwd_count_info
{
    u64 selected_dirents;
    u64 selected_directories;
    // u64 selected_symlinks;
    u64 selected_files;
    u64 selected_files_size;

    u64 filtered_dirents;
    u64 filtered_directories;
    // u64 filtered_symlinks;
    u64 filtered_files;

    u64 child_dirents;
    u64 child_directories;
    // u64 child_symlinks;
    u64 child_files;
};

static
void render_count_summary(u64 cnt_dir, u64 cnt_file, u64 cnt_symlink = 0) noexcept
{
    bool dir = cnt_dir > 0, file = cnt_file > 0, symlink = cnt_symlink > 0;

    if (dir) {
        imgui::Text("%s %zu", get_icon(basic_dirent::kind::directory), cnt_dir);
        // imgui::TextColored(directory_color(), "%zu %s", cnt_dir, get_icon(basic_dirent::kind::directory));
    }
    if (file) {
        if (dir) imgui::SameLine();
        imgui::Text("%s %zu", get_icon(basic_dirent::kind::file), cnt_file);
        // imgui::TextColored(file_color(), "%zu %s", cnt_file, get_icon(basic_dirent::kind::file));
    }
    if (symlink) {
        if (dir || file) imgui::SameLine();
        imgui::Text("%s %zu", get_icon(basic_dirent::kind::symlink_ambiguous), cnt_symlink);
        // imgui::TextColored(symlink_color(), "%zu %s", cnt_symlink, get_icon(basic_dirent::kind::symlink_ambiguous));
    }
}

struct
render_table_rows_for_cwd_entries_result
{
    std::optional<ImRect> context_menu_target_row_rect = std::nullopt;
    std::optional<swan_path> descend_target = std::nullopt;
    bool do_ascend = false;
};
static
render_table_rows_for_cwd_entries_result render_table_rows_for_cwd_entries(
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

u64 explorer_window::deselect_all_cwd_entries() noexcept
{
    u64 num_deselected = 0;

    for (auto &dirent : this->cwd_entries) {
        bool prev = dirent.selected;
        bool &curr = dirent.selected;
        curr = false;
        num_deselected += curr != prev;
    }

    return num_deselected;
}

void explorer_window::select_all_visible_cwd_entries(bool select_dotdot_dir) noexcept
{
    for (auto &dirent : this->cwd_entries) {
        if ( (!select_dotdot_dir && dirent.basic.is_path_dotdot()) || dirent.filtered ) {
            continue;
        } else {
            dirent.selected = true;
        }
    }
}

void explorer_window::invert_selection_on_visible_cwd_entries() noexcept
{
    for (auto &dirent : this->cwd_entries) {
        if (dirent.filtered) {
            dirent.selected = false;
        } else if (!dirent.basic.is_dotdot_dir()) {
            dirent.selected = !dirent.selected;
        }
    }
}

void explorer_window::set_latest_valid_cwd(swan_path const &new_latest_valid_cwd, bool prevent_filter_clear) noexcept
{
    if (!prevent_filter_clear && global_state::settings().explorer_clear_filter_on_cwd_change) {
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
        dirent.cut = false;
    }
}

void explorer_window::reset_filter() noexcept
{
    cstr_clear(this->filter_text.data());

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
        if (!dirent.selected || dirent.basic.is_path_dotdot()) {
            continue;
        }

        if (operation_type == file_operation_type::move) {
            if (dirent.cut) {
                continue; // prevent same dirent from being cut multiple times
                          // (although multiple copy commands of the same dirent are permitted, intentionally)
            } else {
                dirent.cut = true;
            }
        }

        if (operation_type == file_operation_type::copy && dirent.cut) {
            // this situation wouldn't make sense, because you can't CopyItem after MoveItem since there's nothing left to copy
            // TODO: maybe indicate something to the user rather than ignoring their request?
            continue;
        }

        swan_path src = expl.cwd;

        if (path_append(src, dirent.basic.path.data(), global_state::settings().dir_separator_utf8, true)) {
            global_state::file_op_cmd_buf().items.push_back({ operation_desc, operation_type, dirent.basic.type, src });
        } else {
            err << "Current working directory path + [" << src.data() << "] exceeds max allowed path length.\n";
        }
    }

    std::string errors = err.str();

    return { errors.empty(), errors };
}

static
generic_result delete_selected_entries(explorer_window &expl, swan_settings const &settings) noexcept
{
    auto file_operation_task = [](
        std::wstring working_directory_utf16,
        std::wstring paths_to_delete_utf16,
        std::mutex *init_done_mutex,
        std::condition_variable *init_done_cond,
        bool *init_done,
        std::string *init_error,
        char dir_sep_utf8,
        s32 num_max_file_operations
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

        explorer_file_op_progress_sink prog_sink = {};
        prog_sink.contains_delete_operations = true;
        prog_sink.dst_expl_id = -1;
        prog_sink.dst_expl_cwd_when_operation_started = path_create("");
        prog_sink.dir_sep_utf8 = dir_sep_utf8;
        prog_sink.num_max_file_operations = num_max_file_operations;

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

            prog_sink.group_id = global_state::completed_file_operations_calc_next_group_id();
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

    wchar_t cwd_utf16[MAX_PATH]; cstr_clear(cwd_utf16);

    if (!utf8_to_utf16(expl.cwd.data(), cwd_utf16, lengthof(cwd_utf16))) {
        return { false, "Conversion of current working directory path from UTF-8 to UTF-16." };
    }

    std::wstring packed_paths_to_delete_utf16 = {};
    u64 item_count = 0;
    {
        wchar_t item_utf16[MAX_PATH];
        std::stringstream err = {};

        for (auto const &item : expl.cwd_entries) {
            if (!item.filtered && item.selected) {
                cstr_clear(item_utf16);

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
        &initialization_error,
        settings.dir_separator_utf8,
        settings.num_max_file_operations);

    {
        std::unique_lock lock(expl.shlwapi_task_initialization_mutex);
        expl.shlwapi_task_initialization_cond.wait(lock, [&]() noexcept { return initialization_done; });
    }

    return { initialization_error.empty(), initialization_error };
}

generic_result move_files_into(swan_path const &destination_utf8, explorer_window &expl, explorer_drag_drop_payload &payload) noexcept
{
    /*
        ? Moving files via shlwapi is a blocking operation.
        ? Thus we must call IFileOperation::PerformOperations outside of main UI thread so user can continue to interact with UI during file operation.
        ? There is a constraint however: the IFileOperation object must be initialized on same thread which will call PerformOperations.
        ? This function blocks until IFileOperation initialization is complete so that we can report any initialization errors to user.
        ? After worker thread signals that initialization is complete, we proceed with execution and let PerformOperations happen asynchronously.
    */

    SCOPE_EXIT { free_explorer_drag_drop_payload(); };

    wchar_t destination_utf16[MAX_PATH]; cstr_clear(destination_utf16);

    if (!utf8_to_utf16(destination_utf8.data(), destination_utf16, lengthof(destination_utf16))) {
        return { false, "Conversion of destination directory path from UTF-8 to UTF-16." };
    }

    bool initialization_done = false;
    std::string initialization_error = {};

    global_state::thread_pool().push_task(perform_file_operations,
        expl.id,
        destination_utf16,
        std::wstring(payload.full_paths_delimited_by_newlines),
        std::vector<file_operation_type>(payload.num_items, file_operation_type::move),
        &expl.shlwapi_task_initialization_mutex,
        &expl.shlwapi_task_initialization_cond,
        &initialization_done,
        &initialization_error,
        global_state::settings().dir_separator_utf8,
        global_state::settings().num_max_file_operations);

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
    auto payload_data = (explorer_drag_drop_payload *)payload_wrapper->Data;
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

generic_result symlink_data::load(char const *lnk_file_path_utf8, char const *cwd) noexcept
{
    assert(lnk_file_path_utf8 != nullptr);

    swan_path lnk_file_full_path_utf8;
    wchar_t lnk_file_path_utf16[MAX_PATH]; cstr_clear(lnk_file_path_utf16);
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
        return { false, err + " (from IShellLinkW::GetIDList)." };
    }

    if (!SHGetPathFromIDListW(item_id_list, this->target_path_utf16)) {
        auto err = cstr_empty(this->target_path_utf16) ? "Empty target path." : get_last_winapi_error().formatted_message;
        return { false, err + " (from SHGetPathFromIDListW)." };
    }

    if (!utf16_to_utf8(this->target_path_utf16, this->target_path_utf8.data(), this->target_path_utf8.size())) {
        return { false, "Conversion of symlink target path from UTF-16 to UTF-8." };
    }

    com_handle = g_shell_link->GetWorkingDirectory(this->working_directory_path_utf16, MAX_PATH);
    if (com_handle != S_OK) {
        return { false, get_last_winapi_error().formatted_message + " (from IShellLinkW::GetWorkingDirectory)." };
    }

    com_handle = g_shell_link->GetArguments(this->arguments_utf16, 1024);
    if (com_handle != S_OK) {
        return { false, get_last_winapi_error().formatted_message + " (from IShellLinkW::GetArguments)." };
    }

    com_handle = g_shell_link->GetShowCmd(&this->show_cmd);
    if (com_handle != S_OK) {
        return { false, get_last_winapi_error().formatted_message + " (from IShellLinkW::GetShowCmd)" };
    }

    return { true, "" }; // success
}

generic_result symlink_data::save(char const *lnk_file_path_utf8, char const *cwd) noexcept
{
    assert(lnk_file_path_utf8 != nullptr);
    assert(this->target_path_utf8.data() != nullptr);

    swan_path lnk_file_full_path_utf8;
    wchar_t lnk_file_path_utf16[MAX_PATH];
    cstr_clear(lnk_file_path_utf16);
    HRESULT com_handle = {};

    if (cwd) {
        lnk_file_full_path_utf8 = path_create(cwd);

        if (!path_append(lnk_file_full_path_utf8, lnk_file_path_utf8, global_state::settings().dir_separator_utf8, true)) {
            return { false, "Max path length exceeded when appending symlink name to current working directory path." };
        }
    } else {
        lnk_file_full_path_utf8 = path_create(lnk_file_path_utf8);
    }

    if (!utf8_to_utf16(lnk_file_full_path_utf8.data(), lnk_file_path_utf16, lengthof(lnk_file_path_utf16))) {
        return { false, "Conversion of symlink path from UTF-8 to UTF-16 failed." };
    }

    if (!utf8_to_utf16(this->target_path_utf8.data(), this->target_path_utf16, MAX_PATH)) {
        return { false, "Conversion of symlink target path from UTF-8 to UTF-16 failed." };
    }

    com_handle = g_shell_link->SetPath(this->target_path_utf16);
    if (com_handle != S_OK) {
        return { false, get_last_winapi_error().formatted_message + " (from IShellLinkW::SetPath)." };
    }

    com_handle = g_shell_link->SetWorkingDirectory(this->working_directory_path_utf16);
    if (com_handle != S_OK) {
        return { false, get_last_winapi_error().formatted_message + " (from IShellLinkW::SetWorkingDirectory)." };
    }

    com_handle = g_shell_link->SetArguments(this->arguments_utf16);
    if (com_handle != S_OK) {
        return { false, get_last_winapi_error().formatted_message + " (from IShellLinkW::SetArguments)." };
    }

    com_handle = g_shell_link->SetShowCmd(this->show_cmd);
    if (com_handle != S_OK) {
        return { false, get_last_winapi_error().formatted_message + " (from IShellLinkW::SetShowCmd)." };
    }

    com_handle = g_shell_link->SetDescription(L"");
    if (com_handle != S_OK) {
        return { false, get_last_winapi_error().formatted_message + " (from IShellLinkW::SetDescription)." };
    }

    com_handle = g_shell_link->QueryInterface(IID_IPersistFile, reinterpret_cast<void**>(&g_persist_file_interface));
    if (com_handle != S_OK) {
        return { false, get_last_winapi_error().formatted_message + " (from IShellLinkW::QueryInterface(IID_IPersistFile)." };
    }

    com_handle = g_persist_file_interface->Save(lnk_file_path_utf16, TRUE);
    if (com_handle != S_OK) {
        return { false, get_last_winapi_error().formatted_message + " (from IPersistFile::Save)." };
    }

    return { true, "" }; // success
}

/// @brief Attempts to load information from a .lnk file and do something with it depending on the target type.
/// @param dirent The .lnk file.
/// @param expl Contextual explorer window.
/// @return If symlink points to a file, attempt ShellExecuteW("open") and return the result.
/// If symlink points to a directory, return the path of the pointed to directory in `error_or_utf8_path`.
/// If data extraction fails, the reason is stated in `error_or_utf8_path`.
static
generic_result open_symlink(explorer_window::dirent const &dirent, explorer_window &expl) noexcept
{
    symlink_data lnk_data = {};
    auto extract_result = lnk_data.load(dirent.basic.path.data(), expl.cwd.data());
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

/// @brief Partitions and sorts `expl.cwd_entries` by `filtered` and `expl.sort_specs`, in place.
/// Entries are partitioned by the `filtered` flag.
/// The first partition contains the entries with `filtered == false`, sorted according to `expl.sort_specs`.
/// The second partition contains entries with `filtered == true`, whose order is undefined.
/// @return Iterator to the second partition, can be `cwd_entries.end()` if all entries are `filtered == false`.
static
std::vector<explorer_window::dirent>::iterator
sort_cwd_entries(explorer_window &expl, std::source_location sloc = std::source_location::current()) noexcept
{
    f64 sort_us = 0;
    SCOPE_EXIT { expl.sort_timing_samples.push_back(sort_us); };
    scoped_timer<timer_unit::MICROSECONDS> sort_timer(&sort_us);

    auto &cwd_entries = expl.cwd_entries;

    print_debug_msg("[ %d ] sort_cwd_entries() called from [%s:%d]", expl.id, path_cfind_filename(sloc.file_name()), sloc.line());

    using dir_ent_t = explorer_window::dirent;

    auto first_filtered_dirent = std::partition(cwd_entries.begin(), cwd_entries.end(), [](dir_ent_t const &dirent) noexcept {
        return !dirent.filtered;
    });

    s32 obj_precedence_table[(u64)basic_dirent::kind::count] = {
        10, // directory
        10, // symlink_to_directory
        5,  // file
        5,  // symlink_to_file
        5,  // symlink_ambiguous
        5,  // invalid_symlink
    };

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
                case explorer_window::cwd_entries_table_col_object:
                case explorer_window::cwd_entries_table_col_type: {
                    assert((s32)right.basic.type >= 0);
                    delta = obj_precedence_table[(u64)left.basic.type] - obj_precedence_table[(u64)right.basic.type];
                    break;
                }
                // case explorer_window::cwd_entries_table_col_type: {
                //     if (left.basic.type != right.basic.type) {
                //         assert((s32)right.basic.type >= 0);
                //         delta = obj_precedence_table[(u64)left.basic.type] - obj_precedence_table[(u64)right.basic.type];
                //     } else {
                //         delta = lstrcmpiA(path_cfind_file_ext(right.basic.path.data()), path_cfind_file_ext(left.basic.path.data()));
                //     }
                //     break;
                // }
                case explorer_window::cwd_entries_table_col_size_formatted:
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
    f64 time_inside_func_us = 0;
    SCOPE_EXIT { this->update_cwd_entries_culmulative_us += time_inside_func_us; };
    scoped_timer<timer_unit::MICROSECONDS> culm_timer(&time_inside_func_us);

    update_cwd_entries_result retval = {};

    print_debug_msg("[ %d ] expl.update_cwd_entries(%d) called from [%s:%d]", this->id, actions, path_cfind_filename(sloc.file_name()), sloc.line());

    this->scroll_to_nth_selected_entry_next_frame = u64(-1);
    this->tabbing_focus_idx = -1;

    update_cwd_entries_timers timers = {};
    SCOPE_EXIT { this->update_cwd_entries_timing_samples.push_back(timers); };

    char dir_sep_utf8 = global_state::settings().dir_separator_utf8;

    {
        scoped_timer<timer_unit::MICROSECONDS> function_timer(&timers.total_us);

        if (actions & query_filesystem) {
            static std::vector<swan_path> s_preserve_select = {};
            s_preserve_select.clear();

            for (auto const &dirent : this->cwd_entries) {
                if (dirent.selected) {
                    // this could throw on alloc failure, which will call std::terminate
                    s_preserve_select.push_back(dirent.basic.path);
                }
            }
            // std::sort(s_preserve_select.begin(), s_preserve_select.end());

            for (auto &e : this->cwd_entries) {
                if (e.icon_GLtexID > 0) {
                    // if we delete the icon texture immediately, ImGui will render a black square for the image
                    // because the frame is drawn much later.
                    // !delete_icon_texture(e.icon_GLtexID, "explorer_window::dirent");

                    // Instead, delete the texture after the frame (that we are currently rendering) is actually drawn by the GPU.
                    global_state::delete_icon_textures_queue().push_back(e.icon_GLtexID);
                }
            }
            this->cwd_entries.clear();

            if (parent_dir != "") {
                bool inside_recycle_bin;

                wchar_t search_path_utf16[512]; cstr_clear(search_path_utf16);
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

                    inside_recycle_bin = cstr_starts_with(parent_dir_trimmed.data() + 1, ":\\$Recycle.Bin\\"); // assume drive letter is first char
                }

                // just for debug log
                {
                    char utf8_buffer[2048]; cstr_clear(utf8_buffer);

                    if (!utf16_to_utf8(search_path_utf16, utf8_buffer, lengthof(utf8_buffer))) {
                        return retval;
                    }

                    print_debug_msg("[ %d ] querying filesystem, search_path = [%s]", this->id, utf8_buffer);
                }

                std::scoped_lock lock(select_cwd_entries_on_next_update_mutex); // lock for rest of this function to prevent other threads from adding items and breaking order
                {
                    scoped_timer<timer_unit::MICROSECONDS> sort_timer(&timers.entries_to_select_sort);
                    std::sort(select_cwd_entries_on_next_update.begin(), select_cwd_entries_on_next_update.end(), std::less<swan_path>());
                }

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
                        // if (!s_preserve_select.empty()) {
                        //     auto [first_iter, last_iter] = std::equal_range(s_preserve_select.begin(), s_preserve_select.end(), entry.basic.path);
                        //     if (bool found = std::distance(first_iter, last_iter) == 1) {
                        //         entry.selected = true;
                        //         retval.num_entries_selected += 1;
                        //         std::swap(*first_iter, s_preserve_select.back());
                        //         s_preserve_select.pop_back();
                        //     }
                        // }
                        //? Don't bother trying to make this more efficient, instead work on issue #3 which will eliminate this code
                        for (auto prev_selected_entry = s_preserve_select.begin(); prev_selected_entry != s_preserve_select.end(); ++prev_selected_entry) {
                            bool was_selected_before_refresh = path_equals_exactly(entry.basic.path, *prev_selected_entry);
                            if (was_selected_before_refresh) {
                                entry.selected = true;
                                retval.num_entries_selected += 1;
                                std::swap(*prev_selected_entry, s_preserve_select.back());
                                s_preserve_select.pop_back();
                                break;
                            }
                        }
                        {
                            f64 search_us = 0;
                            scoped_timer<timer_unit::MICROSECONDS> search_timer(&search_us);

                            if (!this->select_cwd_entries_on_next_update.empty()) {
                                auto [first_iter, last_iter] = std::equal_range(this->select_cwd_entries_on_next_update.begin(),
                                                                                this->select_cwd_entries_on_next_update.end(), entry.basic.path);
                                if (bool found = std::distance(first_iter, last_iter) == 1) {
                                    entry.selected = true;
                                    retval.num_entries_selected += 1;
                                }
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
                this->last_filesystem_query_time = get_time_precise();
                {
                    // std::scoped_lock lock(select_cwd_entries_on_next_update_mutex);
                    this->select_cwd_entries_on_next_update.clear();
                }
            }
        }

        if (actions & filter) {
            scoped_timer<timer_unit::MICROSECONDS> filter_timer(&timers.filter_us);

            this->filter_error.clear();

            bool dirent_type_to_visibility_table[(u64)basic_dirent::kind::count] = {
                /* directory */                  this->filter_show_directories,
                /* symlink_to_directory */       this->filter_show_directories, // filter_show_symlink_directories
                /* file */                       this->filter_show_files,
                /* symlink_to_file */            this->filter_show_files, // filter_show_files
                /* symlink_ambiguous */          true,
                /* invalid_symlink */            this->filter_show_files // filter_show_invalid_symlinks
            };

            u64 filter_text_len = strlen(this->filter_text.data());

            static std::regex s_filter_regex;
            if (this->filter_mode == explorer_window::filter_mode::regex_match) {
                try {
                    scoped_timer<timer_unit::MICROSECONDS> regex_ctor_timer(&timers.regex_ctor_us);
                    s_filter_regex = this->filter_text.data();
                }
                catch (std::exception const &except) {
                    this->filter_error = except.what();
                }
            }

            for (auto &dirent : this->cwd_entries) {
                assert((s32)dirent.basic.type != -1);
                bool this_type_of_dirent_is_visible = dirent_type_to_visibility_table[(u64)dirent.basic.type];

                dirent.filtered = !this_type_of_dirent_is_visible;
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
                            dirent.filtered = filtered_out;

                            if (!filtered_out && filter_polarity == true) {
                                // highlight just the substring
                                dirent.highlight_start_idx = std::distance(dirent_name, match_start);
                                dirent.highlight_len = filter_text_len;
                            }

                            break;
                        }

                        case explorer_window::filter_mode::regex_match: {
                            auto match_flags = std::regex_constants::match_default | (std::regex_constants::icase * (this->filter_case_sensitive == 0));

                            bool filtered_out = this->filter_polarity != std::regex_match(dirent_name, s_filter_regex, (std::regex_constants::match_flag_type)match_flags);
                            dirent.filtered = filtered_out;

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

    char file_name[32]; cstr_clear(file_name);
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

            out << "tree_node_open_debug_state "        << (s32)tree_node_open_debug_state << '\n';
            out << "tree_node_open_debug_memory "       << (s32)tree_node_open_debug_memory << '\n';
            out << "tree_node_open_debug_performance "  << (s32)tree_node_open_debug_performance << '\n';
            out << "tree_node_open_debug_other "        << (s32)tree_node_open_debug_other << '\n';

            out << "wd_history_pos "                    << wd_history_pos << '\n';

            for (auto const &item : wd_history) {
                out << "wd_history_elem " << path_length(item.path) << ' ' << item.path.data() << ' '
                    << std::chrono::system_clock::to_time_t(item.time_departed) << '\n';
            }
        }
    }
    catch (std::exception const &except) {
        print_debug_msg("FAILED catch(std::exception) %s", except.what());
    }
    catch (...) {
        print_debug_msg("FAILED catch(...)");
        result = false;
    }

    print_debug_msg("%s [%s]", file_name, result ? "SUCCESS" : "FAILED");
    this->latest_save_to_disk_result = (s8)result;

    return result;
}

bool explorer_window::load_from_disk(char dir_separator) noexcept
{
    assert(this->name != nullptr);

    char file_name[32]; cstr_clear(file_name);
    [[maybe_unused]] s32 written = snprintf(file_name, lengthof(file_name), "data\\explorer_%d.txt", this->id);
    assert(written < lengthof(file_name));

    u64 num_lines_parsed = 0;
    u64 num_lines_skipped = 0;

    try {

    std::filesystem::path full_path = global_state::execution_path() / file_name;
    std::ifstream ifs(full_path);
    if (!ifs) {
        print_debug_msg("FAILED explorer_window::load_from_disk, !file");
        return false;
    }

    auto content = std::string(std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>());
    while (content.back() == '\n') {
        content.pop_back();
    }

    auto lines = std::string_view(content) | std::ranges::views::split('\n');

    char const *key_pattern = "[a-z0-9_.]{1,}";
    char const *uint_pattern = "[0-9]{1,}";
    char const *uint_or_float_pattern = "[0-9]{1,}.?[0-9]{0,}";

    auto key_numerical_pattern = make_str_static<128>("^%s %s$", key_pattern, uint_or_float_pattern);
    auto key_ImVec4_pattern = make_str_static<128>("^%s %s %s %s %s$", key_pattern, uint_or_float_pattern, uint_or_float_pattern, uint_or_float_pattern, uint_or_float_pattern);
    auto key_path_pattern = make_str_static<128>("^%s %s [a-z]:[\\\\/].*$", key_pattern, uint_pattern);
    auto filter_pattern = make_str_static<128>("^filter %s .*$", key_pattern, uint_pattern);
    auto wd_history_elem_pattern = make_str_static<128>("^wd_history_elem %s [a-z]:[\\\\/].* %s$", uint_pattern, key_pattern);

    std::vector<std::regex> valid_line_regexs = {};
    valid_line_regexs.emplace_back(key_numerical_pattern.data(), std::regex_constants::icase);
    valid_line_regexs.emplace_back(key_ImVec4_pattern.data(), std::regex_constants::icase);
    valid_line_regexs.emplace_back(key_path_pattern.data(), std::regex_constants::icase);
    valid_line_regexs.emplace_back(filter_pattern.data(), std::regex_constants::icase);
    valid_line_regexs.emplace_back(wd_history_elem_pattern.data(), std::regex_constants::icase);

    std::stringstream ss;
    std::string line_str = {};
    std::string property = {};
    u64 line_num = 0;

    auto extract_bool = [&]() noexcept -> bool {
        char bool_ch = {};
        ss >> bool_ch;
        return bool_ch == '1';
    };
    auto extract_u64 = [&]() noexcept -> u64 {
        u64 val = {};
        ss >> val;
        return val;
    };
    auto extract_f32 = [&]() noexcept -> f32 {
        f32 val = {};
        ss >> val;
        return val;
    };
    auto extract_ImGuiDir = [&]() noexcept -> ImGuiDir {
        s32 val = {};
        ss >> val;
        return static_cast<ImGuiDir>(val);
    };
    auto extract_ImVec4 = [&]() noexcept -> ImVec4 {
        f32 x, y, z, w;
        ss >> x >> y >> z >> w;
        return ImVec4(x, y, z, w);
    };
    auto skip_one_whitespace_char = [](std::istream &instream) {
        char whitespace = 0;
        instream.read(&whitespace, 1);
        assert(whitespace == ' ');
    };

    for (auto const &line : lines) {
        ++line_num;
        line_str = std::string(line.data(), line.size());

        if (line_str.empty()) {
            print_debug_msg("Blank line, skipping...", line_num);
            ++num_lines_skipped;
            continue;
        }

        print_debug_msg("Parsing line %zu [%s]", line_num, line_str.c_str());

        bool malformed_line = false;
        for (auto const &valid_line_regex : valid_line_regexs) {
            if (std::regex_match(line_str, valid_line_regex)) {
                malformed_line = true;
                break;
            }
        }
        if (!malformed_line) {
            print_debug_msg("ERROR malformed line %zu, skipping...", line_num);
            ++num_lines_skipped;
            continue;
        }

        ss.str(""); ss.clear();
        ss << line_str;
        ss >> property;

        if (property == "cwd") {
            u64 cwd_len = extract_u64();
            if (cwd_len > 0) {
                skip_one_whitespace_char(ss);
                ss.read(this->cwd.data(), cwd_len);
                path_force_separator(this->cwd, dir_separator);
            }
        }
        else if (property == "filter") {
            u64 filter_len = extract_u64();
            if (filter_len > 0) {
                skip_one_whitespace_char(ss);
                ss.read(this->filter_text.data(), filter_len);
            }
        }
        else if (property.starts_with("wd_history_")) {
            std::string_view remainder(property.c_str() + lengthof("wd_history"));

            if (remainder == "pos") {
                ss >> (s32 &)this->wd_history_pos;
            }
            else if (remainder == "elem") {
                u64 path_len = extract_u64();
                assert(path_len > 0);

                explorer_window::history_item elem = {};

                if (path_len > 0) {
                    skip_one_whitespace_char(ss);
                    ss.read(elem.path.data(), path_len);
                    skip_one_whitespace_char(ss);
                    elem.time_departed = extract_system_time_from_istream(ss);
                    this->wd_history.push_back(elem);
                }
            }
            else {
                print_debug_msg("Unknown property [%s] at line %zu, skipping...", property.c_str(), line_num);
                ++num_lines_skipped;
                continue;
            }
        }
        else if (property.starts_with("filter_")) {
            std::string_view remainder(property.c_str() + lengthof("filter"));

            if (remainder == "mode") {
                ss >> (s32 &)this->filter_mode;
            }
            else if (remainder == "case_sensitive") {
                this->filter_case_sensitive = extract_bool();
            }
            else if (remainder == "polarity") {
                this->filter_polarity = extract_bool();
            }
            else if (remainder == "show_directories") {
                this->filter_show_directories = extract_bool();
            }
            else if (remainder == "show_symlink_directories") {
                this->filter_show_symlink_directories = extract_bool();
            }
            else if (remainder == "show_files") {
                this->filter_show_files = extract_bool();
            }
            else if (remainder == "show_symlink_files") {
                this->filter_show_symlink_files = extract_bool();
            }
            else if (remainder == "show_invalid_symlinks") {
                this->filter_show_invalid_symlinks = extract_bool();
            }
            else {
                print_debug_msg("Unknown property [%s] at line %zu, skipping...", property.c_str(), line_num);
                ++num_lines_skipped;
                continue;
            }
        }
        else if (property.starts_with("tree_node_open_debug_")) {
            std::string_view remainder(property.c_str() + lengthof("tree_node_open_debug"));

            if (remainder == "state") {
                this->tree_node_open_debug_state = extract_bool();
            }
            else if (remainder == "memory") {
                this->tree_node_open_debug_memory = extract_bool();
            }
            else if (remainder == "performance") {
                this->tree_node_open_debug_performance = extract_bool();
            }
            else if (remainder == "other") {
                this->tree_node_open_debug_other = extract_bool();
            }
            else {
                print_debug_msg("Unknown property [%s] at line %zu, skipping...", property.c_str(), line_num);
                ++num_lines_skipped;
                continue;
            }
        }
        else {
            print_debug_msg("Unknown property [%s] at line %zu, skipping...", property.c_str(), line_num);
            ++num_lines_skipped;
            continue;
        }

        ++num_lines_parsed;
    }

    }
    catch (std::exception const &except) {
        print_debug_msg("FAILED catch(std::exception) %s", except.what());
        return false;
    }
    catch (...) {
        print_debug_msg("FAILED catch(...)");
        return false;
    }

    this->wd_history_pos = std::clamp(this->wd_history_pos, u64(0), this->wd_history.size() - 1);

    print_debug_msg("SUCCESS parsed %zu lines, skipped %zu", num_lines_parsed, num_lines_skipped);
    return true;
}

void explorer_window::advance_history(swan_path const &new_latest_entry) noexcept
{
    char dir_sep_utf8 = global_state::settings().dir_separator_utf8;

    if (this->wd_history_pos > 0) {
        assert(!this->wd_history.empty());
        // we are not at head of history, new entry will become head
        this->wd_history.erase(this->wd_history.begin(), this->wd_history.begin() + this->wd_history_pos);
        this->wd_history_pos = 0;
    }

    swan_path new_latest_entry_clean = new_latest_entry;
    path_force_separator(new_latest_entry_clean, dir_sep_utf8);
    while (path_pop_back_if(new_latest_entry_clean, dir_sep_utf8));
    new_latest_entry_clean = path_reconstruct_canonically(new_latest_entry_clean.data());

    if (!this->wd_history.empty() && path_loosely_same(new_latest_entry_clean, this->wd_history.back().path.data())) {
        return; // avoid pushing adjacent duplicates
    }

    u64 size_pre_insert = this->wd_history.size();
    u64 pos_pre_insert = this->wd_history_pos;

    this->wd_history.emplace_front(get_time_system(), new_latest_entry_clean);

    while (this->wd_history.size() > MAX_WD_HISTORY_SIZE) {
        this->wd_history.pop_back();
    }

    if (size_pre_insert == 0) {
        // we now have 1 elem, point to it
        this->wd_history_pos = 0;
    } else if (pos_pre_insert > 0) {
        this->wd_history_pos -= 1;
    }
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
            expl.advance_history(res.parent_dir);
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

    swan_path new_cwd_canoncial_utf8 = path_reconstruct_canonically(expl.cwd.data());
    path_force_separator(new_cwd_canoncial_utf8, dir_sep_utf8);

    bool prepend_separator = path_length(expl.cwd) > 0;
    if (!path_append(new_cwd_canoncial_utf8, target_utf8, dir_sep_utf8, prepend_separator)) {
        print_debug_msg("[ %d ] FAILED path_append, new_cwd_utf8 = [%s], append data = [%c%s]", expl.id, new_cwd_canoncial_utf8.data(), dir_sep_utf8, target_utf8);
        descend_result res;
        res.success = false;
        res.err_msg = "Max path length exceeded when trying to append target to current working directory path.";
        return res;
    }

    wchar_t new_cwd_canonical_utf16[MAX_PATH]; cstr_clear(new_cwd_canonical_utf16);

    if (!utf8_to_utf16(new_cwd_canoncial_utf8.data(), new_cwd_canonical_utf16, lengthof(new_cwd_canonical_utf16))) {
        descend_result res;
        res.success = false;
        res.err_msg = "Conversion of new cwd path from UTF-8 to UTF-16.";
        return res;
    }

    swan_path new_cwd_utf8;

    if (!utf16_to_utf8(new_cwd_canonical_utf16, new_cwd_utf8.data(), new_cwd_utf8.size())) {
        descend_result res;
        res.success = false;
        res.err_msg = "Conversion of new canonical cwd path from UTF-8 to UTF-16.";
        return res;
    }

    auto [cwd_exists, _] = expl.update_cwd_entries(query_filesystem, new_cwd_canoncial_utf8.data());

    if (!cwd_exists) {
        descend_result res;
        res.success = false;
        res.err_msg = "Directory not found.";
        return res;
    }

    expl.advance_history(new_cwd_canoncial_utf8);
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

bool swan_windows::render_explorer_debug(explorer_window &expl, bool &open, bool any_popups_open) noexcept
{
    (void) any_popups_open;

    imgui::SetNextWindowSize({ 1000, 720 }, ImGuiCond_Appearing);

    {
        // auto window_name = make_str_static<64>(" Explorer %d Debug Info ", expl.id + 1);

        auto id = (s32)swan_windows::id::explorer_0_debug + expl.id;

        if (!imgui::Begin(swan_windows::get_name(swan_windows::id(id)), &open, ImGuiWindowFlags_NoCollapse)) {
            return false;
        }
    }

    u64 size_unit_multiplier = global_state::settings().size_unit_multiplier;

    static bool s_initalized = false;
    if (s_initalized == false) {
        s_initalized = true;
        imgui::TreeNodeSetOpen(imgui::GetCurrentWindow()->GetID("State"), expl.tree_node_open_debug_state);
        imgui::TreeNodeSetOpen(imgui::GetCurrentWindow()->GetID("Memory"), expl.tree_node_open_debug_memory);
        imgui::TreeNodeSetOpen(imgui::GetCurrentWindow()->GetID("Performance"), expl.tree_node_open_debug_performance);
        imgui::TreeNodeSetOpen(imgui::GetCurrentWindow()->GetID("Other"), expl.tree_node_open_debug_other);
    }

    bool open_states_begin[] = {
        expl.tree_node_open_debug_state,
        expl.tree_node_open_debug_memory,
        expl.tree_node_open_debug_performance,
        expl.tree_node_open_debug_other,
    };

    expl.tree_node_open_debug_state = imgui::TreeNode("State");
    if (expl.tree_node_open_debug_state) {
        imgui::Text("latest_valid_cwd: [%s]", expl.latest_valid_cwd.data());
        imgui::Text("select_cwd_entries_on_next_update.size(): %zu", expl.select_cwd_entries_on_next_update.size());
        imgui::Text("cwd_latest_selected_dirent_idx: %zu", expl.cwd_latest_selected_dirent_idx);
        imgui::Text("latest_save_to_disk_result: %d", expl.latest_save_to_disk_result);
        imgui::Text("cwd_input_text_scroll_x: %.1f", expl.cwd_input_text_scroll_x);

        // bools
        imgui::Text("show_filter_window: %d", expl.show_filter_window);
        imgui::Text("filter_text_input_focused: %d", expl.filter_text_input_focused);
        imgui::Text("cwd_latest_selected_dirent_idx_changed: %d", expl.cwd_latest_selected_dirent_idx_changed);
        imgui::Text("footer_hovered: %d", expl.footer_hovered);
        imgui::Text("footer_filter_info_hovered: %d", expl.footer_filter_info_hovered);
        imgui::Text("footer_selection_info_hovered: %d", expl.footer_selection_info_hovered);
        imgui::Text("footer_clipboard_hovered: %d", expl.footer_clipboard_hovered);

        imgui::TreePop();
    }

    imgui::Separator();

    expl.tree_node_open_debug_memory = imgui::TreeNode("Memory");
    if (expl.tree_node_open_debug_memory) {
        std::array<char, 32> cwd_entries_occupied, cwd_entries_capacity, swan_path_occupied, swan_path_wasted;
        f64 swan_path_waste_percent;
        {
            u64 elem_size = sizeof(explorer_window::dirent);
            u64 num_elems = expl.cwd_entries.size();
            u64 elem_capacity = expl.cwd_entries.capacity();

            u64 bytes_occupied = num_elems * elem_size;
            u64 bytes_reserved = elem_capacity * elem_size;

            cwd_entries_occupied = format_file_size(bytes_occupied, size_unit_multiplier);
            cwd_entries_capacity = format_file_size(bytes_reserved, size_unit_multiplier);
        }
        {
            u64 bytes_occupied = expl.cwd_entries.size() * sizeof(swan_path);
            u64 bytes_actually_used = 0;

            for (auto const &dirent : expl.cwd_entries) {
                bytes_actually_used += path_length(dirent.basic.path);
            }

            f64 usage_ratio = ( f64(bytes_actually_used) + (bytes_occupied == 0) ) / ( f64(bytes_occupied) + (bytes_occupied == 0) );
            swan_path_waste_percent = 100.0 - (usage_ratio * 100.0);
            u64 bytes_wasted = u64( bytes_occupied * (1.0 - usage_ratio) );

            swan_path_occupied = format_file_size(bytes_occupied, size_unit_multiplier);
            swan_path_wasted = format_file_size(bytes_wasted, size_unit_multiplier);
        }
        imgui::Text("cwd_entries (used): %s", cwd_entries_occupied.data());
        imgui::Text("cwd_entries (capacity): %s", cwd_entries_capacity.data());
        imgui::Text("swan_path footprint: %s, (%3.1lf %% waste, %s)", swan_path_occupied.data(), swan_path_waste_percent, swan_path_wasted.data());

        imgui::TreePop();
    }

    imgui::Separator();

    expl.tree_node_open_debug_performance = imgui::TreeNode("Performance");
    if (expl.tree_node_open_debug_performance) {
        imgui::SeparatorText("(Latest)");
        imgui::Text("entries_to_select_sort: %.1lf us", expl.update_cwd_entries_timing_samples.empty() ? NAN : expl.update_cwd_entries_timing_samples.back().entries_to_select_sort);
        imgui::Text("entries_to_select_search: %.1lf us", expl.update_cwd_entries_timing_samples.empty() ? NAN : expl.update_cwd_entries_timing_samples.back().entries_to_select_search);

        imgui::SeparatorText("(Culmulative)");
        imgui::Text("num_file_finds: %zu", expl.num_file_finds);
        imgui::Text("update_cwd_entries_culmulative: %.0lf ms", expl.update_cwd_entries_culmulative_us / 1000.);
        imgui::Text("filetime_to_string_culmulative: %.0lf ms", expl.filetime_to_string_culmulative_us / 1000.);
        imgui::Text("format_file_size_culmulative: %.0lf ms", expl.format_file_size_culmulative_us / 1000.);
        imgui::Text("type_description_culmulative_us: %.0lf ms", expl.type_description_culmulative_us / 1000.);

        imgui::TreePop();
    }

    imgui::Separator();

    expl.tree_node_open_debug_other = imgui::TreeNode("Other");
    if (expl.tree_node_open_debug_other) {
        imgui::SeparatorText("compute_drive_usage_color()");
        {
            static f32 s_fraction_used = 0.f;
            {
                ImVec4 color = compute_drive_usage_color(s_fraction_used);
                imgui::ScopedColor c1(ImGuiCol_FrameBg, color);
                imgui::ScopedColor c2(ImGuiCol_SliderGrab, color);
                imgui::ScopedColor c3(ImGuiCol_SliderGrabActive, color);
                imgui::ScopedColor c4(ImGuiCol_FrameBgHovered, color);
                imgui::ScopedAvailWidth w = {};
                imgui::SliderFloat("## Fraction", &s_fraction_used, 0.f, 1.f, "%.2f");
            }
            // s_fraction_used = std::clamp(s_fraction_used, 0.f, 1.f);

            // imgui::ScopedColor c(ImGuiCol_PlotHistogram, compute_drive_usage_color(s_fraction_used));
            // imgui::ProgressBar(s_fraction_used);
        }
        imgui::TreePop();
    }

    bool open_states_end[] = {
        expl.tree_node_open_debug_state,
        expl.tree_node_open_debug_memory,
        expl.tree_node_open_debug_performance,
        expl.tree_node_open_debug_other,
    };

    static_assert(lengthof(open_states_begin) == lengthof(open_states_end));

    if (memcmp(open_states_begin, open_states_end, lengthof(open_states_end)) != 0) {
        (void) expl.save_to_disk();
    }

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

    return true;
}

static
ImRect render_num_cwd_items_selected([[maybe_unused]] explorer_window &expl, cwd_count_info const &cnt) noexcept
{
    assert(cnt.child_dirents > 0);

    ImRect retval_text_rect;

    if (cnt.selected_directories == 0) {
        auto formatted_size = format_file_size(cnt.selected_files_size, global_state::settings().size_unit_multiplier);
        imgui::Text("%zu selections  %s", cnt.selected_dirents, formatted_size.data());
    } else {
        imgui::Text("%zu selections", cnt.selected_dirents);
    }

    retval_text_rect = imgui::GetItemRect();

    if (imgui::IsItemHovered({}, 1) && imgui::BeginTooltip()) {
        render_count_summary(cnt.selected_directories, cnt.selected_files, 0); // cnt.selected_symlinks
        imgui::SameLine();
        imgui::TextDisabled(ICON_FA_GRIP_LINES_VERTICAL);
        imgui::SameLine();
        imgui::TextUnformatted("Click to cycle");
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

        expl.scroll_to_nth_selected_entry_next_frame = expl.nth_last_cwd_dirent_scrolled;
    }

    return retval_text_rect;
}

static
void render_button_history_right(explorer_window &expl) noexcept
{
    auto io = imgui::GetIO();

    imgui::ScopedDisable disabled(expl.wd_history_pos == 0);
    imgui::ScopedItemFlag no_nav(ImGuiItemFlags_NoNav, true);

    if (imgui::Button(ICON_LC_ARROW_RIGHT "## expl.wd_history")) {
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

    if (imgui::IsItemHovered({}, 1)) {
        imgui::SetTooltip("Forward");
    }
}

static
void render_button_history_left(explorer_window &expl) noexcept
{
    u64 wd_history_last_idx = expl.wd_history.empty() ? 0 : expl.wd_history.size() - 1;
    auto io = imgui::GetIO();

    imgui::ScopedDisable disabled(expl.wd_history_pos == wd_history_last_idx);
    imgui::ScopedItemFlag no_nav(ImGuiItemFlags_NoNav, true);

    if (imgui::Button(ICON_LC_ARROW_LEFT "## expl.wd_history")) {
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

    if (imgui::IsItemHovered({}, 1)) {
        imgui::SetTooltip("Back");
    }
}

static
bool render_history_browser_popup(explorer_window &expl, bool cwd_exists) noexcept
{
    static std::string s_search_buf = {};
    bool search_text_edited;

    static s64 s_focus_idx = -1;
    bool set_focus = false;

    auto cleanup_and_close_popup = [&expl]() noexcept {
        s_focus_idx = -1;
        s_search_buf.clear();
        imgui::CloseCurrentPopup();
        imgui::ClearNavFocus();
    };
    SCOPE_EXIT { imgui::EndPopup(); };

    // when window is appearing
    if (imgui::IsWindowAppearing() && !imgui::IsAnyItemActive() && !imgui::IsMouseClicked(0)) {
        imgui::SetKeyboardFocusHere(); // set initial focus on search input
        s_focus_idx = -1;
        s_search_buf.clear();
    }

    if (imgui::IsKeyPressed(ImGuiKey_Tab)) {
        s64 min = 0, max = expl.wd_history.size() - 1;
        if (imgui::GetIO().KeyShift) dec_or_wrap(s_focus_idx, min, max);
        else inc_or_wrap(s_focus_idx, min, max);
        set_focus = true;
    }

    if (imgui::GetIO().KeyCtrl && imgui::IsKeyPressed(ImGuiKey_F)) {
        imgui::ActivateItemByID(imgui::GetID("## history search"));
        s_focus_idx = -1;
    }
    {
        imgui::ScopedItemWidth w(imgui::CalcTextSize("123456789_123456789_123456789_").x);
        search_text_edited = imgui::InputTextWithHint("## history search", ICON_CI_SEARCH " TODO", &s_search_buf);
    }
    imgui::SameLine();
    {
        imgui::ScopedDisable d(expl.wd_history.empty());
        imgui::ScopedItemFlag f(ImGuiItemFlags_NoNav, true);

        if (imgui::Button(ICON_CI_CLEAR_ALL "## explorer_window History")) {
            expl.wd_history.clear();
            expl.wd_history_pos = 0;

            if (cwd_exists) {
                expl.advance_history(expl.cwd);
            }

            expl.save_to_disk();
            cleanup_and_close_popup();
        }
    }
    if (imgui::IsItemHovered({}, 1)) {
        imgui::SetTooltip("Clear history");
    }

    imgui::SameLineSpaced(1);
    if (imgui::Button(ICON_LC_SQUARE_X)) {
        cleanup_and_close_popup();
    }
    imgui::SameLine();
    imgui::TextDisabled("or [Escape] to exit");

    s32 table_flags =
        ImGuiTableFlags_SizingStretchProp|
        ImGuiTableFlags_Resizable|
        ImGuiTableFlags_BordersV|
        // ImGuiTableFlags_Reorderable|
        ImGuiTableFlags_ScrollY|
        (global_state::settings().tables_alt_row_bg ? ImGuiTableFlags_RowBg : 0)|
        (global_state::settings().table_borders_in_body ? 0 : ImGuiTableFlags_NoBordersInBody)
    ;
    imgui::ScopedItemFlag no_nav(ImGuiItemFlags_NoNav, true);

    if (imgui::BeginTable("history_table", 4, table_flags)) {
        for (u64 i = 0; i < expl.wd_history.size(); ++i) {
            explorer_window::history_item const &hist_item = expl.wd_history[i];

            imgui::TableNextRow();

            imgui::TableNextColumn();
            if (i == expl.wd_history_pos) {
                imgui::TextDisabled(ICON_LC_MOVE_RIGHT);
            }

            imgui::TableNextColumn();
            imgui::Text("%3zu ", i + 1);

            imgui::TableNextColumn();
            {
                auto when = time_diff_str(hist_item.time_departed, get_time_system());
                imgui::TextUnformatted(when.data());
            }

            imgui::TableNextColumn();
            {
                auto label = make_str_static<1200>("%s ## %zu", hist_item.path.data(), i);
                bool pressed;
                {
                    char const *icon = get_icon(basic_dirent::kind::directory);
                    auto line = make_str_static<2048>("%s %s", icon, hist_item.path.data());
                    {
                        imgui::ScopedTextColor tc(directory_color());
                        imgui::TextUnformatted(icon);
                    }
                    imgui::SameLineSpaced(1);
                    pressed = imgui::Selectable(label.data(), false, ImGuiSelectableFlags_SpanAllColumns);
                    if (set_focus && !imgui::IsItemFocused() && s_focus_idx == s64(i)) {
                        imgui::FocusItem();
                    }
                    imgui::RenderTooltipWhenColumnTextTruncated(2, line.data(), 0, hist_item.path.data());
                }

                if (pressed) {
                    expl.wd_history_pos = i;
                    expl.cwd = expl.wd_history[i].path;

                    cleanup_and_close_popup();
                    imgui::EndTable();

                    return true;
                }
            }
        }

        imgui::EndTable();
    }

    return false;
}

static
bool render_filter_reset_button(explorer_window &expl) noexcept
{
    bool retval_cwd_entries_affected = false;

    {
        bool starting_filter = cstr_empty(expl.filter_text.data())
            && !expl.filter_case_sensitive
            && expl.filter_mode == explorer_window::filter_mode::contains
            && expl.filter_polarity == true
            && expl.filter_show_directories
            && expl.filter_show_files
            && expl.filter_show_invalid_symlinks
            && expl.filter_show_symlink_directories
            && expl.filter_show_symlink_files
        ;
        imgui::ScopedDisable d(starting_filter);
        imgui::ScopedItemFlag no_nav(ImGuiItemFlags_NoNav, true);

        if (imgui::Button(ICON_LC_SEARCH "## clear_filter")) {
            expl.reset_filter();
            (void) expl.update_cwd_entries(filter, expl.cwd.data());
            (void) expl.save_to_disk();
            retval_cwd_entries_affected = true;
        }
    }
    if (imgui::IsItemHovered({}, 1)) {
        imgui::SetTooltip("Click to restore default filter");
    }

    return retval_cwd_entries_affected;
}

static
void render_drives_table(explorer_window &expl, char dir_sep_utf8, u64 size_unit_multiplier) noexcept
{
    // refresh drives occasionally
    {
        time_point_precise_t now = get_time_precise();
        s64 diff_ms = time_diff_ms(expl.last_drives_refresh_time, now);
        if (diff_ms >= 1000) {
            expl.last_drives_refresh_time = get_time_precise();
            auto drives_info = query_available_drives_info();

            for (auto &d : expl.drives) {
                if (d.icon_GLtexID > 0) delete_icon_texture(d.icon_GLtexID, "drive_entry");
            }
            expl.drives.clear();

            // only repopulate drives found by `query_available_drives_info`, nullified ones won't get rendered
            for (auto const &di : drives_info) {
                if (di.letter == 0) continue;

                assert(di.letter >= 'A' && di.letter <= 'Z');
                char root[] = { di.letter, ':', dir_sep_utf8, '\0' };

                drive_entry d;
                d.info = di;
                std::tie(d.icon_GLtexID, d.icon_size) = load_icon_texture(root, nullptr, "drive_entry");
                expl.drives.push_back(d);
            }
        }
    }

    enum drive_table_col_id : s32
    {
        drive_table_col_id_number,
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
        ImGuiTableFlags_BordersV|
        ImGuiTableFlags_Reorderable|
        ImGuiTableFlags_Resizable|
        ImGuiTableFlags_ScrollY|
        (global_state::settings().table_borders_in_body ? 0 : ImGuiTableFlags_NoBordersInBody)|
        (global_state::settings().tables_alt_row_bg ? ImGuiTableFlags_RowBg : 0)
    ;
    imgui::ScopedItemFlag no_nav(ImGuiItemFlags_NoNav, true);

    if (imgui::BeginTable("## explorer_window drives_table", drive_table_col_id_count, table_flags)) {
        imgui::TableSetupColumn("#", ImGuiTableColumnFlags_NoSort, 0.0f, drive_table_col_id_number);
        imgui::TableSetupColumn("Drive", ImGuiTableColumnFlags_NoSort, 0.0f, drive_table_col_id_letter);
        imgui::TableSetupColumn("Name", ImGuiTableColumnFlags_NoSort, 0.0f, drive_table_col_id_name);
        imgui::TableSetupColumn("Filesystem", ImGuiTableColumnFlags_NoSort, 0.0f, drive_table_col_id_filesystem);
        imgui::TableSetupColumn("Total Space", ImGuiTableColumnFlags_NoSort, 0.0f, drive_table_col_id_total_space);
        imgui::TableSetupColumn("Usage", ImGuiTableColumnFlags_NoSort, 0.0f, drive_table_col_id_used_percent);
        imgui::TableSetupColumn("Free Space", ImGuiTableColumnFlags_NoSort, 0.0f, drive_table_col_id_free_space);
        ImGui::TableSetupScrollFreeze(0, 1);
        imgui::TableHeadersRow();

        // for (auto &drive : expl.drives) {
            // if (drive.info.letter == 0) continue;
        for (u64 i = 0; i < expl.drives.size(); ++i) {
            auto &drive = expl.drives[i];
            assert(drive.info.letter != 0);

            imgui::TableNextRow();

            if (imgui::TableSetColumnIndex(drive_table_col_id_number)) {
                imgui::Text("%zu", i+1);
            }

            if (imgui::TableSetColumnIndex(drive_table_col_id_letter)) {
                imgui::Image((ImTextureID)std::max(drive.icon_GLtexID, s64(0)), drive.icon_size);
                imgui::SameLine();
                imgui::Text("%C:", drive.info.letter);
            }

            if (imgui::TableSetColumnIndex(drive_table_col_id_name)) {
                bool selected = false;
                char const *label = cstr_empty(drive.info.name_utf8) ? "Unnamed Disk" : drive.info.name_utf8;

                if (imgui::Selectable(label, &selected, ImGuiSelectableFlags_SpanAllColumns)) {
                    char root[] = { drive.info.letter, ':', dir_sep_utf8, '\0' };
                    expl.cwd = expl.latest_valid_cwd = path_create(root);
                    expl.set_latest_valid_cwd(expl.cwd);
                    expl.advance_history(expl.cwd);
                    auto [drive_exists, _] = expl.update_cwd_entries(query_filesystem, expl.cwd.data());
                    if (drive_exists) {
                        (void) expl.update_cwd_entries(filter, expl.cwd.data());
                        (void) expl.save_to_disk();
                    } else {
                        // TODO: handle error
                    }
                }

                if (expl.tabbing_set_focus && expl.tabbing_focus_idx == s64(i) && !imgui::IsItemFocused()) {
                    expl.tabbing_set_focus = false;
                    imgui::FocusItem();
                }
            }

            if (imgui::TableSetColumnIndex(drive_table_col_id_filesystem)) {
                imgui::TextUnformatted(drive.info.filesystem_name_utf8);
            }

            if (imgui::TableSetColumnIndex(drive_table_col_id_total_space)) {
                auto total_space_formatted = format_file_size(drive.info.total_bytes, size_unit_multiplier);
                imgui::TextUnformatted(total_space_formatted.data());
            }

            if (imgui::TableSetColumnIndex(drive_table_col_id_free_space)) {
                auto free_space_formatted = format_file_size(drive.info.available_bytes, size_unit_multiplier);
                imgui::TextUnformatted(free_space_formatted.data());
            }

            if (imgui::TableSetColumnIndex(drive_table_col_id_used_percent)) {
                u64 used_bytes = drive.info.total_bytes - drive.info.available_bytes;
                f32 fraction_used = ( f32(used_bytes) / f32(drive.info.total_bytes) );
                f32 percent_used = fraction_used * 100.0f;
                imgui::Text("%3.0lf%%", percent_used);
                imgui::SameLine();

                ImVec4 drive_usage_color = compute_drive_usage_color(fraction_used);
                imgui::ScopedColor c(ImGuiCol_PlotHistogram, drive_usage_color);
                imgui::ProgressBar(f32(used_bytes) / f32(drive.info.total_bytes), ImVec2(-1, imgui::CalcTextSize("1").y), "");
            }
        }

        imgui::EndTable();
    }
}

struct render_filter_text_input_result
{
    bool focused;
    bool edited;
};
static
render_filter_text_input_result render_filter_text_input(explorer_window &expl, bool window_hovered, bool ctrl_key_down, cwd_count_info const &cnt, bool cwd_exists_after_edit, bool any_popups_open) noexcept
{
    render_filter_text_input_result retval = {};

    auto width = std::max(
        imgui::CalcTextSize(expl.filter_text.data()).x + (imgui::GetStyle().FramePadding.x * 2) + 10.f,
        imgui::CalcTextSize("1").x * 20.75f
    );

    ImVec4 border_color = imgui::GetStyleColorVec4(ImGuiCol_Border);
    if (bool valid_non_empty_cwd = !path_is_empty(expl.cwd) && cnt.child_dirents > 0) {
        if (!expl.filter_error.empty()) {
            border_color = error_color();
        }
        else if (cnt.filtered_dirents == cnt.child_dirents) {
            ImVec4 low_warning = warning_color(); low_warning.w /= 1.5;
            border_color = low_warning;
        }
    }

    auto hint = make_str_static<1024>("Filter %zu entries...", cnt.child_dirents);
    imgui::ScopedColor b(ImGuiCol_Border, border_color);
    imgui::ScopedItemWidth iw(width);

    if (imgui::InputTextWithHint("## explorer_window filter", hint.data(), expl.filter_text.data(), expl.filter_text.size())) {
        (void) expl.update_cwd_entries(filter, expl.cwd.data());
        (void) expl.save_to_disk();
    }
    retval.focused = imgui::IsItemFocused();

    bool show_all_filtered_msg = cwd_exists_after_edit && cnt.child_dirents > 0 && cnt.filtered_dirents == cnt.child_dirents;
    bool show_filter_error_msg = expl.filter_error != "";
    bool show_tooltip = show_filter_error_msg || show_all_filtered_msg;

    if (show_tooltip) {
        ImRect filter_text_input_rect = imgui::GetItemRect();
        ImVec2 tooltip_pos = ImVec2(
            filter_text_input_rect.GetTL().x,
            filter_text_input_rect.GetTL().y - imgui::GetTextLineHeight() - 2*imgui::GetStyle().FramePadding.y - imgui::GetStyle().ItemSpacing.y - imgui::GetStyle().WindowPadding.y
        );

        ImGuiWindowFlags tooltip_flags =
            ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoFocusOnAppearing |
            ImGuiWindowFlags_NoNav |
            ImGuiWindowFlags_NoInputs
        ;
        ImVec2 tooltip_size;

        imgui::ScopedColor border(ImGuiCol_Border, error_color());

        imgui::SetNextWindowPos(tooltip_pos);

        if (imgui::Begin("FilterTooltip", nullptr, tooltip_flags)) {
            if (show_filter_error_msg) {
                imgui::TextColored(error_color(), "Filter error: %s", expl.filter_error.c_str());
            } else if (show_all_filtered_msg) {
                imgui::TextColored(warning_color(), ICON_LC_MESSAGE_CIRCLE_WARNING " All items filtered");
            }

            tooltip_size = ImGui::GetWindowSize();
            imgui::End();
        }
        imgui::RenderInputBlockingOverlay(tooltip_pos, tooltip_size);
    }

    if (retval.focused && imgui::IsKeyPressed(ImGuiKey_Tab)) {
        if (expl.tabbing_focus_idx == -1) {
            expl.tabbing_focus_idx = 0;
        }
        expl.tabbing_set_focus = true;
    }

    if (!any_popups_open && window_hovered && ctrl_key_down && !path_is_empty(expl.cwd) && imgui::IsKeyPressed(ImGuiKey_F)) {
        imgui::ActivateItemByID(imgui::GetID("## explorer_window filter"));
        expl.tabbing_focus_idx = -1;
    }

    return retval;
}

static
bool render_filter_type_toggler_buttons(explorer_window &expl, cwd_count_info const &cnt) noexcept
{
    bool retval_cwd_entries_affected = false;

    std::tuple<basic_dirent::kind, bool &, char const *, u64, u64> button_defs[] = {
        { basic_dirent::kind::directory,             expl.filter_show_directories,          "Directories"         , cnt.child_directories , cnt.filtered_directories },
        { basic_dirent::kind::file,                  expl.filter_show_files,                "Files"               , cnt.child_files       , cnt.filtered_files },

        // { basic_dirent::kind::symlink_to_file,       expl.filter_show_symlink_files,        "file shortcuts"      , },
        // { basic_dirent::kind::symlink_to_directory,  expl.filter_show_symlink_directories,  "directory shortcuts" , },
        // { basic_dirent::kind::invalid_symlink,       expl.filter_show_invalid_symlinks,     "invalid shortcuts"   , },
    };

    for (auto &button_def : button_defs) {
        basic_dirent::kind type = std::get<0>(button_def);
        bool &show              = std::get<1>(button_def);
        char const *type_str    = std::get<2>(button_def);
        u64 type_cnt            = std::get<3>(button_def);
        u64 type_cnt_filtered   = std::get<4>(button_def);

        imgui::SameLine(0, 0);

        {
            imgui::ScopedItemFlag no_nav(ImGuiItemFlags_NoNav, true);
            if (show) {
                auto btn_label = make_str_static<64>("%s %zu/%zu", get_icon(type), type_cnt - type_cnt_filtered, type_cnt);
                imgui::Button(btn_label.data());
            } else {
                auto btn_label = make_str_static<64>("%s 0/%zu", get_icon(type), type_cnt);
                imgui::Button(btn_label.data());
            }
        }

        if (imgui::IsItemHovered({}, 1)) {
            char const *filter_desc = expl.filter_text.data();
            auto contains_desc = make_str_static<1024>("%s", expl.filter_text.data());
            auto regex_desc = make_str_static<1024>("/^%s$/", expl.filter_text.data());
            switch (expl.filter_mode) {
                case explorer_window::filter_mode::contains: filter_desc = contains_desc.data(); break;
                case explorer_window::filter_mode::regex_match: filter_desc = regex_desc.data(); break;
                default: break;
            }

            auto filter_explanation = make_str_static<2048>("%zu elements are filtered by [%s].\n", type_cnt_filtered, filter_desc);

            imgui::SetTooltip(
                "%s are [%s]\n"
                "%s\n"
                "[      L click] to %s %s\n"
                "[Ctrl  L click] to show %s only\n"
                "[Shift L click] to unhide all types",

                type_str, (show ? "visible" : "hidden"),
                (strlen(expl.filter_text.data()) > 0 ? filter_explanation.data() : ""),
                (show ? "hide" : "unhide"), type_str,
                type_str
            );
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
            retval_cwd_entries_affected = true;
        }
    }
    return retval_cwd_entries_affected;
}

static
bool render_filter_mode_button(explorer_window &expl) noexcept
{
    bool retval_cwd_entries_affected = false;

    static char const *s_filter_modes[] = {
         ICON_CI_WHOLE_WORD, // ICON_FA_FONT,
         ICON_CI_REGEX, // ICON_FA_ASTERISK,
        // "(" ICON_CI_REGEX ")",
    };

    static_assert(lengthof(s_filter_modes) == (u64)explorer_window::filter_mode::count);

    static char const *s_current_mode = nullptr;
    s_current_mode = s_filter_modes[expl.filter_mode];

    auto label = make_str_static<64>("%s""## %zu", s_current_mode, expl.filter_mode);

    imgui::ScopedItemFlag no_nav(ImGuiItemFlags_NoNav, true);

    if (imgui::Button(label.data())) {
        inc_or_wrap<u64>((u64 &)expl.filter_mode, 0, u64(explorer_window::filter_mode::count) - 1);
        (void) expl.update_cwd_entries(filter, expl.cwd.data());
        (void) expl.save_to_disk();
        retval_cwd_entries_affected = true;
    }

    if (imgui::IsItemHovered({}, 1)) {
        char const *mode = nullptr;
        switch (expl.filter_mode) {
            case explorer_window::filter_mode::contains: mode = "CONTAINS"; break;
            case explorer_window::filter_mode::regex_match: mode = "REGEXP_MATCH"; break;
            // case explorer_window::filter_mode::regex_find: mode = "REGEXP_FIND"; break;
            default: break;
        }
        imgui::SetTooltip("Mode: %s\n", mode);
    }

    return retval_cwd_entries_affected;
}

static
bool render_filter_case_sensitivity_button(explorer_window &expl) noexcept
{
    bool retval_cwd_entries_affected = false;
    {
        imgui::ScopedStyle<f32> s(imgui::GetStyle().Alpha, expl.filter_case_sensitive ? 1 : imgui::GetStyle().DisabledAlpha);
        imgui::ScopedItemFlag no_nav(ImGuiItemFlags_NoNav, true);

        if (imgui::Button(ICON_LC_CASE_SENSITIVE)) { // ICON_FA_CROSSHAIRS
            flip_bool(expl.filter_case_sensitive);
            (void) expl.update_cwd_entries(filter, expl.cwd.data());
            (void) expl.save_to_disk();
            retval_cwd_entries_affected = true;
        }
    }
    if (imgui::IsItemHovered({}, 1)) {
        imgui::SetTooltip("Filter case %s", expl.filter_case_sensitive ? "sensitive" : "insensitive");
    }
    return retval_cwd_entries_affected;
}

static
bool render_filter_polarity_button(explorer_window &expl) noexcept
{
    bool retval_cwd_entries_affected = false;

    imgui::ScopedItemFlag no_nav(ImGuiItemFlags_NoNav, true);

    if (imgui::Button(expl.filter_polarity ? (ICON_LC_FILE_CHECK_2 "## filter_polarity") : (ICON_LC_FILE_MINUS_2 "## filter_polarity"))) {
        flip_bool(expl.filter_polarity);
        (void) expl.update_cwd_entries(filter, expl.cwd.data());
        (void) expl.save_to_disk();
        retval_cwd_entries_affected = true;
    }
    if (imgui::IsItemHovered({}, 1)) {
        imgui::SetTooltip("%s matches", expl.filter_polarity ? "Showing" : "Hiding");
    }

    return retval_cwd_entries_affected;
}

static
void render_button_pin_cwd(explorer_window &expl, bool cwd_exists) noexcept
{
    u64 pin_idx;
    {
        scoped_timer<timer_unit::MICROSECONDS> check_if_pinned_timer(&expl.check_if_pinned_us);
        pin_idx = global_state::pinned_find_idx(expl.cwd);
    }
    bool already_pinned = pin_idx != std::string::npos;

    auto label = make_str_static<8>("%s", already_pinned ? ICON_LC_BOOKMARK_CHECK : ICON_LC_BOOKMARK_PLUS);

    imgui::ScopedDisable disabled(!cwd_exists && !already_pinned);
    imgui::ScopedItemFlag no_nav(ImGuiItemFlags_NoNav, true);

    if (imgui::Button(label.data())) {
        if (already_pinned) {
            imgui::OpenConfirmationModalWithCallback(
                /* confirmation_id  = */ swan_id_confirm_explorer_unpin_directory,
                /* confirmation_msg = */
                [pin_idx]() noexcept {
                    auto const &pin = global_state::pinned_get()[pin_idx];

                    imgui::TextUnformatted("Are you sure you want to delete this bookmark?");
                    imgui::Spacing(2);

                    imgui::SameLineSpaced(1); // indent
                    imgui::TextUnformatted(pin.label.c_str());
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
        (void) global_state::pinned_save_to_disk();
    }
    if (imgui::IsItemHovered({}, 1)) {
        imgui::SetTooltip("%s current working directory", already_pinned ? "Unbookmark" : "Bookmark");
    }
}

static
bool render_history_browser_button() noexcept
{
    imgui::ScopedItemFlag no_nav(ImGuiItemFlags_NoNav, true);

    if (imgui::Button(ICON_LC_HISTORY "## expl.wd_history")) {
        return true;
    }
    if (imgui::IsItemHovered({}, 1)) {
        imgui::SetTooltip("Open history");
    }
    return false;
}

static
void render_up_to_cwd_parent_button(explorer_window &expl, bool cwd_exists_before_edit) noexcept
{
    imgui::ScopedDisable disabled(!cwd_exists_before_edit);
    imgui::ScopedItemFlag no_nav(ImGuiItemFlags_NoNav, true);

    if (imgui::Button(ICON_LC_MOVE_UP "## up")) { // ICON_FA_ARROW_UP
        // print_debug_msg("[ %d ] (..) button triggered", expl.id);

        auto result = try_ascend_directory(expl);

        if (!result.success) {
            bool cwd_is_drive = path_drive_like(expl.cwd.data(), path_length(expl.cwd));
            if (cwd_is_drive) {
                path_clear(expl.cwd);
            } else {
                std::string action = make_str("Ascend to directory [%s]", result.parent_dir.data());
                char const *failed = "Directory not found.";
                swan_popup_modals::open_error(action.c_str(), failed);
            }
        }
    }

    if (imgui::IsItemHovered({}, 1)) {
        imgui::SetTooltip("Up to parent directory");
    }
}

static
void render_help_icon(explorer_window &expl) noexcept
{
    auto help = render_help_indicator(true);

    if (help.hovered && !imgui::IsPopupOpen("## explorer help")) {
        imgui::SetTooltip("Left click for help, right click for debug info");
    }
    if (help.left_clicked) {
        imgui::OpenPopup("## explorer help");
        imgui::SetNextWindowSizeConstraints(ImVec2(200, 200), ImVec2(imgui::GetMainViewport()->Size.x, 800));
    }
    if (help.right_clicked) {
        bool *open = &global_state::settings().show.explorer_0_debug + (u64)expl.id;
        flip_bool(*open);
    }

    if (imgui::BeginPopup("## explorer help", ImGuiWindowFlags_NoSavedSettings)) {
        static std::string s_search_input = {};

        imgui::AlignTextToFramePadding();
        imgui::TextUnformatted("[Explorer] Help");
        imgui::SameLineSpaced(1);
        bool search_edited = imgui::InputTextWithHint("## explorer help search", ICON_CI_SEARCH, &s_search_input);

        imgui::Separator();
        imgui::Spacing();
        {
            ImGuiTableFlags table_flags =
                ImGuiTableFlags_SizingFixedFit|
                ImGuiTableFlags_BordersV|
                (global_state::settings().tables_alt_row_bg ? ImGuiTableFlags_RowBg : 0)|
                (global_state::settings().table_borders_in_body ? 0 : ImGuiTableFlags_NoBordersInBody)
            ;
            imgui::ScopedItemFlag no_nav(ImGuiItemFlags_NoNav, true);

            if (imgui::BeginTable("## explorer_window help_icon_table", 3, table_flags)) {
                struct keybind_cell
                {
                    char const *const content = nullptr;
                    u64 highlight_start_idx = 0;
                    u64 highlight_len = 0;
                };
                struct keybind_row
                {
                    keybind_cell keybind;
                    keybind_cell action;
                    keybind_cell description;
                };
                static keybind_row s_table_row_data[] = {
                    { {"Ctrl F"}, {"Filter"}, {"Toggle controls to filter (hide/find) entries in the current working directory."} },
                    { {"Ctrl Shift F"}, {"Finder"}, {"Open current working directory in Finder."} },
                    { {"Ctrl Shift A F"}, {"Add to Finder"}, {"Add current working directory to Finder."} },
                    { {"Ctrl C"}, {"Copy selected entries to clipboard"}, {"Add copy operation to the clipboard for each selected entry in the current working directory."} },
                    { {"Ctrl X"}, {"Cut selected entries to clipboard"}, {"Add move operation to the clipboard for each selected entry in the current working directory."} },
                    { {"Ctrl V"}, {"Paste"}, {"Execute operations from the clipboard into the current working directory."} },
                    { {"Del"}, {"Delete selected entries"}, {"Initiate revertable delete operation for all selected entries in the current working directory, including filtered ones."} },
                    { {"Escape"}, {"Deselect all entries"}, {"Deselect all entries in the current working directory, including filtered ones."} },
                    { {"Ctrl A"}, {"Select all visible entries"}, {"Select all visible entries in the current working directory."} },
                    { {"Ctrl I"}, {"Invert selection"}, {"Invert selection state of all visible entries in the current working directory."} },
                    { {"Ctrl R, F2"}, {"Rename selected entries"}, {"Open bulk rename modal if multiple entries are selected, or the simpler single rename modal if only one entry is selected."} },
                    { {"Ctrl N F"}, {"New file"}, {"Open modal for creating a new empty file."} },
                    { {"Ctrl N D"}, {"New directory"}, {"Open modal for creating a new empty directory."} },
                    { {"Ctrl H"}, {"Open history"}, {"Open history modal where you can view and navigate to previous working directories."} },
                    { {"Ctrl P"}, {"Bookmark current working directory"}, {"Open modal to bookmark the current working directory, or modify the existing bookmark."} },
                    { {"Ctrl O"}, {"Open bookmarks"}, {"Open modal where bookmarked directories can be accessed."} },
                };

                imgui::TableSetupColumn("KEYBIND", {}, {}, 0);
                imgui::TableSetupColumn("ACTION", {}, {}, 1);
                imgui::TableSetupColumn("DESCRIPTION", {}, {}, 2);
                imgui::TableHeadersRow();

                for (auto &table_row : s_table_row_data) {
                    imgui::TableNextRow();

                    auto render_cell = [search_edited](keybind_cell &cell) noexcept {
                        if (search_edited) {
                            char const *match = StrStrIA(cell.content, s_search_input.c_str());
                            if (!match) {
                                cell.highlight_start_idx = cell.highlight_len = 0;
                            } else {
                                cell.highlight_start_idx = std::distance(cell.content, match);
                                cell.highlight_len = s_search_input.size();
                            }
                        }
                        imgui::TableNextColumn();
                        imgui::TextUnformatted(cell.content);
                        if (cell.highlight_len > 0) {
                            ImVec2 content_rect_TL = imgui::GetItemRectMin();
                            imgui::HighlightTextRegion(content_rect_TL, cell.content, cell.highlight_start_idx, cell.highlight_len,
                                                    imgui::ReduceAlphaTo(imgui::Denormalize(warning_lite_color()), 75));
                        }
                    };
                    render_cell(table_row.keybind);
                    render_cell(table_row.action);
                    render_cell(table_row.description);
                }

                imgui::EndTable();
            }
        }

        struct line
        {
            char const *const content = nullptr;
            u64 highlight_start_idx = 0;
            u64 highlight_len = 0;
        };
        auto render_line = [search_edited](line &l) noexcept {
            if (search_edited) {
                char const *match = StrStrIA(l.content, s_search_input.c_str());
                if (!match) {
                    l.highlight_start_idx = l.highlight_len = 0;
                } else {
                    l.highlight_start_idx = std::distance(l.content, match);
                    l.highlight_len = s_search_input.size();
                }
            }

            imgui::TextUnformatted(l.content);

            if (l.highlight_len > 0) {
                ImVec2 content_rect_TL = imgui::GetItemRectMin();
                imgui::HighlightTextRegion(content_rect_TL, l.content, l.highlight_start_idx, l.highlight_len,
                                           imgui::ReduceAlphaTo(imgui::Denormalize(warning_lite_color()), 75));
            }
        };

        imgui::Spacing();
        {
            static line s_lines[] = {
                {" - Left click an entry to toggle it's selection state (all other entries will be deselected)."},
                {" - Shift left click an entry to select a range starting from previously selected entry, or the first entry if none have been selected yet."},
                {" - Ctrl left click an entry to toggle it's selection state without altering other selections."},
                {" - Double left click an entry to open it."},
                {" - Right click an entry to open the context menu for selected entries, or the hovered entry if none are selected."},
            };
            imgui::TextUnformatted("[General Navigation]");
            for (auto &l : s_lines) {
                render_line(l);
            }
        }
        imgui::Spacing();
        {
            static line s_lines[] = {
                {" - Entry: Generic term for a filesystem object - directory, file, or symlink."},
                {" - Directory: Known as a folder in the Windows File Explorer. Contains other entries."},
                {" - File: Anything that isn't a directory or symlink. Contains data."},
                {" - Symlink: Known as a shortcut in the Windows File Explorer. Points to an entry, possibly in a different directory."},
            };
            imgui::TextUnformatted("[Terminology]");
            for (auto &l : s_lines) {
                render_line(l);
            }
        }
        imgui::Spacing();
    #if 0
        {
            // static line s_lines[] = {
            //     {},
            // };
            imgui::TextUnformatted("[Tips]");
            // for (auto &l : s_lines) {
            //     render_line(l);
            // }
        }
        imgui::Spacing();
    #endif
        imgui::EndPopup();
    }
}

struct render_cwd_text_input_result
{
    bool is_hovered;
    bool edit_occurred;
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

    cwd_text_input_callback_user_data user_data = {
        .expl_id = expl.id,
        .dir_sep_utf16 = dir_sep_utf16,
        .edit_occurred = false,
        .text_content = s_cwd_input.data(),
    };

    ImGuiInputTextState *input_text_state = nullptr;
    {
        ImVec4 low_warning = warning_color(); low_warning.w /= 1.5;
        imgui::ScopedColor b(ImGuiCol_Border, low_warning, !path_is_empty(expl.cwd) && !cwd_exists_before_edit);
        imgui::ScopedAvailWidth w(avail_width_subtract_amt);
        imgui::ScopedItemFlag no_nav(ImGuiItemFlags_NoNav, true);

        auto label = make_str_static<64>("## cwd expl_%d", expl.id);

        is_input_text_enter_pressed = imgui::InputTextWithHint(label.data(), "Current working directory", s_cwd_input.data(), s_cwd_input.size(),
            ImGuiInputTextFlags_CallbackCharFilter|ImGuiInputTextFlags_CallbackEdit|ImGuiInputTextFlags_EnterReturnsTrue,
            cwd_text_input_callback, (void *)&user_data);

        retval.is_hovered = imgui::IsItemHovered();

        ImGuiID id = imgui::GetCurrentWindow()->GetID(label.data());

        if (imgui::GetIO().KeyCtrl && imgui::IsKeyPressed(ImGuiKey_D)) {
            imgui::ActivateItemByID(id);
            expl.tabbing_focus_idx = -1;
        }
        input_text_state = imgui::GetInputTextState(id);
    }
    expl.cwd_input_text_scroll_x = input_text_state ? input_text_state->ScrollX : -1;

    retval.edit_occurred = user_data.edit_occurred;

    if (user_data.edit_occurred) {
        bool path_functionally_diff = !path_loosely_same(expl.cwd, s_cwd_input);

        expl.cwd = path_squish_adjacent_separators(s_cwd_input);
        path_force_separator(expl.cwd, dir_sep_utf8);

        if (path_functionally_diff) {
            auto [cwd_exists, _] = expl.update_cwd_entries(query_filesystem, expl.cwd.data());
            cwd_exists_after_edit = cwd_exists;

            if (cwd_exists_after_edit && !path_is_empty(expl.cwd)) {
                expl.cwd = path_reconstruct_canonically(expl.cwd.data());
                expl.advance_history(expl.cwd);
                expl.set_latest_valid_cwd(expl.cwd); // this may mutate filter
                (void) expl.update_cwd_entries(filter, expl.cwd.data());
            }
        }

        (void) expl.save_to_disk();
    }

#if 0
    if (cwd_exists_after_edit) {
        if (imgui::BeginDragDropTargetCustom({}, 0)) {
            // TODO
            // 1. Determine which segment is being hovered (consider expl.cwd_input_text_scroll_x)
            // 2. Draw rectangle around segment
            // 3. If mouse1 released, move selected to segment using move_files_into()
        }
    }
#endif

    return retval;
}

bool swan_windows::render_explorer(explorer_window &expl, bool &open, finder_window &finder, bool any_popups_open) noexcept
{
#if 0
    // Get the current window's dock node
    ImGuiWindow* window = ImGui::FindWindowByName(expl.name);
    if (window && window->DockNode)
    {
        ImGuiDockNode* dockNode = window->DockNode;

        // Check if the dock node has a tab bar
        if (dockNode->TabBar)
        {
            ImGuiTabBar* tabBar = dockNode->TabBar;

            // Iterate through the tabs
            for (int tabIndex = 0; tabIndex < tabBar->Tabs.Size; ++tabIndex) {
                const ImGuiTabItem* tab = &tabBar->Tabs[tabIndex];

                // Calculate the tab's rectangle
                ImVec2 tabPos = ImVec2(tabBar->BarRect.Min.x + tab->Offset, tabBar->BarRect.Min.y);
                ImVec2 tabSize = ImVec2(tab->Width, tabBar->BarRect.GetHeight());
                ImRect tabRect(tabPos, ImVec2(tabPos.x + tabSize.x, tabPos.y + tabSize.y));

                if (!cstr_eq(tab->Window->Name, expl.name)) {
                    continue;
                }

                // imgui::GetForegroundDrawList()->AddRect(tabRect.Min, tabRect.Max, IM_COL32(255,0,0,255));

                // Check if the mouse is hovering over the tab
                if (tab->Window->Hidden && ImGui::IsMouseHoveringRect(tabRect.Min, tabRect.Max, false) && !path_is_empty(expl.cwd)) {
                    // imgui::SetNextWindowPos(tabRect.GetBL());
                    ImGui::BeginTooltip();
                    render_path_with_stylish_separators(expl.cwd.data());
                    ImGui::EndTooltip();
                    break;
                }
            }
        }
    }
#endif

    imgui::SetNextWindowSize({ 1280, 720 }, ImGuiCond_Appearing);

    if (!imgui::Begin(expl.name, &open, ImGuiWindowFlags_NoCollapse)) {
        return false;
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

    std::optional<swan_path> descend_target = std::nullopt;
    std::optional<swan_path> open_target = std::nullopt;
    bool do_ascend = false;

    auto label_child = make_str_static<64>("%s ## main_child", expl.name);
    if (imgui::BeginChild(label_child.data(), {}, false)) {

    if (!any_popups_open) {
        // handle most keybinds here. The redundant checks for `window_focused` and `io.KeyCtrl` are intentional,
        // a tiny performance penalty for much easier to read code.

        bool window_focused_or_hovered = window_focused || window_hovered;

        if (window_focused_or_hovered && imgui::IsKeyPressed(ImGuiKey_Delete)) {
            u64 num_entries_selected = std::count_if(expl.cwd_entries.begin(), expl.cwd_entries.end(),
                                                     [](explorer_window::dirent const &e) noexcept { return e.selected; });

            if (num_entries_selected > 0) {
                imgui::OpenConfirmationModalWithCallback(
                    /* confirmation_id  = */ swan_id_confirm_explorer_execute_delete,
                    /* confirmation_msg = */ make_str("Are you sure you want to delete %zu file%s?",
                                                      num_entries_selected, pluralized(num_entries_selected, "", "s")).c_str(),
                    /* on_yes_callback  = */
                    [&]() noexcept {
                        auto result = delete_selected_entries(expl, global_state::settings());

                        if (!result.success) {
                            char const *action = "Delete items.";
                            char const *failed = result.error_or_utf8_path.c_str();
                            swan_popup_modals::open_error(action, failed);
                        }
                        (void) global_state::settings().save_to_disk();
                    },
                    /* confirmation_enabled = */ &(global_state::settings().confirm_explorer_delete_via_keybind)
                );
            }
        }
        else if (window_focused_or_hovered && (imgui::IsKeyPressed(ImGuiKey_F2) || (io.KeyCtrl && imgui::IsKeyPressed(ImGuiKey_R)))) {
            u64 num_entries_selected = std::count_if(expl.cwd_entries.begin(), expl.cwd_entries.end(),
                                                     [](explorer_window::dirent const &e) noexcept { return e.selected; });

            if (num_entries_selected == 0) {
                if (expl.tabbing_focus_idx >= 0) {
                    open_single_rename_popup = true;
                    s_dirent_to_be_renamed = &expl.cwd_entries[expl.tabbing_focus_idx];
                }
                else {
                    // TODO notification
                    // swan_popup_modals::open_error("Keybind for rename was pressed.", "Nothing is selected.");
                }
            }
            else if (num_entries_selected == 1) {
                auto selected_dirent = std::find_if(expl.cwd_entries.begin(), expl.cwd_entries.end(),
                                                    [](explorer_window::dirent const &e) noexcept { return e.selected; });
                open_single_rename_popup = true;
                s_dirent_to_be_renamed = &(*selected_dirent);
            }
            else if (num_entries_selected > 1) {
                open_bulk_rename_popup = true;
            }
        }
        else if (window_focused_or_hovered && io.KeyCtrl && imgui::IsKeyPressed(ImGuiKey_H)) {
            imgui::OpenPopup("History");
        }
        else if (window_focused_or_hovered && io.KeyCtrl && imgui::IsKeyPressed(ImGuiKey_U)) {
            auto result = try_ascend_directory(expl);

            if (!result.success) {
                bool cwd_is_drive = path_drive_like(expl.cwd.data(), path_length(expl.cwd));
                if (cwd_is_drive) {
                    path_clear(expl.cwd);
                } else {
                    std::string action = make_str("Ascend to directory [%s]", result.parent_dir.data());
                    char const *failed = "Directory not found.";
                    swan_popup_modals::open_error(action.c_str(), failed);
                }
            }
        }
        else if (window_focused_or_hovered && io.KeyCtrl && imgui::IsKeyDown(ImGuiKey_N) && imgui::IsKeyPressed(ImGuiKey_F)) {
            swan_popup_modals::open_new_file(expl.cwd.data(), expl.id);
        }
        else if (window_focused_or_hovered && io.KeyCtrl && imgui::IsKeyDown(ImGuiKey_N) && imgui::IsKeyPressed(ImGuiKey_D)) {
            swan_popup_modals::open_new_directory(expl.cwd.data(), expl.id);
        }
        else if (window_focused_or_hovered && io.KeyCtrl && io.KeyShift && imgui::IsKeyPressed(ImGuiKey_F)) {
            if (cwd_exists_before_edit) {
                if (finder.search_task.active_token.load() == true) {
                    finder.search_task.cancellation_token.store(true);
                }

                if (!imgui::IsKeyDown(ImGuiKey_A)) {
                    finder.search_directories.clear();
                }
                finder.search_directories.push_back({ cwd_exists_before_edit, expl.cwd });

                cstr_clear(finder.search_value.data());
                finder.focus_search_value_input = true;

                global_state::settings().show.finder = true;
                (void) global_state::settings().save_to_disk();

                imgui::SetWindowFocus(swan_windows::get_name(swan_windows::id::finder));
            }
        }
        else if (window_focused_or_hovered && io.KeyCtrl && imgui::IsKeyPressed(ImGuiKey_F)) {
            flip_bool(expl.show_filter_window);
        }
        else if (window_focused_or_hovered && io.KeyCtrl && imgui::IsKeyPressed(ImGuiKey_P)) {
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
        else if (window_focused_or_hovered && io.KeyCtrl && imgui::IsKeyPressed(ImGuiKey_O)) {
            imgui::OpenPopup("Bookmarks");
        }
    }

    if (imgui::IsPopupOpen("Bookmarks")) {
        ImVec2 avail = imgui::GetContentRegionAvail();
        avail.y -= imgui::GetStyle().WindowPadding.y*10;
        imgui::SetNextWindowPos(base_window_pos, ImGuiCond_Always);
        imgui::SetNextWindowSize(imgui::GetWindowContentRegionMax(), ImGuiCond_Always);
    }
    if (imgui::BeginPopupModal("Bookmarks", nullptr, ImGuiWindowFlags_NoResize)) {
        static pinned_path *s_context_target = nullptr;
        auto [open_target_, close_btn] = render_pinned(s_context_target, true);

        if (imgui::IsKeyPressed(ImGuiKey_Escape) || close_btn || open_target_) {
            imgui::CloseCurrentPopup();
            imgui::ClearNavFocus();
        }

        if (open_target_) {
            if (!directory_exists(open_target_->path.data())) {
                std::string action = make_str("Open bookmark [%s].", open_target_->label.c_str());
                std::string failed = make_str("Bookmark path [%s] does not exit.", open_target_->path.data());
                swan_popup_modals::open_error(action.c_str(), failed.c_str());
            }
            else {
                auto [pin_is_valid_dir, _] = expl.update_cwd_entries(query_filesystem, open_target_->path.data());
                if (pin_is_valid_dir) {
                    expl.cwd = open_target_->path;
                    expl.advance_history(open_target_->path);
                    expl.set_latest_valid_cwd(open_target_->path); // this may mutate filter
                    (void) expl.update_cwd_entries(filter, open_target_->path.data());
                    (void) expl.save_to_disk();
                } else {
                    (void) expl.update_cwd_entries(full_refresh, expl.cwd.data());
                }
            }
        }
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
                        // print_debug_msg("[ %d ] SUCCESS ReadDirectoryChangesW [%s]", expl.id, expl.cwd.data());
                    } else {
                        print_debug_msg("[ %d ] FAILED ReadDirectoryChangesW: %s", expl.id, get_last_winapi_error().formatted_message.c_str());
                    }
                }
            };

            if (expl.read_dir_changes_refresh_request_time != time_point_precise_t() &&
                time_diff_ms(expl.last_filesystem_query_time, get_time_precise()) >= 250)
            {
                refresh(full_refresh);
                expl.read_dir_changes_refresh_request_time = time_point_precise_t();
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
                        issue_read_dir_changes();
                        if (expl.read_dir_changes_refresh_request_time == time_point_precise_t()) {
                            // no refresh pending, submit request to refresh
                            expl.read_dir_changes_refresh_request_time = get_time_precise();
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

    auto do_counting = [](explorer_window const &expl) noexcept -> cwd_count_info {
        // print_debug_msg("[ %d ] do_counting [%s]", expl.id, expl.cwd.data());

        cwd_count_info cnt = {};

        for (auto &dirent : expl.cwd_entries) {
            static_assert(u64(false) == 0);
            static_assert(u64(true)  == 1);

            [[maybe_unused]] char const *path = dirent.basic.path.data();

            bool is_path_dotdot = dirent.basic.is_path_dotdot();
            bool is_dir = dirent.basic.is_directory() || dirent.basic.is_symlink_to_directory();
            bool is_file = dirent.basic.is_file() || dirent.basic.is_symlink_to_file();

            cnt.filtered_directories += u64(dirent.filtered && is_dir);
            cnt.filtered_files       += u64(dirent.filtered && is_file);

            cnt.child_dirents     += 1; // u64(!is_path_dotdot);
            cnt.child_directories += u64(dirent.basic.is_directory() || dirent.basic.is_symlink_to_directory());
            cnt.child_files       += u64(is_file || dirent.basic.is_symlink_ambiguous());

            // cnt.filtered_symlinks += u64(dirent.filtered && dirent.basic.is_symlink());
            // cnt.child_symlinks += u64(dirent.basic.is_symlink());

            if (!dirent.filtered && dirent.selected) {
                cnt.selected_directories += u64(dirent.selected && is_dir && !is_path_dotdot);
                cnt.selected_files       += u64(dirent.selected && is_file);
                cnt.selected_files_size  += u64(is_file) * dirent.basic.size;

                // cnt.selected_symlinks += u64(dirent.selected && dirent.basic.is_symlink());
            }
        }

        cnt.filtered_dirents = cnt.filtered_directories + cnt.filtered_files; // + cnt.filtered_symlinks
        cnt.selected_dirents = cnt.selected_directories + cnt.selected_files; // + cnt.selected_symlinks

        return cnt;
    };

    cwd_count_info cnt = do_counting(expl);

    // header controls
    {
        render_button_history_left(expl);

        imgui::SameLine(0, 0);

        render_button_history_right(expl);

        imgui::SameLine(0, 0);

        render_up_to_cwd_parent_button(expl, cwd_exists_before_edit);

        // imgui::SameLine(0, style.ItemSpacing.x / 2);
        imgui::SameLine();

        auto [_, cwd_edited] = render_cwd_text_input(expl, cwd_exists_after_edit, dir_sep_utf8, dir_sep_utf16, 0, cwd_exists_before_edit);

        if (cwd_edited) { // recount
            cnt = do_counting(expl);
        }

    #if 1
        bool show_dir_not_found_msg = !cwd_exists_after_edit;
        bool show_empty_dir_msg = (cwd_exists_after_edit && expl.cwd_entries.empty());
        bool show_tooltip = show_dir_not_found_msg || show_empty_dir_msg;

        if (show_tooltip) {
            ImRect cwd_text_input_rect = imgui::GetItemRect();
            ImVec2 tooltip_pos = ImVec2(
                cwd_text_input_rect.GetBL().x,
                cwd_text_input_rect.GetBL().y + ImGui::GetStyle().ItemSpacing.y
            );

            // First pass: draw the actual tooltip
            ImGui::SetNextWindowPos(tooltip_pos);

            ImGuiWindowFlags tooltip_flags = ImGuiWindowFlags_AlwaysAutoResize |
                                             ImGuiWindowFlags_NoDecoration |
                                             ImGuiWindowFlags_NoSavedSettings |
                                             ImGuiWindowFlags_NoFocusOnAppearing |
                                             ImGuiWindowFlags_NoNav |
                                             ImGuiWindowFlags_NoInputs;

            ImVec2 tooltip_size;
            {
                imgui::ScopedColor border(ImGuiCol_Border, warning_color());
                imgui::SetNextWindowSize({ cwd_text_input_rect.GetWidth(), -1 });

                if (ImGui::Begin("CustomTooltip", nullptr, tooltip_flags)) {
                    if (show_empty_dir_msg) {
                        imgui::TextColored(warning_color(), ICON_LC_MESSAGE_CIRCLE_WARNING " Empty directory");
                    }
                    if (show_dir_not_found_msg) {
                        imgui::TextColored(warning_color(), ICON_LC_MESSAGE_CIRCLE_WARNING " Directory not found" );
                    }

                    tooltip_size = ImGui::GetWindowSize();
                    ImGui::End();
                }
            }
            imgui::RenderInputBlockingOverlay(tooltip_pos, tooltip_size);
        }
    #endif
    }

    bool b_render_drives_table = path_is_empty(expl.cwd);

    if (window_hovered && !expl.filter_text_input_focused && !any_popups_open) {
        if (imgui::IsKeyPressed(ImGuiKey_Tab)) {
            if (expl.tabbing_focus_idx == -1) {
                expl.tabbing_focus_idx = 0;
            } else {
                s64 min = 0;
                s64 max = b_render_drives_table
                    ? (expl.drives.size() - 1)
                    : std::distance(expl.cwd_entries.begin(), expl.first_filtered_cwd_dirent_iter) - 1
                ;
                if (imgui::GetIO().KeyShift) dec_or_wrap(expl.tabbing_focus_idx, min, max);
                else inc_or_wrap(expl.tabbing_focus_idx, min, max);
            }
            expl.tabbing_set_focus = true;
        }
    }

    if (b_render_drives_table) {
        imgui::NewLine();
        render_drives_table(expl, dir_sep_utf8, size_unit_multiplier);
    }
    else {
        for (auto &drive : expl.drives)
            if (drive.icon_GLtexID > 0)
                delete_icon_texture(drive.icon_GLtexID, "explorer_window::drive");

        // distance from top of cwd_entries table to top border of dirent we are trying to scroll to
        std::optional<f32> scrolled_to_dirent_offset_y = std::nullopt;

        if (expl.scroll_to_nth_selected_entry_next_frame != u64(-1)) {
            u64 target = expl.scroll_to_nth_selected_entry_next_frame;
            u64 counter = 0;

            auto scrolled_to_dirent = std::find_if(expl.cwd_entries.begin(), expl.cwd_entries.end(), [&counter, target](explorer_window::dirent &e) noexcept {
                // stop spotlighting the previous dirents, looks better when rapidly advancing the spotlighted dirent.
                // without it multiple dirents can be spotlighted at the same time which is visually distracting and possible confusing.
                e.spotlight_frames_remaining = 0;

                return e.selected && (counter++) == target;
            });

            if (scrolled_to_dirent != expl.cwd_entries.end()) {
                scrolled_to_dirent->spotlight_frames_remaining = u32(imgui::GetIO().Framerate) / 3;
                scrolled_to_dirent_offset_y = ImGui::GetTextLineHeightWithSpacing() * f32(std::distance(expl.cwd_entries.begin(), scrolled_to_dirent));

                // stop spotlighting any dirents ahead which could linger if we just wrapped the spotlight back to the top,
                // looks better this way when rapidly advancing the spotlighted dirent.
                // without it multiple dirents can be spotlighted at the same time which is visually distracting and possibly confusing.
                std::for_each(scrolled_to_dirent + 1, expl.cwd_entries.end(),
                              [](explorer_window::dirent &e) noexcept { e.spotlight_frames_remaining = 0; });
            }

            u64 scroll_idx = std::distance(expl.cwd_entries.begin(), scrolled_to_dirent);
            imgui::SetNextWindowScroll(ImVec2(-1.0f, ImGui::GetTextLineHeightWithSpacing() * scroll_idx));

            expl.scroll_to_nth_selected_entry_next_frame = u64(-1);
        }

        s32 table_flags =
            ImGuiTableFlags_SizingStretchProp|
            ImGuiTableFlags_Hideable|
            ImGuiTableFlags_Resizable|
            ImGuiTableFlags_Reorderable|
            ImGuiTableFlags_Sortable|
            ImGuiTableFlags_BordersV|
            ImGuiTableFlags_ScrollY|
            ImGuiTableFlags_SortMulti|
            // ImGuiTableFlags_SortTristate|
            (global_state::settings().tables_alt_row_bg ? ImGuiTableFlags_RowBg : 0)|
            (global_state::settings().table_borders_in_body ? 0 : ImGuiTableFlags_NoBordersInBody)
        ;
        imgui::ScopedItemFlag no_nav(ImGuiItemFlags_NoNav, true);

        ImVec2 avail = imgui::GetContentRegionAvail();
        ImVec2 size = ImVec2(avail.x, avail.y - imgui::CalcTextSize("X").y - imgui::GetStyle().ItemSpacing.y*1 - imgui::GetStyle().FramePadding.y*2);

        // bool show_cwd_entries_table = !expl.cwd_entries.empty() && cnt.filtered_dirents < expl.cwd_entries.size();
        bool show_cwd_entries_table = true;

        if (show_cwd_entries_table && imgui::BeginTable("## explorer_window cwd_entries", explorer_window::cwd_entries_table_col_count, table_flags, size)) {
            s32 const col_flags_sortable_prefer_asc = ImGuiTableColumnFlags_DefaultSort|ImGuiTableColumnFlags_PreferSortAscending;
            s32 const col_flags_sortable_prefer_desc = ImGuiTableColumnFlags_DefaultSort|ImGuiTableColumnFlags_PreferSortDescending;

            f32 widths[explorer_window::cwd_entries_table_col_count] = {
                0.05f, // cwd_entries_table_col_number
                0.05f, // cwd_entries_table_col_id
                0.f, // cwd_entries_table_col_path (calculated as remainder)
                0.05f, // cwd_entries_table_col_object
                0.15f, // cwd_entries_table_col_type
                0.05f, // cwd_entries_table_col_size_formatted
                0.05f, // cwd_entries_table_col_size_bytes
                0.075f, // cwd_entries_table_col_creation_time
                0.075f, // cwd_entries_table_col_last_write_time
            };
            f32 fixed_widths_sum = std::accumulate(widths, widths + lengthof(widths), 0.f);
            widths[explorer_window::cwd_entries_table_col_path] = 1.0f - fixed_widths_sum;
            [[maybe_unused]] f32 widths_sum = std::accumulate(widths, widths + lengthof(widths), 0.f);
            [[maybe_unused]] f32 epsilon = 0.00001f;
            assert(fabs(1.0 - widths_sum) < epsilon); // a.k.a widths_sum == 1.0f

            imgui::TableSetupColumn("#", ImGuiTableColumnFlags_NoSort|ImGuiTableColumnFlags_WidthStretch, widths[explorer_window::cwd_entries_table_col_number], explorer_window::cwd_entries_table_col_number);
            imgui::TableSetupColumn("ID", col_flags_sortable_prefer_asc|ImGuiTableColumnFlags_WidthStretch, widths[explorer_window::cwd_entries_table_col_id], explorer_window::cwd_entries_table_col_id);
            imgui::TableSetupColumn("Name", col_flags_sortable_prefer_asc|ImGuiTableColumnFlags_WidthStretch|ImGuiTableColumnFlags_NoHide, widths[explorer_window::cwd_entries_table_col_path], explorer_window::cwd_entries_table_col_path);
            imgui::TableSetupColumn("Object", col_flags_sortable_prefer_asc|ImGuiTableColumnFlags_WidthStretch, widths[explorer_window::cwd_entries_table_col_object], explorer_window::cwd_entries_table_col_object);
            imgui::TableSetupColumn("Type", col_flags_sortable_prefer_asc|ImGuiTableColumnFlags_WidthStretch, widths[explorer_window::cwd_entries_table_col_type], explorer_window::cwd_entries_table_col_type);
            imgui::TableSetupColumn("Size", col_flags_sortable_prefer_desc|ImGuiTableColumnFlags_WidthStretch, widths[explorer_window::cwd_entries_table_col_size_formatted], explorer_window::cwd_entries_table_col_size_formatted);
            imgui::TableSetupColumn("Bytes", col_flags_sortable_prefer_desc|ImGuiTableColumnFlags_WidthStretch, widths[explorer_window::cwd_entries_table_col_size_bytes], explorer_window::cwd_entries_table_col_size_bytes);
            imgui::TableSetupColumn("Created", col_flags_sortable_prefer_asc|ImGuiTableColumnFlags_WidthStretch, widths[explorer_window::cwd_entries_table_col_creation_time], explorer_window::cwd_entries_table_col_creation_time);
            imgui::TableSetupColumn("Modified", col_flags_sortable_prefer_asc|ImGuiTableColumnFlags_WidthStretch, widths[explorer_window::cwd_entries_table_col_last_write_time], explorer_window::cwd_entries_table_col_last_write_time);
            ImGui::TableSetupScrollFreeze(0, 1);
            imgui::TableHeadersRow();

            //? ImGui does not allow you to hold a ImGuiTableSortSpecs pointer over multiple frames,
            //? so we make a copy of it for ourselves because we would like to use it in later frames.
            //? luckily it's a trivial and compact object so there is little implication making a copy each frame.
            ImGuiTableSortSpecs *table_sort_specs = imgui::TableGetSortSpecs();
            if (table_sort_specs != nullptr) {
                expl.column_sort_specs = expl.copy_column_sort_specs(table_sort_specs);
            }

            if (table_sort_specs != nullptr && table_sort_specs->SpecsDirty) {
                table_sort_specs->SpecsDirty = false;
                expl.first_filtered_cwd_dirent_iter = sort_cwd_entries(expl);
            } else {
                f64 find_first_filtered_cwd_dirent_us = 0;
                SCOPE_EXIT { expl.find_first_filtered_cwd_dirent_timing_samples.push_back(find_first_filtered_cwd_dirent_us); };
                scoped_timer<timer_unit::MICROSECONDS> timer(&find_first_filtered_cwd_dirent_us);

                // no point in binary search here, cost of linear traversal is tiny even for huge collection
                expl.first_filtered_cwd_dirent_iter = std::find_if(expl.cwd_entries.begin(), expl.cwd_entries.end(),
                    [](explorer_window::dirent const &ent) noexcept { return ent.filtered; });
            }

            // opens "Context" popup if a rendered dirent is right clicked
            auto [context_menu_target_row_rect, descend_target_, do_ascend_] = render_table_rows_for_cwd_entries(expl, cnt, size_unit_multiplier, any_popups_open, dir_sep_utf8, dir_sep_utf16);

            if (descend_target_.has_value()) descend_target = descend_target_;
            do_ascend = do_ascend_;

            auto result = render_dirent_context_menu(expl, cnt, global_state::settings());
            open_bulk_rename_popup   |= result.open_bulk_rename_popup;
            open_single_rename_popup |= result.open_single_rename_popup;
            if (result.single_dirent_to_be_renamed) {
                s_dirent_to_be_renamed = result.single_dirent_to_be_renamed;
            }

            if (result.context_menu_rect.has_value() && context_menu_target_row_rect.has_value() && cnt.selected_dirents <= 1) {
                assert(result.context_menu_rect.has_value());
                ImVec2 min = context_menu_target_row_rect.value().Min;
                min.x += 1; // to avoid overlap with table left V border
                ImVec2 const &max = context_menu_target_row_rect.value().Max;
                ImGui::GetWindowDrawList()->AddRect(min, max, imgui::ImVec4_to_ImU32(imgui::GetStyleColorVec4(ImGuiCol_NavHighlight), true));
            }

            imgui::EndTable();
        }
        accept_move_dirents_drag_drop(expl);
    }

    ImVec2 cwd_entries_child_size = imgui::GetItemRectSize();
    ImVec2 cwd_entries_child_min = imgui::GetItemRectMin();
    ImVec2 cwd_entries_child_max = imgui::GetItemRectMax();

    bool mouse_hovering_cwd_entries_child = imgui::IsMouseHoveringRect(cwd_entries_child_min, cwd_entries_child_max);

    // footer

    {
        bool recount = false;

        imgui::ScopedDisable d2(path_is_empty(expl.cwd));

        // imgui::SameLine(0, style.ItemSpacing.x / 2);

        recount = render_filter_reset_button(expl);

        imgui::SameLineSpaced(0);

        if (recount) {
            cnt = do_counting(expl);
        }

        auto [filter_focused, filter_edited] = render_filter_text_input(expl, window_hovered, io.KeyCtrl, cnt, cwd_exists_after_edit, any_popups_open);
        expl.filter_text_input_focused = filter_focused;

        imgui::SameLine();

        /* recount |= */ render_filter_mode_button(expl);
        imgui::SameLine(0, 0);
        /* recount |= */ render_filter_case_sensitivity_button(expl);
        imgui::SameLine(0, 0);
        /* recount |= */ render_filter_polarity_button(expl);

        imgui::SameLineSpaced(3);

        /* recount |= */ render_filter_type_toggler_buttons(expl, cnt);

        imgui::SameLineSpaced(2);

        {
            imgui::ScopedDisable d(path_is_empty(expl.cwd));
            imgui::ScopedItemFlag no_nav(ImGuiItemFlags_NoNav, true);
            if (imgui::Button(ICON_LC_FOLDER_PLUS)) swan_popup_modals::open_new_directory(expl.cwd.data(), expl.id); // ICON_CI_NEW_FOLDER
            if (imgui::IsItemHovered({}, 1)) imgui::SetTooltip("Create directory inside %s", expl.name);
        }

        imgui::SameLine(0, 0);
        {
            imgui::ScopedDisable d(path_is_empty(expl.cwd));
            imgui::ScopedItemFlag no_nav(ImGuiItemFlags_NoNav, true);
            if (imgui::Button(ICON_LC_FILE_PLUS)) swan_popup_modals::open_new_file(expl.cwd.data(), expl.id); // ICON_CI_FILE_ADD
            if (imgui::IsItemHovered({}, 1)) imgui::SetTooltip("Create file inside %s", expl.name);
        }

        bool can_paste = !global_state::file_op_cmd_buf().items.empty() && !path_is_empty(expl.cwd) & cwd_exists_after_edit;
        if (can_paste) {
            imgui::SameLine(0, 0);
            {
                imgui::ScopedDisable d(!can_paste);
                imgui::ScopedItemFlag no_nav(ImGuiItemFlags_NoNav, true);
                if (imgui::Button(ICON_LC_CLIPBOARD_PASTE)) {
                    auto result = global_state::file_op_cmd_buf().execute(expl);
                    // TODO: why is result unused?
                }
                if (imgui::IsItemHovered({}, 1)) imgui::SetTooltip("Paste into %s", expl.name);
            }
        }

        imgui::SameLineSpaced(2);

        bool open_history_browser = render_history_browser_button();
        if (open_history_browser) imgui::OpenPopup("History");

        imgui::SameLine(0, 0);
        {
            imgui::ScopedItemFlag no_nav(ImGuiItemFlags_NoNav, true);
            if (imgui::Button(ICON_LC_BOOK_OPEN_TEXT)) imgui::OpenPopup("Bookmarks");
            if (imgui::IsItemHovered({}, 1)) imgui::SetTooltip("Open bookmarks");
        }

        imgui::SameLine(0, 0);
        render_button_pin_cwd(expl, cwd_exists_after_edit);

        imgui::SameLine(0, 0);
        {
            imgui::ScopedDisable d(path_is_empty(expl.cwd) || !cwd_exists_after_edit);
            imgui::ScopedItemFlag no_nav(ImGuiItemFlags_NoNav, true);
            if (imgui::Button(ICON_CI_SYMBOL_NULL)) expl.invert_selection_on_visible_cwd_entries();
            if (imgui::IsItemHovered({}, 1)) imgui::SetTooltip("Invert selection");
        }

        imgui::SameLineSpaced(2);
        render_help_icon(expl);
    }

    if (!path_is_empty(expl.cwd)) {
        if (cnt.selected_dirents > 0) {
            imgui::SameLineSpaced(2);
            (void) render_num_cwd_items_selected(expl, cnt);
        }

        if (expl.refresh_message != "") {
            imgui::SameLineSpaced(2);
            imgui::TextColored(warning_color(), "%s", expl.refresh_message.c_str());
            if (imgui::IsItemHovered()) {
                imgui::SetTooltip(expl.refresh_message_tooltip.c_str());
            }
            if (imgui::IsItemClicked(ImGuiMouseButton_Left)) {
                (void) expl.update_cwd_entries(full_refresh, expl.cwd.data());
            }
        }
    }

    bool show_dir_not_found_msg = !cwd_exists_after_edit;

    if (show_dir_not_found_msg) {
        imgui::SameLineSpaced(3);
        {
            imgui::ScopedItemFlag no_nav(ImGuiItemFlags_NoNav, true);

            if (imgui::Button(ICON_LC_PLUS ICON_LC_FOLDERS)) {
                char const *full_path_utf8 = expl.cwd.data();
                try {
                    std::filesystem::path fs_path = full_path_utf8;
                    std::filesystem::create_directories(fs_path);
                    expl.update_cwd_entries(full_refresh, expl.cwd.data());
                }
                catch (std::exception const &except) {
                    std::string action = make_str("std::filesystem::create_directories [%s].", full_path_utf8);
                    char const *failed = except.what();
                    swan_popup_modals::open_error(action.c_str(), failed);
                }
                catch (...) {
                    std::string action = make_str("std::filesystem::create_directories [%s].", full_path_utf8);
                    char const *failed = "Caught unexpected exception -> catch(...)";
                    swan_popup_modals::open_error(action.c_str(), failed);
                }
            }
            if (imgui::IsItemHovered()) imgui::SetTooltip("Create directories");
        }
    }

    if (!any_popups_open && mouse_hovering_cwd_entries_child) {
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
            expl.tabbing_focus_idx = -1;
        }
        else if (imgui::IsKeyPressed(ImGuiKey_A) && io.KeyCtrl) {
            expl.select_all_visible_cwd_entries();
        }
        else if (imgui::IsKeyPressed(ImGuiKey_X) && io.KeyCtrl && window_hovered && !expl.filter_text_input_focused) {
            global_state::file_op_cmd_buf().clear();
            auto result = add_selected_entries_to_file_op_payload(expl, "Cut", file_operation_type::move);
            handle_file_op_failure("cut", result);
        }
        else if (imgui::IsKeyPressed(ImGuiKey_C) && io.KeyCtrl && window_hovered) {
            global_state::file_op_cmd_buf().clear();
            auto result = add_selected_entries_to_file_op_payload(expl, "Copy", file_operation_type::copy);
            handle_file_op_failure("copy", result);
        }
        else if (imgui::IsKeyPressed(ImGuiKey_V) && io.KeyCtrl && window_hovered && !global_state::file_op_cmd_buf().items.empty()) {
            global_state::file_op_cmd_buf().execute(expl);
        }
        else if (imgui::IsKeyPressed(ImGuiKey_I) && io.KeyCtrl) {
            expl.invert_selection_on_visible_cwd_entries();
        }
    }

    bool any_clickable_footer_components_hovered = false;

    if (expl.footer_rect.has_value()) {
        expl.footer_hovered = imgui::IsMouseHoveringRect(expl.footer_rect.value());

        expl.footer_selection_info_hovered = expl.footer_selection_info_rect.has_value() ? imgui::IsMouseHoveringRect(expl.footer_selection_info_rect.value()) : false;
        expl.footer_filter_info_hovered    = expl.footer_filter_info_rect.has_value()    ? imgui::IsMouseHoveringRect(expl.footer_filter_info_rect.value())    : false;
        expl.footer_clipboard_hovered      = expl.footer_clipboard_rect.has_value()      ? imgui::IsMouseHoveringRect(expl.footer_clipboard_rect.value())    : false;

        any_clickable_footer_components_hovered = expl.footer_selection_info_hovered || expl.footer_filter_info_hovered || expl.footer_clipboard_hovered;

        if (imgui::IsMouseClicked(ImGuiMouseButton_Left) && expl.footer_hovered && !any_clickable_footer_components_hovered) {
            expl.deselect_all_cwd_entries();
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

    {
        ImVec2 avail = imgui::GetContentRegionAvail();
        avail.y -= imgui::GetStyle().WindowPadding.y*10;
        imgui::SetNextWindowPos(base_window_pos, ImGuiCond_Always);
        imgui::SetNextWindowSize(imgui::GetWindowContentRegionMax(), ImGuiCond_Always);
    }
    if (imgui::BeginPopupModal("History", nullptr, ImGuiWindowFlags_NoResize)) {
        swan_path backup = expl.cwd;
        bool history_item_clicked = render_history_browser_popup(expl, cwd_exists_after_edit);

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

            if (!path_loosely_same(pin.path.data(), expl.cwd)) {
                open_target = pin.path;
            }
        }

        imgui::EndDragDropTarget();
    }

    if (open_target.has_value()) {
        swan_path initial_cwd = expl.cwd;
        expl.cwd = path_create("");

        auto result = try_descend_to_directory(expl, descend_target.value().data());

        if (!result.success) {
            std::string action = make_str("Open [%s] in Explorer %d.", descend_target.value().data(), expl.id+1);
            char const *failed = result.err_msg.c_str();
            swan_popup_modals::open_error(action.c_str(), failed);

            expl.cwd = initial_cwd;
            (void) expl.update_cwd_entries(full_refresh, expl.cwd.data()); // restore entries cleared by try_descend_to_directory
        }
    }
    if (descend_target.has_value()) {
        swan_path initial_cwd = expl.cwd;

        auto result = try_descend_to_directory(expl, descend_target.value().data());

        if (!result.success) {
            std::string action = make_str("Descend into [%s] in Explorer %d.", descend_target.value().data(), expl.id+1);
            char const *failed = result.err_msg.c_str();
            swan_popup_modals::open_error(action.c_str(), failed);

            expl.cwd = initial_cwd;
            (void) expl.update_cwd_entries(full_refresh, expl.cwd.data()); // restore entries cleared by try_descend_to_directory
        }
    }
    else if (do_ascend) {
        swan_path initial_cwd = expl.cwd;

        auto result = try_ascend_directory(expl);

        if (!result.success) {
            std::string action = make_str("Ascend to [%s].", result.parent_dir.data());
            char const *failed = "Directory not found.";
            swan_popup_modals::open_error(action.c_str(), failed);

            expl.cwd = initial_cwd;
            (void) expl.update_cwd_entries(full_refresh, expl.cwd.data()); // restore entries cleared by try_ascend_directory
        }
    }

    return true;
}

void file_operation_command_buf::clear() noexcept
{
    global_state::file_op_cmd_buf().items.clear();

    auto &all_explorers = global_state::explorers();
    for (auto &expl : all_explorers) {
        expl.uncut();
    }
}

generic_result file_operation_command_buf::execute(explorer_window &expl) noexcept
{
    wchar_t cwd_utf16[2048]; cstr_clear(cwd_utf16);

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
            cstr_clear(item_utf16);

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
        &initialization_error,
        global_state::settings().dir_separator_utf8,
        global_state::settings().num_max_file_operations);

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
render_table_rows_for_cwd_entries_result render_table_rows_for_cwd_entries(
    explorer_window &expl,
    cwd_count_info const &cnt,
    u64 size_unit_multiplier,
    bool any_popups_open,
    char dir_sep_utf8,
    wchar_t dir_sep_utf16) noexcept
{
    render_table_rows_for_cwd_entries_result retval = {};

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

            ImRect selectable_rect;

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
                static ImVec2 s_last_known_icon_size = {};

                if (global_state::settings().win32_file_icons) {
                    if (dirent.icon_GLtexID == 0) {
                        swan_path full_path_utf8 = expl.cwd;
                        if (path_append(full_path_utf8, dirent.basic.path.data(), dir_sep_utf8, true)) {
                            std::tie(dirent.icon_GLtexID, dirent.icon_size) = load_icon_texture(full_path_utf8.data(), 0, "explorer_window::dirent");
                            if (dirent.icon_GLtexID > 0) {
                                s_last_known_icon_size = dirent.icon_size;
                            }
                        }
                    }
                    auto const &icon_size = dirent.icon_GLtexID < 1 ? s_last_known_icon_size : dirent.icon_size;
                    imgui::Image((ImTextureID)std::max(dirent.icon_GLtexID, s64(0)), icon_size, ImVec2(0,0), ImVec2(1,1), ImVec4(1,1,1, dirent.cut ? .3f : 1.f));
                }
                else { // fallback to generic icons
                    char const *icon = dirent.basic.kind_icon();
                    ImVec4 color = get_color(dirent.basic.type);
                    if (dirent.cut) imgui::ReduceAlphaTo(color, .25f);
                    imgui::TextColored(color, icon);
                }
                imgui::SameLine();

                ImVec2 path_text_rect_min = imgui::GetCursorScreenPos();
                {
                    imgui::ScopedTextColor tc_spotlight(dirent.spotlight_frames_remaining > 0 ? success_color() : imgui::GetStyle().Colors[ImGuiCol_Text]);
                    // imgui::ScopedStyle<ImVec4> tc_context(imgui::GetStyle().Colors[ImGuiCol_Text], error_color(), dirent.context_menu_active);

                    auto label = make_str_static<1200>("%s##dirent%zu", path, i);
                    if (imgui::Selectable(label.data(), dirent.selected, ImGuiSelectableFlags_SpanAllColumns|ImGuiSelectableFlags_AllowDoubleClick)) {
                        bool selection_before_deselect = dirent.selected;

                        u64 num_deselected = 0;
                        if (!io.KeyCtrl && !io.KeyShift) {
                            // entry was selected but Ctrl was not held, so deselect everything
                            num_deselected = expl.deselect_all_cwd_entries(); // this will alter dirent.selected
                        }

                        if (num_deselected > 1) {
                            dirent.selected = true;
                        } else {
                            dirent.selected = !selection_before_deselect;
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
                                    dirent_.selected = true;
                                }
                            }
                        }
                        else { // not shift click, check for double click

                            static swan_path s_last_click_path = {};
                            swan_path const &current_click_path = dirent.basic.path;

                            if (imgui::IsItemActivated() || imgui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && !io.KeyCtrl && path_equals_exactly(current_click_path, s_last_click_path)) {
                                if (dirent.basic.is_directory()) {
                                    if (dirent.basic.is_path_dotdot()) {
                                        retval.do_ascend = true;
                                    }
                                    else {
                                        retval.descend_target = current_click_path;
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
                                            expl.advance_history(expl.cwd);
                                            (void) expl.update_cwd_entries(full_refresh, expl.cwd.data());
                                            (void) expl.save_to_disk();
                                        }
                                        else if (dirent.basic.is_symlink_to_file()) {
                                            char const *full_file_path = res.error_or_utf8_path.c_str();
                                            global_state::recent_files_update("Opened", full_file_path);
                                            (void) global_state::recent_files_save_to_disk(nullptr);
                                        }
                                    } else {
                                        std::string action = make_str("Open symlink [%s].", dirent.basic.path.data());
                                        char const *failed = res.error_or_utf8_path.c_str();
                                        swan_popup_modals::open_error(action.c_str(), failed);
                                    }
                                }
                                else {
                                    // print_debug_msg("[ %d ] double clicked file [%s]", expl.id, dirent.basic.path.data());

                                    // TODO: async
                                    auto res = open_file(dirent.basic.path.data(), expl.cwd.data());

                                    if (res.success) {
                                        char const *full_file_path = res.error_or_utf8_path.c_str();
                                        global_state::recent_files_update("Opened", full_file_path);
                                        (void) global_state::recent_files_save_to_disk(nullptr);
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

                    selectable_rect = imgui::GetItemRect();

                    if (expl.tabbing_set_focus && expl.tabbing_focus_idx == s64(i) && !imgui::IsItemFocused()) {
                        expl.tabbing_set_focus = false;
                        imgui::FocusItem();
                    }

                    // {
                    //     f32 offset_for_icon_and_space = imgui::CalcTextSize(icon).x + imgui::CalcTextSize(" ").x;
                    //     imgui::RenderTooltipWhenColumnTextTruncated(explorer_window::cwd_entries_table_col_path, path, offset_for_icon_and_space);
                    // }
                }

                if (dirent.highlight_len > 0) {
                    imgui::HighlightTextRegion(path_text_rect_min, dirent.basic.path.data(), dirent.highlight_start_idx, dirent.highlight_len,
                                               imgui::ReduceAlphaTo(imgui::Denormalize(warning_lite_color()), 75));
                }

                if (dirent.basic.is_path_dotdot()) {
                    dirent.selected = false; // do no allow [..] to be selected
                }

                if (imgui::IsItemClicked(ImGuiMouseButton_Right) && !any_popups_open && !dirent.basic.is_path_dotdot()) {
                    // print_debug_msg("[ %d ] right clicked [%s]", expl.id, dirent.basic.path.data());
                    imgui::OpenPopup("## explorer context_menu");
                    expl.context_menu_target = &dirent;
                    dirent.context_menu_active = true;

                    if (!dirent.selected) {
                        expl.deselect_all_cwd_entries();
                    }
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
                    explorer_drag_drop_payload &payload,
                    std::wstring &paths,
                    std::wstring const &cwd_utf16,
                    basic_dirent const &dirent) noexcept
                {
                    static std::wstring dirent_full_path_utf16 = {};
                    dirent_full_path_utf16.clear();

                    wchar_t dirent_name_utf16[MAX_PATH];
                    if (!utf8_to_utf16(dirent.path.data(), dirent_name_utf16, lengthof(dirent_name_utf16))) {
                        return;
                    }

                    try {
                        dirent_full_path_utf16 = cwd_utf16;
                        if (dirent_full_path_utf16.back() != dir_sep_utf16) {
                            dirent_full_path_utf16 += dir_sep_utf16;
                        }
                        dirent_full_path_utf16 += dirent_name_utf16;

                        paths += dirent_full_path_utf16 += L'\n';
                        payload.num_items += 1;
                        payload.obj_type_counts[(u64)dirent.type] += 1;
                    }
                    catch (...) {
                        // TODO report error
                    }
                };

                // don't compute payload until someone accepts or it gets dropped
                if (!global_state::move_dirents_payload_set()) {
                    std::wstring cwd_utf16 = cwd_to_utf16();
                    assert(!cwd_utf16.empty());
                    explorer_drag_drop_payload payload = {};
                    std::wstring paths = {};

                    payload.src_explorer_id = expl.id;

                    if (dirent.selected) {
                        for (auto const &dirent_ : expl.cwd_entries) {
                            if (!dirent_.filtered && dirent_.selected) {
                                add_payload_item(payload, paths, cwd_utf16, dirent_.basic);
                            }
                        }
                    }
                    else {
                        add_payload_item(payload, paths, cwd_utf16, dirent.basic);
                        global_state::move_dirents_payload_set() = true;
                    }

                    u64 paths_len_including_trailing_newline = paths.size() + 1;

                    wchar_t *paths_data = new wchar_t[paths_len_including_trailing_newline];
                    StrCpyNW(paths_data, paths.c_str(), s32(paths_len_including_trailing_newline));

                    payload.full_paths_delimited_by_newlines = paths_data;
                    payload.full_paths_delimited_by_newlines_len = paths_len_including_trailing_newline;

                    imgui::SetDragDropPayload("explorer_drag_drop_payload", (void *)&payload, sizeof(payload), ImGuiCond_Once);
                    print_debug_msg("SetDragDropPayload(explorer_drag_drop_payload) - %s", format_file_size(payload.get_num_heap_bytes_allocated(), 1024));
                    global_state::move_dirents_payload_set() = true;

                    // WCOUT_IF_DEBUG("payload.src_explorer_id = " << payload.src_explorer_id << '\n');
                    // WCOUT_IF_DEBUG("payload.full_paths_delimited_by_newlines:\n" << payload.full_paths_delimited_by_newlines << '\n');
                }

                auto payload_wrapper = imgui::GetDragDropPayload();
                if (payload_wrapper != nullptr && cstr_eq(payload_wrapper->DataType, "explorer_drag_drop_payload")) {
                    auto payload_data = reinterpret_cast<explorer_drag_drop_payload *>(payload_wrapper->Data);

                    u64 digits = count_digits(payload_data->num_items);
                    bool homogeneous_obj_type = std::count_if(payload_data->obj_type_counts, payload_data->obj_type_counts + u64(basic_dirent::kind::count), [](u64 val) { return val != 0; }) == 1;

                    for (u64 j = 0; j < (u64)basic_dirent::kind::count; ++j) {
                        u64 obj_type_cnt = payload_data->obj_type_counts[j];
                        if (obj_type_cnt > 0) {
                            basic_dirent::kind obj_type = basic_dirent::kind(j);
                            imgui::TextColored(get_color(obj_type), "%*zu %s", digits, obj_type_cnt, get_icon(obj_type));
                        }
                    }
                    if (!homogeneous_obj_type) {
                        imgui::Separator();
                        imgui::Text("%*zu", digits, payload_data->num_items);
                    }

                    std::optional<bool> mouse_inside_window = win32_is_mouse_inside_window(global_state::window_handle());

                    if (!mouse_inside_window.value_or(true)) {
                        auto drop_obj = std::make_unique<explorer_drop_source>();

                        // make a copy, don't want race conditions or use after free bugs
                        drop_obj->full_paths_delimited_by_newlines = payload_data->full_paths_delimited_by_newlines;
                        DWORD effect;

                        HRESULT result_drag = DoDragDrop(drop_obj.get(), drop_obj.get(), DROPEFFECT_LINK|DROPEFFECT_COPY, &effect);

                        switch (result_drag) {
                            case DRAGDROP_S_DROP: {
                                print_debug_msg("DoDragDrop: The OLE drag-and-drop operation was successful.");
                                // s_prevent_drag_drop = true;

                                // After DoDragDrop completes, IsMouseDown && IsMouseReleased && io.KeyCtrl state remains true which is incorrect,
                                // simulate a release to clear the stuck state.
                                imgui::GetIO().AddMouseButtonEvent(ImGuiMouseButton_Left, false);

                                free_explorer_drag_drop_payload();

                                break;
                            }
                            case DRAGDROP_S_CANCEL: {
                                print_debug_msg("DoDragDrop: The OLE drag-and-drop operation was canceled.");
                                break;
                            }
                            case E_UNEXPECTED: {
                                print_debug_msg("DoDragDrop: Unexpected error occurred.");
                                break;
                            }
                            default: {
                                print_debug_msg("DoDragDrop (%X) %s", result_drag, _com_error(result_drag).ErrorMessage());
                                break;
                            }
                        }
                    }
                }

                imgui::EndDragDropSource();
            }

            if (!dirent.basic.is_file() && !dirent.basic.is_symlink_to_file() && !dirent.selected && imgui::BeginDragDropTarget()) {
                auto payload_wrapper = imgui::GetDragDropPayload();

                if (payload_wrapper != nullptr && cstr_eq(payload_wrapper->DataType, "explorer_drag_drop_payload")) {
                    payload_wrapper = imgui::AcceptDragDropPayload("explorer_drag_drop_payload");
                    if (payload_wrapper != nullptr) {
                        handle_drag_drop_onto_dirent(expl, dirent, payload_wrapper, dir_sep_utf8);
                    }
                }

                imgui::EndDragDropTarget();
            }

            if (imgui::TableSetColumnIndex(explorer_window::cwd_entries_table_col_object)) {
                char const *object_desc = dirent.basic.kind_short_cstr();
                imgui::TextUnformatted(object_desc);
                imgui::RenderTooltipWhenColumnTextTruncated(explorer_window::cwd_entries_table_col_object, object_desc);
            }

            if (imgui::TableSetColumnIndex(explorer_window::cwd_entries_table_col_type)) {
            // #if CACHE_FORMATTED_STRING_COLUMNS
                // Not worth caching, miniscule cost to compute each frame

                std::array<char, 64> type_text;
                {
                    f64 func_us = 0;
                    SCOPE_EXIT { expl.type_description_culmulative_us += func_us; };
                    scoped_timer<timer_unit::MICROSECONDS> timer(&func_us);

                    if (dirent.basic.is_directory()) {
                        type_text = { "Directory" };
                    } else {
                        if (std::strchr(dirent.basic.path.data(), '.')) {

                            char const *extension = path_cfind_file_ext(dirent.basic.path.data());
                            type_text = get_type_text_for_extension(extension);
                        } else {
                            type_text = { "File" };
                        }
                    }
                }
                imgui::TextUnformatted(type_text.data());
                imgui::RenderTooltipWhenColumnTextTruncated(explorer_window::cwd_entries_table_col_type, type_text.data());
            }

            if (imgui::TableSetColumnIndex(explorer_window::cwd_entries_table_col_size_formatted)) {
                if (!dirent.basic.is_directory()) {
            #if CACHE_FORMATTED_STRING_COLUMNS
                    if (cstr_empty(dirent.formatted_size.data())) {
                        f64 func_us = 0;
                        SCOPE_EXIT { expl.format_file_size_culmulative_us += func_us; };
                        scoped_timer<timer_unit::MICROSECONDS> timer(&func_us);
                        dirent.formatted_size = format_file_size(dirent.basic.size, size_unit_multiplier);
                    }
                    imgui::TextUnformatted(dirent.formatted_size.data());
                    imgui::RenderTooltipWhenColumnTextTruncated(explorer_window::cwd_entries_table_col_size_formatted, dirent.formatted_size.data());
            #else
                    std::array<char, 32> formatted_size;
                    {
                        f64 func_us = 0;
                        SCOPE_EXIT { expl.format_file_size_culmulative_us += func_us; };
                        scoped_timer<timer_unit::MICROSECONDS> timer(&func_us);
                        formatted_size = format_file_size(dirent.basic.size, size_unit_multiplier);
                    }
                    imgui::TextUnformatted(formatted_size.data());
                    imgui::RenderTooltipWhenColumnTextTruncated(explorer_window::cwd_entries_table_col_size_formatted, formatted_size.data());
            #endif
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
            #if CACHE_FORMATTED_STRING_COLUMNS
                if (cstr_empty(dirent.creation_time.data())) {
                    f64 func_us = 0;
                    SCOPE_EXIT { expl.filetime_to_string_culmulative_us += func_us; };
                    scoped_timer<timer_unit::MICROSECONDS> timer(&func_us);
                    dirent.creation_time = filetime_to_string(&dirent.basic.creation_time_raw).second;
                }
                imgui::TextUnformatted(dirent.creation_time.data());
                imgui::RenderTooltipWhenColumnTextTruncated(explorer_window::cwd_entries_table_col_creation_time, dirent.creation_time.data());
            #else
                std::pair<s32, std::array<char, 64U>> result;
                {
                    f64 func_us = 0;
                    SCOPE_EXIT { expl.filetime_to_string_culmulative_us += func_us; };
                    scoped_timer<timer_unit::MICROSECONDS> timer(&func_us);
                    result = filetime_to_string(&dirent.basic.creation_time_raw);
                }
                imgui::TextUnformatted(result.second.data());
                imgui::RenderTooltipWhenColumnTextTruncated(explorer_window::cwd_entries_table_col_creation_time, result.second.data());
            #endif
            }

            if (imgui::TableSetColumnIndex(explorer_window::cwd_entries_table_col_last_write_time)) {
            #if CACHE_FORMATTED_STRING_COLUMNS
                if (cstr_empty(dirent.last_write_time.data())) {
                    f64 func_us = 0;
                    SCOPE_EXIT { expl.filetime_to_string_culmulative_us += func_us; };
                    scoped_timer<timer_unit::MICROSECONDS> timer(&func_us);
                    dirent.last_write_time = filetime_to_string(&dirent.basic.last_write_time_raw).second;
                }
                imgui::TextUnformatted(dirent.last_write_time.data());
                imgui::RenderTooltipWhenColumnTextTruncated(explorer_window::cwd_entries_table_col_last_write_time, dirent.last_write_time.data());
            #else
                std::pair<s32, std::array<char, 64U>> result;
                {
                    f64 func_us = 0;
                    SCOPE_EXIT { expl.filetime_to_string_culmulative_us += func_us; };
                    scoped_timer<timer_unit::MICROSECONDS> timer(&func_us);
                    result = filetime_to_string(&dirent.basic.last_write_time_raw);
                }
                imgui::TextUnformatted(result.second.data());
                imgui::RenderTooltipWhenColumnTextTruncated(explorer_window::cwd_entries_table_col_last_write_time, result.second.data());
            #endif
            }

            if (dirent.context_menu_active) {
                retval.context_menu_target_row_rect.emplace(selectable_rect.Min, selectable_rect.Max);
            }

            dirent.spotlight_frames_remaining -= (dirent.spotlight_frames_remaining > 0);
        }
    }

    return retval;
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

            if ((path_ends_with(expl.context_menu_target->basic.path, ".exe") || path_ends_with(expl.context_menu_target->basic.path, ".bat"))
                && imgui::Selectable("Run as administrator"))
            {
                // TODO: async
                auto res = open_file(expl.context_menu_target->basic.path.data(), expl.cwd.data(), true);

                if (res.success) {
                    char const *full_file_path = res.error_or_utf8_path.c_str();
                    global_state::recent_files_update("Opened", full_file_path);
                    (void) global_state::recent_files_save_to_disk(nullptr);
                } else {
                    std::string action = make_str("Open file as administrator [%s].", expl.context_menu_target->basic.path.data());
                    char const *failed = res.error_or_utf8_path.c_str();
                    swan_popup_modals::open_error(action.c_str(), failed);
                }
            }

            if (expl.context_menu_target->basic.is_symlink_to_file() && imgui::Selectable("Open file location")) {
                symlink_data lnk_data = {};
                auto extract_result = lnk_data.load(expl.context_menu_target->basic.path.data(), expl.cwd.data());

                if (!extract_result.success) {
                    std::string action = make_str("Open file location of [%s].", expl.context_menu_target->basic.path.data());
                    char const *failed = extract_result.error_or_utf8_path.c_str();
                    swan_popup_modals::open_error(action.c_str(), failed);
                } else {
                    // no error checking because symlink_data::extract has already validated things

                    std::string_view parent_dir = path_extract_location(lnk_data.target_path_utf8.data());
                    expl.cwd = path_create(parent_dir.data(), parent_dir.size());

                    swan_path select_name_utf8 = path_create(path_find_filename(lnk_data.target_path_utf8.data()));
                    {
                        std::scoped_lock lock(expl.select_cwd_entries_on_next_update_mutex);
                        expl.select_cwd_entries_on_next_update.clear();
                        expl.select_cwd_entries_on_next_update.push_back(select_name_utf8);
                    }

                    expl.advance_history(expl.cwd);
                    (void) expl.update_cwd_entries(full_refresh, expl.cwd.data());
                    (void) expl.save_to_disk();

                    expl.scroll_to_nth_selected_entry_next_frame = 0;
                }
            }

            {
                static progressive_task<std::optional<generic_result>> task = {};

                if (imgui::Selectable("Open with...")) {
                    global_state::thread_pool().push_task([&expl]() noexcept {
                        task.active_token.store(true);

                        // TODO: async
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

            if (imgui::Selectable("Reveal in WFE")) {
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

            if (imgui::Selectable("Cut" "## single")) {
                if (!io.KeyShift) {
                    global_state::file_op_cmd_buf().clear();
                }
                expl.deselect_all_cwd_entries();
                expl.context_menu_target->selected = true;
                auto result = add_selected_entries_to_file_op_payload(expl, "Cut", file_operation_type::move);
                handle_failure("cut", result);
            }
            if (imgui::Selectable("Copy" "## single")) {
                if (!io.KeyShift) {
                    global_state::file_op_cmd_buf().clear();
                }

                expl.deselect_all_cwd_entries();
                expl.context_menu_target->selected = true;

                auto result = add_selected_entries_to_file_op_payload(expl, "Copy", file_operation_type::copy);
                handle_failure("copy", result);
            }

            imgui::Separator();

            if (imgui::Selectable("Create shortcut" "## single")) {
                swan_path lnk_path = expl.context_menu_target->basic.path;
                // TODO add something to end of path to avoid overwriting existing .lnk file

                if (!path_append(lnk_path, ".lnk")) {
                    std::string action = make_str("Create link path for [%s].", expl.context_menu_target->basic.path.data());
                    char const *failed = "Max path length exceeded when appending [.lnk] to file name.";
                    swan_popup_modals::open_error(action.c_str(), failed);
                }
                else {
                    symlink_data lnk;
                    lnk.show_cmd = SW_SHOWDEFAULT;
                    lnk.target_path_utf8 = expl.context_menu_target->basic.path;
                    cstr_clear(lnk.target_path_utf16);
                    cstr_clear(lnk.arguments_utf16);
                    cstr_clear(lnk.working_directory_path_utf16);

                    swan_path cwd_canonical = path_reconstruct_canonically(expl.cwd.data());
                    if (!utf8_to_utf16(cwd_canonical.data(), lnk.working_directory_path_utf16, lengthof(lnk.working_directory_path_utf16))) {
                        // TODO notification (warning)
                    }

                    auto result = lnk.save(lnk_path.data(), expl.cwd.data());

                    if (!result.success) {
                        std::string action = make_str("Create link file for [%s].", expl.context_menu_target->basic.path.data());
                        char const *failed = result.error_or_utf8_path.c_str();
                        swan_popup_modals::open_error(action.c_str(), failed);
                    }
                }
            }
            if (imgui::Selectable("Delete" "## single")) {
                expl.deselect_all_cwd_entries();
                expl.context_menu_target->selected = true;

                imgui::OpenConfirmationModalWithCallback(
                    /* confirmation_id  = */ swan_id_confirm_explorer_execute_delete,
                    /* confirmation_msg = */ "Are you sure you want to delete this file?",
                    /* on_yes_callback  = */
                    [&]() noexcept {
                        auto result = delete_selected_entries(expl, global_state::settings());

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
            if (imgui::Selectable("Rename" "## single")) {
                retval.open_single_rename_popup = true;
                retval.single_dirent_to_be_renamed = expl.context_menu_target;
            }

            imgui::Separator();

            if (!global_state::file_op_cmd_buf().items.empty() && !path_is_empty(expl.cwd) && imgui::Selectable("Paste")) {
                auto result = global_state::file_op_cmd_buf().execute(expl);
                // TODO: why is result unused?
            }


            if (imgui::Selectable("Properties")) {
                swan_path full_path = path_create(expl.cwd.data());
                if (!path_append(full_path, expl.context_menu_target->basic.path.data(), settings.dir_separator_utf8, true)) {
                    std::string action = make_str("Open properties of [%s].", expl.context_menu_target->basic.path.data());
                    char const *failed = "Max path length exceeded when appending name to current working directory path.";
                    swan_popup_modals::open_error(action.c_str(), failed);
                } else {
                    imgui::SetClipboardText(full_path.data());
                }
                global_state::thread_pool().push_task([full_path]() { open_file_properties(full_path.data()); });
            }

            if (imgui::BeginMenu("Copy metadata")) {
                if (imgui::Selectable("Name")) {
                    imgui::SetClipboardText(expl.context_menu_target->basic.path.data());
                }
                if (imgui::Selectable("Full path")) {
                    swan_path full_path = path_create(expl.cwd.data());
                    if (!path_append(full_path, expl.context_menu_target->basic.path.data(), settings.dir_separator_utf8, true)) {
                        std::string action = make_str("Copy full path of [%s].", expl.context_menu_target->basic.path.data());
                        char const *failed = "Max path length exceeded when appending name to current working directory path.";
                        swan_popup_modals::open_error(action.c_str(), failed);
                    } else {
                        imgui::SetClipboardText(full_path.data());
                    }
                }
                if (imgui::Selectable("Size in bytes")) {
                    imgui::SetClipboardText(std::to_string(expl.context_menu_target->basic.size).c_str());
                }
                if (imgui::Selectable("Formatted size")) {
                    auto formatted_size = format_file_size(expl.context_menu_target->basic.size, settings.size_unit_multiplier);
                    imgui::SetClipboardText(formatted_size.data());
                }

                imgui::EndMenu();
            }
        }
        else { // right click when > 1 dirents selected
            auto handle_failure = [](char const *operation, generic_result const &result) noexcept {
                if (!result.success) {
                    u64 num_failed = std::count(result.error_or_utf8_path.begin(), result.error_or_utf8_path.end(), '\n');
                    std::string action = make_str("%s %zu items.", operation, num_failed);
                    char const *failed = result.error_or_utf8_path.c_str();
                    swan_popup_modals::open_error(action.c_str(), failed);
                }
            };

            if (imgui::Selectable("Cut" "## multi")) {
                if (!io.KeyShift) {
                    global_state::file_op_cmd_buf().clear();
                }
                auto result = add_selected_entries_to_file_op_payload(expl, "Cut", file_operation_type::move);
                handle_failure("Cut", result);
            }
            if (imgui::Selectable("Copy" "## multi")) {
                if (!io.KeyShift) {
                    global_state::file_op_cmd_buf().clear();
                }
                auto result = add_selected_entries_to_file_op_payload(expl, "Copy", file_operation_type::copy);
                handle_failure("Copy", result);
            }

            imgui::Separator();

            if (imgui::Selectable("Delete" "## multi")) {
                auto result = delete_selected_entries(expl, global_state::settings());
                handle_failure("Delete", result);
            }
            if (imgui::Selectable("Bulk Rename")) {
                retval.open_bulk_rename_popup = true;
            }

            imgui::Separator();

            if (imgui::BeginMenu("Copy metadata")) {
                if (imgui::Selectable("Names")) {
                    std::string clipboard = {};

                    for (auto const &dirent : expl.cwd_entries) {
                        if (dirent.selected && !dirent.filtered) {
                            clipboard += dirent.basic.path.data();
                            clipboard += '\n';
                        }
                    }

                    imgui::SetClipboardText(clipboard.c_str());
                }
                if (imgui::Selectable("Full paths")) {
                    std::string clipboard = {};

                    for (auto const &dirent : expl.cwd_entries) {
                        if (dirent.selected && !dirent.filtered) {
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

                    if (clipboard.ends_with('\n')) clipboard.pop_back();

                    imgui::SetClipboardText(clipboard.c_str());
                }

                imgui::EndMenu();
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
            cstr_eq(payload_wrapper->DataType, "explorer_drag_drop_payload") &&
            reinterpret_cast<explorer_drag_drop_payload *>(payload_wrapper->Data)->src_explorer_id != expl.id)
        {
            payload_wrapper = imgui::AcceptDragDropPayload("explorer_drag_drop_payload");
            if (payload_wrapper != nullptr) {
                auto payload_data = (explorer_drag_drop_payload *)payload_wrapper->Data;
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

void free_explorer_drag_drop_payload() noexcept
{
    global_state::move_dirents_payload_set() = false;

    auto payload_wrapper = imgui::GetDragDropPayload();

    if (payload_wrapper != nullptr && cstr_eq(payload_wrapper->DataType, "explorer_drag_drop_payload")) {
        payload_wrapper = imgui::AcceptDragDropPayload("explorer_drag_drop_payload", ImGuiDragDropFlags_AcceptBeforeDelivery);

        if (payload_wrapper != nullptr) {
            auto payload_data = reinterpret_cast<explorer_drag_drop_payload *>(payload_wrapper->Data);
            assert(payload_data != nullptr);

            wchar_t *&paths_data = payload_data->full_paths_delimited_by_newlines;
            u64 &paths_len = payload_data->full_paths_delimited_by_newlines_len;

            print_debug_msg("FreeDragDropPayload(explorer_drag_drop_payload) - %s",
                            format_file_size(payload_data->get_num_heap_bytes_allocated(), 1024).data());

            cstr_fill(paths_data, L'!'); // fill with some nonsense for easier debugging
            delete[] paths_data;

            paths_data = nullptr;
            paths_len = 0;
        }
    }
}

bool find_in_swan_explorer_0(char const *full_path) noexcept
{
    explorer_window &expl = global_state::explorers()[0];

    swan_path reveal_name_utf8 = path_create(path_cfind_filename(full_path));
    std::string_view path_no_name_utf8 = path_extract_location(full_path);

    expl.deselect_all_cwd_entries();
    {
        std::scoped_lock lock2(expl.select_cwd_entries_on_next_update_mutex);
        expl.select_cwd_entries_on_next_update.clear();
        expl.select_cwd_entries_on_next_update.push_back(reveal_name_utf8);
    }

    swan_path containing_dir_utf8 = path_create(path_no_name_utf8.data(), path_no_name_utf8.size());
    auto [containing_dir_exists, num_selected] = expl.update_cwd_entries(full_refresh, containing_dir_utf8.data());

    if (!containing_dir_exists) {
        std::string action = make_str("Find [%s] in Explorer %d.", full_path, expl.id+1);
        char const *error = "Parent directory not found. It was renamed, moved or deleted after the operation was logged.";
        swan_popup_modals::open_error(action.c_str(), error);
        (void) expl.update_cwd_entries(full_refresh, expl.cwd.data()); // restore
        return false;
    }
    else if (num_selected == 0) {
        std::string action = make_str("Find [%s] in Explorer %d.", full_path, expl.id+1);
        char const *error = "File not found. It was renamed, moved, or deleted after the operation was logged.";
        swan_popup_modals::open_error(action.c_str(), error);
        (void) expl.update_cwd_entries(full_refresh, expl.cwd.data()); // restore
        return false;
    }
    else {
        expl.cwd = expl.latest_valid_cwd = containing_dir_utf8;
        expl.advance_history(expl.cwd);
        (void) expl.save_to_disk();

        global_state::settings().show.explorer_0 = true;
        (void) global_state::settings().save_to_disk();

        expl.scroll_to_nth_selected_entry_next_frame = 0;
        imgui::SetWindowFocus(expl.name);
        return true;
    }
}
