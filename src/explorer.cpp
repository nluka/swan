
#include "imgui/imgui.h"
#include "libs/thread_pool.hpp"

#include "stdafx.hpp"
#include "common.hpp"
#include "imgui_specific.hpp"
#include "path.hpp"
#include "scoped_timer.hpp"
#include "util.hpp"

namespace imgui = ImGui;

static IShellLinkW *s_shell_link = nullptr;
static IPersistFile *s_persist_file_interface = nullptr;
static explorer_options s_explorer_options = {};
static BS::thread_pool s_change_notif_thread_pool(1);

bool explorer_init_windows_shell_com_garbage() noexcept
{
    try {
        // Initialize COM library
        HRESULT com_handle = CoInitialize(nullptr);
        if (FAILED(com_handle)) {
            debug_log("CoInitialize failed");
            return false;
        }

        com_handle = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkW, (LPVOID *)&s_shell_link);
        if (FAILED(com_handle)) {
            debug_log("CoCreateInstance failed");
            CoUninitialize();
            return false;
        }

        com_handle = s_shell_link->QueryInterface(IID_IPersistFile, (LPVOID *)&s_persist_file_interface);
        if (FAILED(com_handle)) {
            debug_log("failed to query IPersistFile interface");
            s_persist_file_interface->Release();
            CoUninitialize();
            return false;
        }

        return true;
    }
    catch (...) {
        return false;
    }
}

struct progress_sink : public IFileOperationProgressSink {
    HRESULT FinishOperations(HRESULT) override { debug_log("FinishOperations"); return S_OK; }
    HRESULT PauseTimer() override { debug_log("PauseTimer"); return S_OK; }
    HRESULT PostCopyItem(DWORD, IShellItem *, IShellItem *, LPCWSTR, HRESULT, IShellItem *) override { debug_log("PostCopyItem"); return S_OK; }
    HRESULT PostDeleteItem(DWORD, IShellItem *, HRESULT, IShellItem *) override { debug_log("PostDeleteItem"); return S_OK; }
    HRESULT PostMoveItem(DWORD, IShellItem *, IShellItem *, LPCWSTR, HRESULT, IShellItem *) override { debug_log("PostMoveItem"); return S_OK; }
    HRESULT PostNewItem(DWORD, IShellItem *, LPCWSTR, LPCWSTR, DWORD, HRESULT, IShellItem *) override { debug_log("PostNewItem"); return S_OK; }
    HRESULT PostRenameItem(DWORD, IShellItem *, LPCWSTR, HRESULT, IShellItem *) override { debug_log("PostRenameItem"); return S_OK; }
    HRESULT PreCopyItem(DWORD, IShellItem *, IShellItem *, LPCWSTR) override { debug_log("PreCopyItem"); return S_OK; }
    HRESULT PreDeleteItem(DWORD, IShellItem *) override { debug_log("PreDeleteItem"); return S_OK; }
    HRESULT PreMoveItem(DWORD, IShellItem *, IShellItem *, LPCWSTR) override { debug_log("PreMoveItem"); return S_OK; }
    HRESULT PreNewItem(DWORD, IShellItem *, LPCWSTR) override { debug_log("PreNewItem"); return S_OK; }
    HRESULT PreRenameItem(DWORD, IShellItem *, LPCWSTR) override { debug_log("PreRenameItem"); return S_OK; }
    HRESULT ResetTimer() override { debug_log("ResetTimer"); return S_OK; }
    HRESULT ResumeTimer() override { debug_log("ResumeTimer"); return S_OK; }
    HRESULT StartOperations() override { debug_log("StartOperations"); return S_OK; }
    HRESULT UpdateProgress(UINT work_total, UINT work_so_far) override { debug_log("UpdateProgress %zu/%zu", work_so_far, work_total); return S_OK; }

    ULONG AddRef() { return 1; }
    ULONG Release() { return 1; }

    HRESULT QueryInterface(const IID &riid, void **ppv)
    {
        if (riid == IID_IUnknown || riid == IID_IFileOperationProgressSink) {
            *ppv = static_cast<IFileOperationProgressSink*>(this);
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
};

explorer_options &get_explorer_options() noexcept
{
    return s_explorer_options;
}

bool explorer_options::save_to_disk() const noexcept
{
    try {
        std::ofstream out("data/explorer_options.txt", std::ios::binary);

        if (!out) {
            return false;
        }

        static_assert(s8(1) == s8(true));
        static_assert(s8(0) == s8(false));

        out << "auto_refresh_interval_ms " << this->auto_refresh_interval_ms << '\n';
        out << "ref_mode " << (s32)this->ref_mode << '\n';
        out << "binary_size_system " << (s32)this->binary_size_system << '\n';
        out << "show_cwd_len " << (s32)this->show_cwd_len << '\n';
        out << "show_debug_info " << (s32)this->show_debug_info << '\n';
        out << "show_dotdot_dir " << (s32)this->show_dotdot_dir << '\n';
        out << "unix_directory_separator " << (s32)this->unix_directory_separator << '\n';

        return true;
    }
    catch (...) {
        return false;
    }
}

bool explorer_options::load_from_disk() noexcept
{
    try {
        std::ifstream in("data/explorer_options.txt", std::ios::binary);
        if (!in) {
            return false;
        }

        static_assert(s8(1) == s8(true));
        static_assert(s8(0) == s8(false));

        std::string what = {};
        what.reserve(100);
        char bit_ch = 0;

        {
            in >> what;
            assert(what == "auto_refresh_interval_ms");
            in >> (s32 &)this->auto_refresh_interval_ms;
        }
        {
            in >> what;
            assert(what == "ref_mode");
            in >> (s32 &)this->ref_mode;
        }
        {
            in >> what;
            assert(what == "binary_size_system");
            in >> bit_ch;
            this->binary_size_system = bit_ch == '1' ? 1 : 0;
        }
        {
            in >> what;
            assert(what == "show_cwd_len");
            in >> bit_ch;
            this->show_cwd_len = bit_ch == '1' ? 1 : 0;
        }
        {
            in >> what;
            assert(what == "show_debug_info");
            in >> bit_ch;
            this->show_debug_info = bit_ch == '1' ? 1 : 0;
        }
        {
            in >> what;
            assert(what == "show_dotdot_dir");
            in >> bit_ch;
            this->show_dotdot_dir = bit_ch == '1' ? 1 : 0;
        }
        {
            in >> what;
            assert(what == "unix_directory_separator");
            in >> bit_ch;
            this->unix_directory_separator = bit_ch == '1' ? 1 : 0;
        }

        return true;
    }
    catch (...) {
        return false;
    }
}

drive_list_t query_drive_list() noexcept
{
    drive_list_t drive_list;

    s32 drives_mask = GetLogicalDrives();

    for (u64 i = 0; i < 26; ++i) {
        if (drives_mask & (1 << i)) {
            char letter = 'A' + (char)i;

            wchar_t drive_root[] = { wchar_t(letter), L':', L'\\', L'\0' };
            wchar_t volume_name[MAX_PATH + 1];          init_empty_cstr(volume_name);
            wchar_t filesystem_name_utf8[MAX_PATH + 1]; init_empty_cstr(filesystem_name_utf8);
            DWORD serial_num = 0;
            DWORD max_component_length = 0;
            DWORD filesystem_flags = 0;
            s32 utf_written = 0;

            auto vol_info_result = GetVolumeInformationW(
                drive_root, volume_name, lengthof(volume_name),
                &serial_num, &max_component_length, &filesystem_flags,
                filesystem_name_utf8, lengthof(filesystem_name_utf8)
            );

            ULARGE_INTEGER total_bytes;
            ULARGE_INTEGER free_bytes;

            if (vol_info_result) {
                auto space_result = GetDiskFreeSpaceExW(drive_root, nullptr, &total_bytes, &free_bytes);
                if (space_result) {
                    drive_info info = {};
                    info.letter = letter;
                    info.total_bytes = total_bytes.QuadPart;
                    info.available_bytes = free_bytes.QuadPart;
                    utf_written = utf16_to_utf8(volume_name, info.name_utf8, lengthof(info.name_utf8));
                    utf_written = utf16_to_utf8(filesystem_name_utf8, info.filesystem_name_utf8, lengthof(info.filesystem_name_utf8));
                    drive_list.push_back(info);
                }
            }
        }
    }

    return drive_list;
}

void explorer_cleanup_windows_shell_com_garbage() noexcept
{
    try {
        s_persist_file_interface->Release();
        s_shell_link->Release();
        CoUninitialize();
    }
    catch (...) {}
}

struct paste_payload
{
    struct item
    {
        u64 size;
        basic_dirent::kind type;
        swan_path_t path;
    };

    std::vector<item> items = {};
    char const *window_name = nullptr;
    u64 bytes = 0;
    bool keep_src = {}; // false = cut, true = copy
    bool has_directories = false; // if true, `items` contains directories
};

static paste_payload s_paste_payload = {};

void explorer_window::deselect_all_cwd_entries() noexcept
{
    for (auto &dirent : this->cwd_entries) {
        dirent.is_selected = false;
    }
}

void explorer_window::select_all_cwd_entries(bool select_dotdot_dir) noexcept
{
    for (auto &dirent : this->cwd_entries) {
        if (!select_dotdot_dir && dirent.basic.is_dotdot()) {
            continue;
        } else {
            dirent.is_selected = true;
        }
    }
}

void explorer_window::set_latest_valid_cwd_then_notify(swan_path_t const &new_val) noexcept
{
    {
        std::scoped_lock lock(this->latest_valid_cwd_mutex);
        this->latest_valid_cwd = new_val;
    }
    this->latest_valid_cwd_cond.notify_one();
}

static
std::pair<s32, std::array<char, 64>> filetime_to_string(FILETIME *time) noexcept
{
    std::array<char, 64> buffer;
    DWORD flags = FDTF_SHORTDATE | FDTF_SHORTTIME;
    s32 length = SHFormatDateTimeA(time, &flags, buffer.data(), (u32)buffer.size());

    // for some reason, SHFormatDateTimeA will pad parts of the string with ASCII 63 (?)
    // when using LONGDATE or LONGTIME, so we will simply replace them with spaces
    std::replace(buffer.begin(), buffer.end(), '?', ' ');

    return { length, buffer };
}

struct generic_result
{
    bool success;
    std::string error_or_utf8_path;
};

generic_result delete_selected_entries(
    explorer_window const &expl,
    std::vector<explorer_window::dirent> const &selection,
    wchar_t dir_sep_utf16) noexcept
{
    {
        SHQUERYRBINFO recycle_bin_info;
        recycle_bin_info.cbSize = sizeof(recycle_bin_info);

        auto result = SHQueryRecycleBinW(nullptr, &recycle_bin_info);

        if (result == S_OK) {
            u64 used_bytes = recycle_bin_info.i64Size;
            u64 num_items = recycle_bin_info.i64NumItems;

            u64 multiplier = get_explorer_options().size_unit_multiplier();

            char used[32]; init_empty_cstr(used);
            format_file_size(used_bytes,  used,  lengthof(used),  multiplier);

            debug_log("RecycleBin size: %zu (%s), %zu items", used_bytes, used, num_items);
        }
        else {
            debug_log("SHQueryRecycleBinW failed: %ld", (s64)result);
        }
    }

    HRESULT result = {};

    IFileOperation *file_op = nullptr;

    result = CoCreateInstance(CLSID_FileOperation, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&file_op));
    if (FAILED(result)) {
        return { false, "CoCreateInstance" };
    }

    result = file_op->SetOperationFlags(FOF_ALLOWUNDO | FOF_NOCONFIRMATION);
    if (FAILED(result)) {
        file_op->Release();
        return { false, "IFileOperation::SetOperationFlags" };
    }

    s32 written = 0;
    wchar_t cwd_utf16[MAX_PATH]; init_empty_cstr(cwd_utf16);
    std::wstring delete_full_path_utf16 = {};
    delete_full_path_utf16.reserve(2048);

    written = utf8_to_utf16(expl.cwd.data(), cwd_utf16, lengthof(cwd_utf16));
    if (written == 0) {
        debug_log("FAILED utf8_to_utf16(expl.data -> cwd_utf16)");
        return { false, "conversion of cwd path from UTF-8 to UTF-16" };
    }

    for (auto const item : selection) {
        wchar_t item_utf16[MAX_PATH]; init_empty_cstr(item_utf16);

        written = utf8_to_utf16(item.basic.path.data(), item_utf16, lengthof(item_utf16));
        if (written == 0) {
            debug_log("FAILED utf8_to_utf16(item.basic.path -> item_utf16)");
            std::stringstream err;
            err << "conversion of [" << item.basic.path.data() << "] from UTF-8 to UTF-16";
            return { false, err.str() };
        }

        delete_full_path_utf16 = cwd_utf16;
        if (!delete_full_path_utf16.ends_with(dir_sep_utf16)) {
            delete_full_path_utf16 += dir_sep_utf16;
        }
        delete_full_path_utf16 += item_utf16;

        // shlwapi doesn't like '/', force them all to '\'
        if (get_explorer_options().unix_directory_separator) {
            std::replace(delete_full_path_utf16.begin(), delete_full_path_utf16.end(), L'/', L'\\');
        }

        IShellItem *to_delete = nullptr;
        result = SHCreateItemFromParsingName(delete_full_path_utf16.c_str(), nullptr, IID_PPV_ARGS(&to_delete));
        if (FAILED(result)) {
            debug_log("FAILED SHCreateItemFromParsingName");
            file_op->Release();
            std::stringstream err;
            err << "SHCreateItemFromParsingName [" << item.basic.path.data() << "]";
            WCOUT_IF_DEBUG("FAILED: SHCreateItemFromParsingName [" << delete_full_path_utf16.c_str() << "]\n");
            return { false, err.str() };
        }

        result = file_op->DeleteItem(to_delete, nullptr);
        if (FAILED(result)) {
            debug_log("FAILED file_op->DeleteItem");
            to_delete->Release();
            file_op->Release();
            std::stringstream err;
            err << "IFileOperation::DeleteItem [" << item.basic.path.data() << "]";
            WCOUT_IF_DEBUG("FAILED: IFileOperation::DeleteItem [" << delete_full_path_utf16.c_str() << "]\n");
            return { false, err.str() };
        } else {
            WCOUT_IF_DEBUG("file_op->DeleteItem [" << delete_full_path_utf16.c_str() << "]\n");
            to_delete->Release();
        }
    }

    auto file_operation_task = [](IFileOperation *file_op) {
        HRESULT result = {};

        result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        if (FAILED(result)) {
            debug_log("file_operation_task CoInitializeEx failed");
            return;
        }

        progress_sink prog_sink;
        DWORD cookie = {};

        result = file_op->Advise(&prog_sink, &cookie);
        if (FAILED(result)) {
            debug_log("file_operation_task file_op->Advise failed");
            file_op->Release();
            CoUninitialize();
            return;
        }

        result = file_op->PerformOperations();
        if (FAILED(result)) {
            debug_log("file_operation_task file_op->PerformOperations failed");
            file_op->Release();
            CoUninitialize();
            return;
        }

        file_op->Unadvise(cookie);
        if (FAILED(result)) {
            debug_log("file_operation_task file_op->Unadvise failed");
        }

        file_op->Release();
        CoUninitialize();
    };

#if 1
    // TODO:
    get_thread_pool().push_task(file_operation_task, file_op);
#else
    // file_operation_task(file_op);
#endif

    return { true, "" };
}

generic_result reveal_in_file_explorer(explorer_window::dirent const &entry, explorer_window const &expl, wchar_t dir_sep_utf16) noexcept
{
    wchar_t select_path_cwd_buffer_utf16[MAX_PATH];     init_empty_cstr(select_path_cwd_buffer_utf16);
    wchar_t select_path_dirent_buffer_utf16[MAX_PATH];  init_empty_cstr(select_path_dirent_buffer_utf16);
    std::wstring select_command = {};
    s32 utf_written = 0;

    select_command.reserve(1024);

    utf_written = utf8_to_utf16(expl.cwd.data(), select_path_cwd_buffer_utf16, lengthof(select_path_cwd_buffer_utf16));

    if (utf_written == 0) {
        debug_log("[ %d ] utf8_to_utf16 failed (expl.cwd -> select_path_cwd_buffer_utf16)", expl.id);
        return { false, "conversion of cwd path from UTF-8 to UTF-16" };
    }

    select_command += L"/select,";
    select_command += L'"';
    select_command += select_path_cwd_buffer_utf16;
    if (!select_command.ends_with(dir_sep_utf16)) {
        select_command += dir_sep_utf16;
    }

    utf_written = utf8_to_utf16(entry.basic.path.data(), select_path_dirent_buffer_utf16, lengthof(select_path_dirent_buffer_utf16));

    if (utf_written == 0) {
        debug_log("[ %d ] utf8_to_utf16 failed (entry.basic.path -> select_path_dirent_buffer_utf16)", expl.id);
        return { false, "conversion of selected entry's path from UTF-8 to UTF-16" };
    }

    select_command += select_path_dirent_buffer_utf16;
    select_command += L'"';

    WCOUT_IF_DEBUG("select_command: [" << select_command.c_str() << "]\n");

    HINSTANCE result = ShellExecuteW(nullptr, L"open", L"explorer.exe", select_command.c_str(), nullptr, SW_SHOWNORMAL);

    if ((intptr_t)result > HINSTANCE_ERROR) {
        return { true, "" };
    } else {
        return { false, get_last_error_string() };
    }
}

generic_result open_file(explorer_window::dirent const &file, explorer_window &expl, char dir_sep_utf8) noexcept
{
    swan_path_t target_full_path_utf8 = expl.cwd;

    if (!path_append(target_full_path_utf8, file.basic.path.data(), dir_sep_utf8, true)) {
        debug_log("[ %d ] path_append failed, cwd = [%s], append data = [\\%s]", expl.id, expl.cwd.data(), file.basic.path.data());
        return { false, "max path length exceeded when appending target name to cwd" };
    }

    wchar_t target_full_path_utf16[MAX_PATH]; init_empty_cstr(target_full_path_utf16);

    s32 utf_written = utf8_to_utf16(target_full_path_utf8.data(), target_full_path_utf16, lengthof(target_full_path_utf16));

    if (utf_written == 0) {
        debug_log("[ %d ] utf8_to_utf16 failed: target_full_path_utf8 -> target_full_path_utf16", expl.id);
        return { false, "conversion of target's full path from UTF-8 to UTF-16" };
    }

    debug_log("[ %d ] utf8_to_utf16 wrote %d characters (target_full_path_utf8 -> target_full_path_utf16)", expl.id, utf_written);
    debug_log("[ %d ] target_full_path = [%s]", expl.id, target_full_path_utf8.data());

    HINSTANCE result = ShellExecuteW(nullptr, L"open", target_full_path_utf16, nullptr, nullptr, SW_SHOWNORMAL);

    auto ec = (intptr_t)result;

    if (ec > HINSTANCE_ERROR) {
        debug_log("[ %d ] ShellExecuteW success", expl.id);
        return { true, "" };
    }
    else if (ec == SE_ERR_NOASSOC) {
        debug_log("[ %d ] ShellExecuteW: SE_ERR_NOASSOC", expl.id);
        return { false, "no association between file type and program (ShellExecuteW: SE_ERR_NOASSOC)" };
    }
    else if (ec == SE_ERR_FNF) {
        debug_log("[ %d ] ShellExecuteW: SE_ERR_FNF", expl.id);
        return { false, "file not found (ShellExecuteW: SE_ERR_FNF)" };
    }
    else {
        auto err = get_last_error_string();
        debug_log("[ %d ] ShellExecuteW: %s", expl.id, err.c_str());
        return { false, err };
    }
}

generic_result open_symlink(explorer_window::dirent const &dirent, explorer_window &expl, char dir_sep_utf8) noexcept
{
    swan_path_t symlink_self_path_utf8 = expl.cwd;
    swan_path_t symlink_target_path_utf8 = {};
    wchar_t symlink_self_path_utf16[MAX_PATH];      init_empty_cstr(symlink_self_path_utf16);
    wchar_t symlink_target_path_utf16[MAX_PATH];    init_empty_cstr(symlink_target_path_utf16);
    wchar_t working_dir_utf16[MAX_PATH];            init_empty_cstr(working_dir_utf16);
    wchar_t command_line_utf16[2048];               init_empty_cstr(command_line_utf16);
    s32 show_command = SW_SHOWNORMAL;
    HRESULT com_handle = {};
    LPITEMIDLIST item_id_list = nullptr;
    s32 utf_written = {};

    if (!path_append(symlink_self_path_utf8, dirent.basic.path.data(), dir_sep_utf8, true)) {
        debug_log("[ %d ] path_append(symlink_self_path_utf8, dir_ent.basic.path.data() failed", expl.id);
        return { false, "max path length exceeded when appending symlink name to cwd" };
    }

    debug_log("[ %d ] double clicked link [%s]", expl.id, symlink_self_path_utf8);

    utf_written = utf8_to_utf16(symlink_self_path_utf8.data(), symlink_self_path_utf16, lengthof(symlink_self_path_utf16));

    if (utf_written == 0) {
        debug_log("[ %d ] utf8_to_utf16 failed: symlink_self_path_utf8 -> symlink_self_path_utf16", expl.id);
        return { false, "conversion of symlink path from UTF-8 to UTF-16" };
    } else {
        debug_log("[ %d ] utf8_to_utf16 wrote %d characters", expl.id, utf_written);
        WCOUT_IF_DEBUG("symlink_self_path_utf16 = [" << symlink_self_path_utf16 << "]\n");
    }

    com_handle = s_persist_file_interface->Load(symlink_self_path_utf16, STGM_READ);

    if (com_handle != S_OK) {
        debug_log("[ %d ] s_persist_file_interface->Load [%s] failed: %s", expl.id, symlink_self_path_utf8, get_last_error_string().c_str());
        return { false, "conversion of symlink path from UTF-8 to UTF-16" };
    } else {
        WCOUT_IF_DEBUG("s_persist_file_interface->Load [" << symlink_self_path_utf16 << "]\n");
    }

    com_handle = s_shell_link->GetIDList(&item_id_list);

    if (com_handle != S_OK) {
        auto err = get_last_error_string();
        debug_log("[ %d ] s_shell_link->GetIDList failed: %s", expl.id, err.c_str());
        return { false, err + " (IShellLinkW::GetIDList)" };
    }

    if (!SHGetPathFromIDListW(item_id_list, symlink_target_path_utf16)) {
        auto err = get_last_error_string();
        debug_log("[ %d ] SHGetPathFromIDListW failed: %s", expl.id, err.c_str());
        return { false, err + " (SHGetPathFromIDListW)" };
    } else {
        WCOUT_IF_DEBUG("symlink_target_path_utf16 = [" << symlink_target_path_utf16 << "]\n");
    }

    if (com_handle != S_OK) {
        auto err = get_last_error_string();
        debug_log("[ %d ] s_shell_link->GetPath failed: %s", expl.id, err.c_str());
        return { false, err + " (IShellLinkW::GetWorkingDirectory)" };
    }

    utf_written = utf16_to_utf8(symlink_target_path_utf16, symlink_target_path_utf8.data(), symlink_target_path_utf8.size());

    if (utf_written == 0) {
        debug_log("[ %d ] utf16_to_utf8 failed: symlink_target_path_utf16 -> symlink_target_path_utf8", expl.id);
        return { false, "conversion of symlink target path from UTF-16 to UTF-8" };
    } else {
        debug_log("[ %d ] utf16_to_utf8 wrote %d characters", expl.id, utf_written);
        debug_log("[ %d ] symlink_target_path_utf8 = [%s]", expl.id, symlink_target_path_utf8.data());
    }

    if (directory_exists(symlink_target_path_utf8.data())) {
        // symlink to a directory, tell caller to navigate there
        return { true, symlink_target_path_utf8.data() };
    }
    else {
        // symlink to a file, let's open it

        com_handle = s_shell_link->GetWorkingDirectory(working_dir_utf16, lengthof(working_dir_utf16));

        if (com_handle != S_OK) {
            auto err = get_last_error_string();
            debug_log("[ %d ] s_shell_link->GetWorkingDirectory failed: %s", expl.id, err.c_str());
            return { false, err + " (IShellLinkW::GetWorkingDirectory)" };
        } else {
            WCOUT_IF_DEBUG("s_shell_link->GetWorkingDirectory [" << working_dir_utf16 << "]\n");
        }

        com_handle = s_shell_link->GetArguments(command_line_utf16, lengthof(command_line_utf16));

        if (com_handle != S_OK) {
            auto err = get_last_error_string();
            debug_log("[ %d ] s_shell_link->GetArguments failed: %s", expl.id, err.c_str());
            return { false, err + " (IShellLinkW::GetArguments)" };
        } else {
            WCOUT_IF_DEBUG("s_shell_link->GetArguments [" << command_line_utf16 << "]\n");
        }

        com_handle = s_shell_link->GetShowCmd(&show_command);

        if (com_handle != S_OK) {
            auto err = get_last_error_string();
            debug_log("[ %d ] s_shell_link->GetShowCmd failed: %s", expl.id, err.c_str());
            return { false, err + " (IShellLinkW::GetShowCmd)" };
        } else {
            WCOUT_IF_DEBUG("s_shell_link->GetShowCmd [" << show_command << "]\n");
        }

        HINSTANCE result = ShellExecuteW(nullptr, L"open", symlink_target_path_utf16,
                                         command_line_utf16, working_dir_utf16, show_command);

        intptr_t err_code = (intptr_t)result;

        if (err_code > HINSTANCE_ERROR) {
            debug_log("[ %d ] ShellExecuteW success", expl.id);
            return { true, "" };
        }
        else if (err_code == SE_ERR_NOASSOC) {
            debug_log("[ %d ] ShellExecuteW error: SE_ERR_NOASSOC", expl.id);
            return { false, "no association between file type and program (ShellExecuteW: SE_ERR_NOASSOC)" };
        }
        else if (err_code == SE_ERR_FNF) {
            debug_log("[ %d ] ShellExecuteW error: SE_ERR_FNF", expl.id);
            return { false, "file not found (ShellExecuteW: SE_ERR_FNF)" };
        }
        else {
            auto err = get_last_error_string();
            debug_log("[ %d ] ShellExecuteW error: unexpected error", expl.id);
            return { false, err };
        }
    }
}

enum cwd_entries_table_col : ImGuiID
{
    cwd_entries_table_col_number,
    cwd_entries_table_col_id,
    cwd_entries_table_col_path,
    cwd_entries_table_col_type,
    cwd_entries_table_col_size_pretty,
    cwd_entries_table_col_size_bytes,
    // cwd_entries_table_col_creation_time,
    cwd_entries_table_col_last_write_time,
    cwd_entries_table_col_count
};


/// @brief Sorts `expl.cwd_entries` in place according to `expl.sort_specs`.
/// Entries where `is_filtered_out` is true are forced to a contiguous block at the back of the vector.
/// The result is 2 distinct halves. the first half [expl.cwd_entries.begin(), retval) are entries which are unfiltered (to be shown),
/// sorted according to `expl.sort_specs`. The second half [retval, expl.cwd_entries.end()) are entries which are filtered (not to be shown).
/// @return Iterator to the first filtered entry if there is one, `cwd_entries.end()` otherwise.
static std::vector<explorer_window::dirent>::iterator
sort_cwd_entries(explorer_window &expl, std::source_location sloc = std::source_location::current()) noexcept
{
    ImGuiTableSortSpecs const *sort_specs = expl.sort_specs;
    assert(sort_specs != nullptr);

    auto &cwd_entries = expl.cwd_entries;

    debug_log("[ %d ] sort_cwd_entries() called from [%s:%d]", expl.id, cget_file_name(sloc.file_name()), sloc.line());

    using dir_ent_t = explorer_window::dirent;

    // move all filtered entries to the back of the vector
    std::sort(cwd_entries.begin(), cwd_entries.end(), [](dir_ent_t const &left, dir_ent_t const &right) {
        return left.is_filtered_out < right.is_filtered_out;
    });

    auto first_filtered_dirent = std::find_if(cwd_entries.begin(), cwd_entries.end(), [](dir_ent_t const &ent) {
        return ent.is_filtered_out;
    });

    // preliminary sort to ensure deterministic behaviour regardless of initial state.
    // necessary or else auto refresh can cause unexpected reordering of directory entries.
    std::sort(cwd_entries.begin(), first_filtered_dirent, [](dir_ent_t const &left, dir_ent_t const &right) {
        // return lstrcmpiA(left.basic.path.data(), right.basic.path.data()) < 0;
        return left.basic.id < right.basic.id;
    });

    // comparator needs to return true when left < right
    std::sort(cwd_entries.begin(), first_filtered_dirent, [&](dir_ent_t const &left, dir_ent_t const &right) {
        bool left_lt_right = false;

        for (s32 i = 0; i < sort_specs->SpecsCount; ++i) {
            auto const &sort_spec = sort_specs->Specs[i];

            // comparing with this variable using == will handle the sort direction
            bool direction_flipper = sort_spec.SortDirection == ImGuiSortDirection_Ascending ? false : true;

            switch (sort_spec.ColumnUserID) {
                default:
                case cwd_entries_table_col_id: {
                    left_lt_right = (left.basic.id < right.basic.id) == direction_flipper;
                    break;
                }
                case cwd_entries_table_col_path: {
                    left_lt_right = (lstrcmpiA(left.basic.path.data(), right.basic.path.data()) < 0) == direction_flipper;
                    break;
                }
                case cwd_entries_table_col_type: {
                    auto compute_precedence = [](explorer_window::dirent const &ent) -> u32 {
                        // lower items (and thus higher values) have greater precedence
                        enum class precedence : u32
                        {
                            everything_else,
                            symlink,
                            directory,
                        };

                        if      (ent.basic.is_directory()) return (u32)precedence::directory;
                        else if (ent.basic.is_symlink())   return (u32)precedence::symlink;
                        else                               return (u32)precedence::everything_else;
                    };

                    u32 left_precedence = compute_precedence(left);
                    u32 right_precedence = compute_precedence(right);

                    left_lt_right = (left_precedence > right_precedence) == direction_flipper;
                    break;
                }
                case cwd_entries_table_col_size_pretty:
                case cwd_entries_table_col_size_bytes: {
                    if (left.basic.is_directory() && right.basic.is_file() && right.basic.size == 0) {
                        left_lt_right = true == direction_flipper;
                    } else {
                        left_lt_right = (left.basic.size < right.basic.size) == direction_flipper;
                    }
                    break;
                }
                // TODO: reintroduce column
                // case cwd_entries_table_col_creation_time: {
                //     s32 cmp = CompareFileTime(&left.creation_time_raw, &right.creation_time_raw);
                //     left_lt_right = (cmp <= 0) == direction_flipper;
                //     break;
                // }
                case cwd_entries_table_col_last_write_time: {
                    s32 cmp = CompareFileTime(&left.basic.last_write_time_raw, &right.basic.last_write_time_raw);
                    left_lt_right = (cmp <= 0) == direction_flipper;
                    break;
                }
            }
        }

        return left_lt_right;
    });

    return first_filtered_dirent;
}

bool update_cwd_entries(
    update_cwd_entries_actions actions,
    explorer_window *expl_ptr,
    std::string_view parent_dir,
    std::source_location sloc) noexcept
{
    debug_log("[ %d ] update_cwd_entries(%d) called from [%s:%d]", expl_ptr->id, actions, cget_file_name(sloc.file_name()), sloc.line());

    IM_ASSERT(expl_ptr != nullptr);

    scoped_timer<timer_unit::MICROSECONDS> function_timer(&expl_ptr->update_cwd_entries_total_us);

    bool parent_dir_exists = false;

    char dir_sep_utf8 = get_explorer_options().dir_separator_utf8();

    explorer_window &expl = *expl_ptr;
    expl.update_cwd_entries_total_us = 0;
    expl.update_cwd_entries_searchpath_setup_us = 0;
    expl.update_cwd_entries_filesystem_us = 0;
    expl.update_cwd_entries_filter_us = 0;
    expl.update_cwd_entries_regex_ctor_us = 0;

    if (actions & query_filesystem) {
        static std::vector<swan_path_t> selected_entries = {};
        selected_entries.clear();

        for (auto const &dirent : expl.cwd_entries) {
            if (dirent.is_selected) {
                // this could throw on alloc failure, which will call std::terminate
                selected_entries.push_back(dirent.basic.path);
            }
        }

        expl.cwd_entries.clear();

        while (parent_dir.ends_with(' ')) {
            parent_dir = std::string_view(parent_dir.data(), parent_dir.size() - 1);
        }

        if (parent_dir != "") {
            wchar_t search_path_utf16[512]; init_empty_cstr(search_path_utf16);
            {
                scoped_timer<timer_unit::MICROSECONDS> search_swan_path_timer(&expl.update_cwd_entries_searchpath_setup_us);

                utf8_to_utf16(parent_dir.data(), search_path_utf16, lengthof(search_path_utf16));

                wchar_t dir_sep_w[] = { (wchar_t)dir_sep_utf8, L'\0' };

                if (!parent_dir.ends_with(dir_sep_utf8)) {
                    (void) StrCatW(search_path_utf16, dir_sep_w);
                }
                (void) StrCatW(search_path_utf16, L"*");
            }

            {
                char utf8_buffer[2048]; init_empty_cstr(utf8_buffer);

                u64 utf_written = utf16_to_utf8(search_path_utf16, utf8_buffer, lengthof(utf8_buffer));

                if (utf_written == 0) {
                    debug_log("[ %d ] utf16_to_utf8 failed (search_path_utf16 -> utf8_buffer)", expl.id);
                    goto exit_update_cwd_entries;
                }

                debug_log("[ %d ] querying filesystem, search_path = [%s]", expl.id, utf8_buffer);
            }

            scoped_timer<timer_unit::MICROSECONDS> filesystem_timer(&expl.update_cwd_entries_filesystem_us);

            WIN32_FIND_DATAW find_data;
            HANDLE find_handle = FindFirstFileW(search_path_utf16, &find_data);

            SCOPE_EXIT { FindClose(find_handle); };
            // auto find_handle_cleanup_routine = make_on_scope_exit([&find_handle] { FindClose(find_handle); });

            if (find_handle == INVALID_HANDLE_VALUE) {
                debug_log("[ %d ] find_handle == INVALID_HANDLE_VALUE", expl.id);
                parent_dir_exists = false;
                goto exit_update_cwd_entries;
            } else {
                parent_dir_exists = true;
            }

            u32 id = 0;

            do {
                explorer_window::dirent entry = {};
                entry.basic.id = id;
                entry.basic.size = two_u32_to_one_u64(find_data.nFileSizeLow, find_data.nFileSizeHigh);
                entry.basic.creation_time_raw = find_data.ftCreationTime;
                entry.basic.last_write_time_raw = find_data.ftLastWriteTime;

                {
                    u64 utf_written = utf16_to_utf8(find_data.cFileName, entry.basic.path.data(), entry.basic.path.size());

                    if (utf_written == 0) {
                        debug_log("[ %d ] utf16_to_utf8 failed (find_data.cFileName -> entry.basic.path)", expl.id);
                        continue;
                    }
                }

                if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    entry.basic.type = basic_dirent::kind::directory;
                }
                else if (path_ends_with(entry.basic.path, ".lnk")) {
                    entry.basic.type = basic_dirent::kind::symlink;
                }
                else {
                    entry.basic.type = basic_dirent::kind::file;
                }

                if (path_equals_exactly(entry.basic.path, ".")) {
                    continue;
                }

                if (entry.basic.is_dotdot()) {
                    if (get_explorer_options().show_dotdot_dir) {
                        expl.cwd_entries.emplace_back(entry);
                        std::swap(expl.cwd_entries.back(), expl.cwd_entries.front());
                    }
                } else {
                    for (auto const &prev_selected_entry : selected_entries) {
                        bool was_selected_before_refresh = path_equals_exactly(entry.basic.path, prev_selected_entry);
                        if (was_selected_before_refresh) {
                            entry.is_selected = true;
                        }
                    }

                    // this could throw on alloc failure, which will call std::terminate
                    expl.cwd_entries.emplace_back(entry);
                }

                ++expl.num_file_finds;
                ++id;
            }
            while (FindNextFileW(find_handle, &find_data));

            if (expl.sort_specs != nullptr) {
                scoped_timer<timer_unit::MICROSECONDS> sort_timer(&expl.sort_us);
                (void) sort_cwd_entries(expl);
            }
        }
    }

    if (actions & filter) {
        scoped_timer<timer_unit::MICROSECONDS> filter_timer(&expl.update_cwd_entries_filter_us);

        expl.filter_error.clear();

        for (auto &dirent : expl.cwd_entries) {
            dirent.is_filtered_out = false;
        }

        bool filter_is_blank = expl.filter.data()[0] == '\0';

        if (!filter_is_blank) {
            static std::regex filter_regex;

            switch (expl.filter_mode) {
                default:
                case explorer_window::filter_mode::contains: {
                    auto matcher = expl.filter_case_sensitive ? StrStrA : StrStrIA;

                    for (auto &dirent : expl.cwd_entries) {
                        char const *path = dirent.basic.path.data();
                        bool filtered_out = expl.filter_polarity != (bool)matcher(path, expl.filter.data());
                        dirent.is_filtered_out = filtered_out;
                    }
                    break;
                }

                case explorer_window::filter_mode::regex: {
                    try {
                        scoped_timer<timer_unit::MICROSECONDS> regex_ctor_timer(&expl.update_cwd_entries_regex_ctor_us);
                        filter_regex = expl.filter.data();
                    }
                    catch (std::exception const &except) {
                        debug_log("[ %d ] error constructing std::regex, %s", expl.id, except.what());
                        expl.filter_error = except.what();
                        break;
                    }

                    auto match_flags = std::regex_constants::match_default | (
                        std::regex_constants::icase * (expl.filter_case_sensitive == 0)
                    );

                    for (auto &dirent : expl.cwd_entries) {
                        char const *path = dirent.basic.path.data();
                        bool filtered_out = expl.filter_polarity != std::regex_match(
                            path,
                            filter_regex,
                            (std::regex_constants::match_flag_type)match_flags
                        );
                        dirent.is_filtered_out = filtered_out;
                    }

                    break;
                }
            }
        }

        if (expl.sort_specs != nullptr) {
            sort_cwd_entries(expl);
        }
    }

exit_update_cwd_entries:
    expl.last_refresh_time = current_time();
    return parent_dir_exists;
}

bool explorer_window::save_to_disk() const noexcept
{
    scoped_timer<timer_unit::MICROSECONDS> save_to_disk_timer(&(this->save_to_disk_us));

    char file_name[32]; init_empty_cstr(file_name);
    s32 written = snprintf(file_name, lengthof(file_name), "data/Explorer_%c.txt", this->name[9]);
    assert(written < lengthof(file_name));

    bool result = true;

    try {
        std::ofstream out(file_name);
        if (!out) {
            result = false;
        } else {
            out << "cwd " << path_length(cwd) << ' ' << cwd.data() << '\n';

            out << "filter " << strlen(filter.data()) << ' ' << filter.data() << '\n';

            out << "filter_mode " << (s32)filter_mode << '\n';

            out << "filter_case_sensitive " << (s32)filter_case_sensitive << '\n';

            out << "filter_polarity " << (s32)filter_polarity << '\n';

            out << "wd_history_pos " << wd_history_pos << '\n';

            out << "wd_history.size() " << wd_history.size() << '\n';

            for (auto const &item : wd_history) {
                out << path_length(item) << ' ' << item.data() << '\n';
            }
        }
    }
    catch (...) {
        result = false;
    }

    debug_log("[%s] save attempted, result: %d", file_name, result);
    this->latest_save_to_disk_result = (s8)result;

    return result;
}

bool explorer_window::load_from_disk(char dir_separator) noexcept
{
    assert(this->name != nullptr);

    char file_name[32]; init_empty_cstr(file_name);
    s32 written = snprintf(file_name, lengthof(file_name), "data/Explorer_%c.txt", this->name[9]);
    assert(written < lengthof(file_name));

    try {
        std::ifstream in(file_name);
        if (!in) {
            debug_log("failed to open file [%s]", file_name);
            return false;
        }

        char whitespace = 0;
        std::string what = {};
        what.reserve(256);

        {
            in >> what;
            assert(what == "cwd");

            u64 cwd_len = 0;
            in >> cwd_len;
            debug_log("[%s] cwd_len = %zu", file_name, cwd_len);

            in.read(&whitespace, 1);

            in.read(cwd.data(), cwd_len);
            path_force_separator(cwd, dir_separator);
            debug_log("[%s] cwd = [%s]", file_name, cwd.data());
        }
        {
            in >> what;
            assert(what == "filter");

            u64 filter_len = 0;
            in >> filter_len;
            debug_log("[%s] filter_len = %zu", file_name, filter_len);

            in.read(&whitespace, 1);

            in.read(filter.data(), filter_len);
            debug_log("[%s] filter = [%s]", file_name, filter.data());
        }
        {
            in >> what;
            assert(what == "filter_mode");

            in >> (s32 &)filter_mode;
            debug_log("[%s] filter_mode = %d", file_name, filter_mode);
        }
        {
            in >> what;
            assert(what == "filter_case_sensitive");

            s32 val = 0;
            in >> val;

            filter_case_sensitive = (bool)val;
            debug_log("[%s] filter_case_sensitive = %d", file_name, filter_case_sensitive);
        }
        {
            in >> what;
            assert(what == "filter_polarity");

            s32 val = 0;
            in >> val;

            filter_polarity = (bool)val;
            debug_log("[%s] filter_polarity = %d", file_name, filter_polarity);
        }
        {
            in >> what;
            assert(what == "wd_history_pos");

            in >> wd_history_pos;
            debug_log("[%s] wd_history_pos = %zu", file_name, wd_history_pos);
        }

        u64 wd_history_size = 0;
        {
            in >> what;
            assert(what == "wd_history.size()");

            in >> wd_history_size;
            debug_log("[%s] wd_history_size = %zu", file_name, wd_history_size);
        }

        wd_history.resize(wd_history_size);
        for (u64 i = 0; i < wd_history_size; ++i) {
            u64 item_len = 0;
            in >> item_len;

            in.read(&whitespace, 1);

            in.read(wd_history[i].data(), item_len);
            path_force_separator(wd_history[i], dir_separator);
            debug_log("[%s] history[%zu] = [%s]", file_name, i, wd_history[i].data());
        }
    }
    catch (...) {
        return false;
    }

    return true;
}

void new_history_from(explorer_window &expl, swan_path_t const &new_latest_entry)
{
    swan_path_t new_latest_entry_clean;
    new_latest_entry_clean = new_latest_entry;
    char dir_sep_utf8 = get_explorer_options().dir_separator_utf8();
    path_pop_back_if(new_latest_entry_clean, dir_sep_utf8);

    if (expl.wd_history.empty()) {
        expl.wd_history_pos = 0;
    }
    else {
        u64 num_trailing_history_items_to_del = expl.wd_history.size() - expl.wd_history_pos - 1;

        for (u64 i = 0; i < num_trailing_history_items_to_del; ++i) {
            expl.wd_history.pop_back();
        }

        if (expl.wd_history.size() == explorer_window::MAX_WD_HISTORY_SIZE) {
            expl.wd_history.pop_front();
        } else {
            ++expl.wd_history_pos;
        }
    }

    expl.wd_history.push_back(new_latest_entry_clean);
}

struct ascend_result
{
    bool success;
    swan_path_t parent_dir;
};

static
ascend_result try_ascend_directory(explorer_window &expl) noexcept
{
    char dir_sep_utf8 = get_explorer_options().dir_separator_utf8();
    ascend_result res = {};
    res.parent_dir = expl.cwd;

    // if there is a trailing separator, remove it
    path_pop_back_if(res.parent_dir, dir_sep_utf8);
    // remove anything between end and final separator
    while (path_pop_back_if_not(res.parent_dir, dir_sep_utf8));

    bool parent_dir_exists = update_cwd_entries(full_refresh, &expl, res.parent_dir.data());
    res.success = parent_dir_exists;
    debug_log("[ %d ] try_ascend_directory parent_dir=[%s] res.success=%d", expl.id, res.parent_dir.data(), res.success);

    if (parent_dir_exists) {
        if (!path_is_empty(expl.cwd)) {
            new_history_from(expl, res.parent_dir);
        }
        expl.filter_error.clear();
        expl.cwd_prev_selected_dirent_idx = explorer_window::NO_SELECTION;
        expl.cwd = res.parent_dir;
        (void) expl.save_to_disk();
        expl.set_latest_valid_cwd_then_notify(expl.cwd);
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
    char dir_sep_utf8 = get_explorer_options().dir_separator_utf8();
    // wchar_t dir_sep_utf16 = get_explorer_options().dir_separator_utf16();

    swan_path_t new_cwd_utf8 = expl.cwd;

    if (!path_append(new_cwd_utf8, target_utf8, dir_sep_utf8, true)) {
        debug_log("[ %d ] path_append failed, new_cwd_utf8 = [%s], append data = [%c%s]", expl.id, new_cwd_utf8.data(), dir_sep_utf8, target_utf8);
        descend_result res;
        res.success = false;
        res.err_msg = "max path length exceeded when trying to append descend target to current working directory path";
        return res;
    }

    wchar_t new_cwd_utf16[MAX_PATH]; init_empty_cstr(new_cwd_utf16);

    s32 utf_written = utf8_to_utf16(new_cwd_utf8.data(), new_cwd_utf16, lengthof(new_cwd_utf16));

    if (utf_written == 0) {
        debug_log("[ %d ] utf8_to_utf16 failed: new_cwd_utf8 -> new_cwd_utf16", expl.id);
        descend_result res;
        res.success = false;
        res.err_msg = "conversion of new cwd path from UTF-8 to UTF-16";
        return res;
    }

    wchar_t new_cwd_canonical_utf16[MAX_PATH]; init_empty_cstr(new_cwd_canonical_utf16);

    {
        HRESULT handle = PathCchCanonicalize(new_cwd_canonical_utf16, lengthof(new_cwd_canonical_utf16), new_cwd_utf16);
        if (handle != S_OK) {
            descend_result res;
            res.success = false;
            switch (handle) {
                case E_INVALIDARG: res.err_msg = "PathCchCanonicalize E_INVALIDARG - the cchPathOut value is > PATHCCH_MAX_CCH"; break;
                case E_OUTOFMEMORY: res.err_msg = "PathCchCanonicalize E_OUTOFMEMORY - the function could not allocate a buffer of the necessary size"; break;
                default: res.err_msg = "unknown PathCchCanonicalize error"; break;
            }
            return res;
        }
    }

    swan_path_t new_cwd_canoncial_utf8;

    utf_written = utf16_to_utf8(new_cwd_canonical_utf16, new_cwd_canoncial_utf8.data(), new_cwd_canoncial_utf8.size());

    if (utf_written == 0) {
        debug_log("[ %d ] utf16_to_utf8 failed: new_cwd_canonical_utf16 -> new_cwd_canoncial_utf8", expl.id);
        descend_result res;
        res.success = false;
        res.err_msg = "conversion of new canonical cwd path from UTF-8 to UTF-16";
        return res;
    }

    bool cwd_exists = update_cwd_entries(full_refresh, &expl, new_cwd_canoncial_utf8.data());

    if (!cwd_exists) {
        debug_log("[ %d ] descend target directory not found", expl.id);
        descend_result res;
        res.success = false;
        res.err_msg = std::string("descend target directory [") + target_utf8 + "] not found";
        return res;
    }

    new_history_from(expl, new_cwd_canoncial_utf8);
    expl.cwd = new_cwd_canoncial_utf8;
    expl.cwd_prev_selected_dirent_idx = explorer_window::NO_SELECTION;
    expl.filter_error.clear();
    (void) expl.save_to_disk();
    expl.set_latest_valid_cwd_then_notify(expl.cwd);

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
};

static
s32 cwd_text_input_callback(ImGuiInputTextCallbackData *data) noexcept
{
    auto user_data = (cwd_text_input_callback_user_data *)(data->UserData);
    user_data->edit_occurred = false;

    auto is_separator = [](wchar_t ch) { return ch == L'/' || ch == L'\\'; };

    if (data->EventFlag == ImGuiInputTextFlags_CallbackCharFilter) {
        if (is_separator(data->EventChar)) {
            data->EventChar = user_data->dir_sep_utf16;
        }
        else {
            static wchar_t const *forbidden_chars = L"<>\"|?*";
            bool is_forbidden = StrChrW(forbidden_chars, data->EventChar);
            if (is_forbidden) {
                data->EventChar = L'\0';
            }
        }
    }
    else if (data->EventFlag == ImGuiInputTextFlags_CallbackEdit) {
        debug_log("[ %d ] ImGuiInputTextFlags_CallbackEdit", user_data->expl_id);
        user_data->edit_occurred = true;
    }

    return 0;
}

static
void render_debug_info(explorer_window &expl, u64 size_unit_multiplier) noexcept
{
    imgui::Text("latest_valid_cwd = [%s]", expl.latest_valid_cwd.data());

#if 1
    {
        u64 bytes_occupied = expl.cwd_entries.size() * sizeof(swan_path_t);
        u64 bytes_actually_used = 0;

        for (auto const &dirent : expl.cwd_entries) {
            bytes_actually_used += path_length(dirent.basic.path);
        }

        char buffer1[32]; init_empty_cstr(buffer1);
        format_file_size(bytes_occupied, buffer1, lengthof(buffer1), size_unit_multiplier);

        f64 usage_ratio = f64(bytes_actually_used) / f64(bytes_occupied);
        f64 waste_percent = 100.0 - (usage_ratio * 100.0);
        u64 bytes_wasted = u64( bytes_occupied * (1.0 - usage_ratio) );

        char buffer2[32]; init_empty_cstr(buffer2);
        format_file_size(bytes_wasted, buffer2, lengthof(buffer1), size_unit_multiplier);

        imgui::Text("swan_path_t memory footprint = %s / %3.1lf %% waste -> %s", buffer1, waste_percent, buffer2);
    }
#endif

    if (imgui::BeginTable("explorer_timers", 3, ImGuiTableFlags_BordersInnerV|ImGuiTableFlags_Resizable)) {
        imgui::TableNextColumn();
        imgui::SeparatorText("misc. state");
        imgui::TextUnformatted("num_file_finds");
        imgui::TextUnformatted("cwd_prev_selected_dirent_idx");
        imgui::TextUnformatted("latest_save_to_disk_result");

        imgui::TableNextColumn();
        imgui::SeparatorText("");
        imgui::Text("%zu", expl.num_file_finds);
        imgui::Text("%lld", expl.cwd_prev_selected_dirent_idx);
        imgui::Text("%d", expl.latest_save_to_disk_result);

        imgui::TableNextColumn();
        imgui::SeparatorText("");

        imgui::TableNextColumn();
        imgui::SeparatorText("update_cwd_entries timers");
        imgui::TextUnformatted("total_us");
        imgui::TextUnformatted("searchpath_setup_us");
        imgui::TextUnformatted("filesystem_us");
        imgui::TextUnformatted("filter_us");
        imgui::TextUnformatted("regex_ctor_us");

        imgui::TableNextColumn();
        imgui::SeparatorText("");
        imgui::Text("%.1lf", expl.update_cwd_entries_total_us);
        imgui::Text("%.1lf", expl.update_cwd_entries_searchpath_setup_us);
        imgui::Text("%.1lf", expl.update_cwd_entries_filesystem_us);
        imgui::Text("%.1lf", expl.update_cwd_entries_filter_us);
        imgui::Text("%.1lf", expl.update_cwd_entries_regex_ctor_us);

        auto calc_perc_total_time = [&expl](f64 time) {
            return time == 0.f ? 0.f : ( (time / expl.update_cwd_entries_total_us) * 100.f );
        };

        imgui::TableNextColumn();
        imgui::SeparatorText("");
        imgui::Text("%.1lf ms", expl.update_cwd_entries_total_us / 1000.f);
        imgui::Text("%.1lf %%", calc_perc_total_time(expl.update_cwd_entries_searchpath_setup_us));
        imgui::Text("%.1lf %%", calc_perc_total_time(expl.update_cwd_entries_filesystem_us));
        imgui::Text("%.1lf %%", calc_perc_total_time(expl.update_cwd_entries_filter_us));
        imgui::Text("%.1lf %%", calc_perc_total_time(expl.update_cwd_entries_regex_ctor_us));

        imgui::TableNextColumn();
        imgui::SeparatorText("other timers");
        imgui::TextUnformatted("sort_us");
        imgui::TextUnformatted("unpin_us");
        imgui::TextUnformatted("save_to_disk_us");

        imgui::TableNextColumn();
        imgui::SeparatorText("");
        imgui::Text("%.1lf", expl.sort_us);
        imgui::Text("%.1lf", expl.unpin_us);
        imgui::Text("%.1lf", expl.save_to_disk_us);

        imgui::TableNextColumn();
        imgui::SeparatorText("");

        imgui::EndTable();
    }

    imgui::Spacing();
    imgui::Separator();
    imgui_spacing(2);
}

static
void render_back_to_prev_valid_cwd_button(explorer_window &expl) noexcept
{
    auto io = imgui::GetIO();

    imgui_scoped_disabled disabled(expl.wd_history_pos == 0);

    if (imgui::ArrowButton("Back", ImGuiDir_Left)) {
        debug_log("[ %d ] back arrow button triggered", expl.id);

        if (io.KeyShift || io.KeyCtrl) {
            expl.wd_history_pos = 0;
        } else {
            expl.wd_history_pos -= 1;
        }

        expl.cwd = expl.wd_history[expl.wd_history_pos];
        (void) update_cwd_entries(full_refresh, &expl, expl.cwd.data());
        expl.set_latest_valid_cwd_then_notify(expl.cwd);
    }
}

static
void render_forward_to_next_valid_cwd_button(explorer_window &expl) noexcept
{
    u64 wd_history_last_idx = expl.wd_history.empty() ? 0 : expl.wd_history.size() - 1;
    auto io = imgui::GetIO();

    imgui_scoped_disabled disabled(expl.wd_history_pos == wd_history_last_idx);

    if (imgui::ArrowButton("Forward", ImGuiDir_Right)) {
        debug_log("[ %d ] forward arrow button triggered", expl.id);

        if (io.KeyShift || io.KeyCtrl) {
            expl.wd_history_pos = wd_history_last_idx;
        } else {
            expl.wd_history_pos += 1;
        }

        expl.cwd = expl.wd_history[expl.wd_history_pos];
        (void) update_cwd_entries(full_refresh, &expl, expl.cwd.data());
        expl.set_latest_valid_cwd_then_notify(expl.cwd);
    }
}

static
bool render_history_browser_popup(explorer_window &expl, bool cwd_exists_before_edit) noexcept
{
    auto cleanup_and_close_popup = []() {
        imgui::CloseCurrentPopup();
    };

    SCOPE_EXIT { imgui::EndPopup(); };

    imgui::TextUnformatted("History");

    imgui::SameLine();

    imgui::BeginDisabled(expl.wd_history.empty());
    if (imgui::SmallButton("Clear")) {
        expl.wd_history.clear();
        expl.wd_history_pos = 0;

        if (cwd_exists_before_edit) {
            new_history_from(expl, expl.cwd);
        }

        expl.save_to_disk();
        cleanup_and_close_popup();
    }
    imgui::EndDisabled();

    imgui::Spacing();
    imgui::Separator();
    imgui::Spacing();

    if (expl.wd_history.empty()) {
        imgui::TextUnformatted("(empty)");
    }
    else {
        if (imgui::BeginTable("history_table", 3)) {
            u64 i = expl.wd_history.size() - 1;
            u64 i_inverse = 0;

            for (auto iter = expl.wd_history.rbegin(); iter != expl.wd_history.rend(); ++iter, --i, ++i_inverse) {
                imgui::TableNextRow();
                swan_path_t const &hist_path = *iter;

                imgui::TableNextColumn();
                if (i == expl.wd_history_pos) {
                    imgui::TextColored(orange(), "->");
                }

                imgui::TableNextColumn();
                imgui::Text("%3zu ", i_inverse + 1);

                char buffer[2048]; init_empty_cstr(buffer);
                {
                    s32 written = snprintf(buffer, lengthof(buffer), "%s##%zu", hist_path.data(), i);
                    assert(written < lengthof(buffer));
                }

                imgui::TableNextColumn();

                bool pressed;
                {
                    imgui_scoped_text_color tc(dir_color());
                    pressed = imgui::Selectable(buffer, false, ImGuiSelectableFlags_SpanAllColumns);
                }

                if (pressed) {
                    expl.wd_history_pos = i;
                    expl.cwd = expl.wd_history[i];

                    cleanup_and_close_popup();
                    imgui::EndTable();

                    return true;
                }
            }

            imgui::EndTable();
        }
    }

    return false;
}

static
void render_pins_popup(explorer_window &expl) noexcept
{
    auto cleanup_and_close_popup = []() {
        imgui::CloseCurrentPopup();
    };

    imgui::TextUnformatted("Pins");

    imgui::Spacing();
    imgui::Separator();
    imgui::Spacing();

    auto const &pins = get_pins();

    if (pins.empty()) {
        imgui::TextUnformatted("(empty)");
    }
    else {
        for (auto const &pin : pins) {
            {
                imgui_scoped_text_color tc(pin.color);
                static bool selected = false;

                if (imgui::Selectable(pin.label.c_str(), &selected)) {
                    if (!directory_exists(pin.path.data())) {
                        std::string action = std::string("open pin [") + pin.label.c_str() + "]";
                        std::string error = std::string("pin [") + pin.label.c_str() + "] points to invalid directory";
                        swan_open_popup_modal_error(action.c_str(), error.c_str());
                    }
                    else {
                        bool pin_is_valid_dir = update_cwd_entries(full_refresh, &expl, pin.path.data());
                        assert(pin_is_valid_dir);
                        expl.cwd = pin.path;
                        new_history_from(expl, pin.path);
                        expl.set_latest_valid_cwd_then_notify(pin.path);
                        (void) expl.save_to_disk();
                    }
                }
                selected = false;
            }
            if (imgui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
                imgui::SetTooltip("%s", pin.path.data());
            }
        }
    }
}

static
void render_create_directory_popup(explorer_window &expl, wchar_t dir_sep_utf16) noexcept
{
    static char dir_name_utf8[MAX_PATH] = {};
    static std::string err_msg = {};

    auto cleanup_and_close_popup = [&]() {
        dir_name_utf8[0] = L'\0';
        err_msg.clear();
        imgui::CloseCurrentPopup();
    };

    // set initial focus on input text below
    if (imgui::IsWindowAppearing() && !imgui::IsAnyItemActive() && !imgui::IsMouseClicked(0)) {
        imgui::SetKeyboardFocusHere(0);
    }
    if (imgui::InputTextWithHint(
        "##dir_name_input", "Directory name...", dir_name_utf8, lengthof(dir_name_utf8),
        ImGuiInputTextFlags_CallbackCharFilter, filter_chars_callback, (void *)windows_illegal_filename_chars())
    ) {
        err_msg.clear();
    }

    imgui::Spacing();

    if (imgui::Button("Create") && dir_name_utf8[0] != '\0') {
        wchar_t cwd_utf16[MAX_PATH];        init_empty_cstr(cwd_utf16);
        wchar_t dir_name_utf16[MAX_PATH];   init_empty_cstr(dir_name_utf16);
        std::wstring create_path = {};
        s32 utf_written = 0;
        BOOL result = {};

        utf_written = utf8_to_utf16(expl.cwd.data(), cwd_utf16, lengthof(cwd_utf16));
        if (utf_written == 0) {
            debug_log("[ %d ] utf8_to_utf16 failed: expl.cwd -> cwd_utf16", expl.id);
            cleanup_and_close_popup();
            goto end_create_dir;
        }

        utf_written = utf8_to_utf16(dir_name_utf8, dir_name_utf16, lengthof(dir_name_utf16));
        if (utf_written == 0) {
            debug_log("[ %d ] utf8_to_utf16 failed: dir_name_utf8 -> dir_name_utf16", expl.id);
            cleanup_and_close_popup();
            goto end_create_dir;
        }

        create_path.reserve(1024);

        create_path = cwd_utf16;
        if (!create_path.ends_with(dir_sep_utf16)) {
            create_path += dir_sep_utf16;
        }
        create_path += dir_name_utf16;

        WCOUT_IF_DEBUG("CreateDirectoryW [" << create_path << "]\n");
        result = CreateDirectoryW(create_path.c_str(), nullptr);

        if (result == 0) {
            auto error = GetLastError();
            switch (error) {
                case ERROR_ALREADY_EXISTS: err_msg = "Directory already exists."; break;
                case ERROR_PATH_NOT_FOUND: err_msg = "One or more intermediate directories do not exist; probably a bug. Sorry!"; break;
                default: err_msg = get_last_error_string(); break;
            }
            debug_log("[ %d ] CreateDirectoryW failed: %d, %s", expl.id, result, err_msg.c_str());
        } else {
            cleanup_and_close_popup();
            (void) update_cwd_entries(full_refresh, &expl, expl.cwd.data());
        }

        end_create_dir:;
    }

    imgui::SameLine();

    if (imgui::Button("Cancel")) {
        cleanup_and_close_popup();
    }

    if (!err_msg.empty()) {
        imgui::Spacing();
        imgui::TextColored(red(), "Error: %s", err_msg.c_str());
    }

    if (imgui::IsWindowFocused() && imgui::IsKeyPressed(ImGuiKey_Escape)) {
        cleanup_and_close_popup();
    }

    imgui::EndPopup();
}

static
void render_clear_filter_button(explorer_window &expl) noexcept
{
    imgui_scoped_disabled disabled(strempty(expl.filter.data()));

    if (imgui::Button(ICON_FA_BACKSPACE "##clear_filter")) {
        init_empty_cstr(expl.filter.data());
        (void) update_cwd_entries(filter, &expl, expl.cwd.data());
        (void) expl.save_to_disk();
    }
}

static
void render_drives_table(explorer_window &expl, char dir_sep_utf8, u64 size_unit_multiplier) noexcept
{
    static time_point_t last_refresh_time = {};
    static drive_list_t drives = {};

    // refresh drives occasionally
    {
        time_point_t now = current_time();
        s64 diff_ms = compute_diff_ms(last_refresh_time, now);
        if (diff_ms >= 1000) {
            drives = query_drive_list();
            last_refresh_time = current_time();
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

    if (imgui::BeginTable("drives", drive_table_col_id_count, ImGuiTableFlags_SizingStretchSame|ImGuiTableFlags_BordersInnerV)) {
        imgui::TableSetupColumn("Drive", ImGuiTableColumnFlags_NoSort, 0.0f, drive_table_col_id_letter);
        imgui::TableSetupColumn("Name", ImGuiTableColumnFlags_NoSort, 0.0f, drive_table_col_id_name);
        imgui::TableSetupColumn("Filesystem", ImGuiTableColumnFlags_NoSort, 0.0f, drive_table_col_id_filesystem);
        imgui::TableSetupColumn("Total Space", ImGuiTableColumnFlags_NoSort, 0.0f, drive_table_col_id_total_space);
        imgui::TableSetupColumn("Usage", ImGuiTableColumnFlags_NoSort, 0.0f, drive_table_col_id_used_percent);
        imgui::TableSetupColumn("Free Space", ImGuiTableColumnFlags_NoSort, 0.0f, drive_table_col_id_free_space);
        imgui::TableHeadersRow();

        for (auto &drive : drives) {
            imgui::TableNextRow();

            if (imgui::TableSetColumnIndex(drive_table_col_id_letter)) {
                imgui::TextColored(get_color(basic_dirent::kind::directory), "%C:", drive.letter);
            }

            if (imgui::TableSetColumnIndex(drive_table_col_id_name)) {
                static bool selected = false;

                if (imgui::Selectable(drive.name_utf8[0] == '\0' ? "Unnamed Disk" : drive.name_utf8,
                                        &selected, ImGuiSelectableFlags_SpanAllColumns))
                {
                    char root[] = { drive.letter, ':', dir_sep_utf8, '\0' };
                    expl.cwd = path_create(root);
                    (void) update_cwd_entries(full_refresh, &expl, expl.cwd.data());
                    (void) expl.save_to_disk();
                    expl.set_latest_valid_cwd_then_notify(expl.cwd);
                    new_history_from(expl, expl.cwd);
                }

                selected = false;
            }

            if (imgui::TableSetColumnIndex(drive_table_col_id_filesystem)) {
                imgui::TextUnformatted(drive.filesystem_name_utf8);
            }

            if (imgui::TableSetColumnIndex(drive_table_col_id_total_space)) {
                char buffer[32]; init_empty_cstr(buffer);
                format_file_size(drive.total_bytes, buffer, lengthof(buffer), size_unit_multiplier);
                imgui::TextUnformatted(buffer);
            }

            if (imgui::TableSetColumnIndex(drive_table_col_id_used_percent)) {
                u64 used_bytes = drive.total_bytes - drive.available_bytes;
                f64 percent_used = ( f64(used_bytes) / f64(drive.total_bytes) ) * 100.0;
                imgui::Text("%3.0lf%%", percent_used);
                imgui::SameLine();
                imgui::ProgressBar(f32(used_bytes) / f32(drive.total_bytes), ImVec2(-1, imgui::CalcTextSize("1").y), "");
            }

            if (imgui::TableSetColumnIndex(drive_table_col_id_free_space)) {
                char buffer[32]; init_empty_cstr(buffer);
                format_file_size(drive.available_bytes, buffer, lengthof(buffer), size_unit_multiplier);
                imgui::TextUnformatted(buffer);
            }
        }

        imgui::EndTable();
    }
}

static
void render_filter_text_input(explorer_window &expl) noexcept
{
    auto width = max(
        imgui::CalcTextSize(expl.filter.data()).x + (imgui::GetStyle().FramePadding.x * 2) + 10.f,
        imgui::CalcTextSize("123456789012345").x
    );

    imgui_scoped_item_width iw(width);

    if (imgui::InputTextWithHint("##filter", "Filter", expl.filter.data(), expl.filter.size())) {
        (void) update_cwd_entries(filter, &expl, expl.cwd.data());
        (void) expl.save_to_disk();
    }
}

static
void render_filter_mode_toggle(explorer_window &expl) noexcept
{
    static char const *filter_modes[] = {
        "Contains",
        "RegExp  ",
    };

    static_assert(lengthof(filter_modes) == (u64)explorer_window::filter_mode::count);

    static char const *current_mode = filter_modes[expl.filter_mode];

    char buffer[64]; init_empty_cstr(buffer);
    snprintf(buffer, lengthof(buffer), "%s##%zu", current_mode, expl.filter_mode);
    if (imgui::Button(buffer)) {
        if ((u64)expl.filter_mode == (u64)explorer_window::filter_mode::count - (u64)1) {
            (u64 &)expl.filter_mode = 0; // wrap around
        } else {
            (u64 &)expl.filter_mode = (u64)expl.filter_mode + 1;
        }
        current_mode = filter_modes[expl.filter_mode];
        (void) update_cwd_entries(filter, &expl, expl.cwd.data());
        (void) expl.save_to_disk();
    }
}

static
void render_filter_case_sensitivity_button(explorer_window &expl) noexcept
{
    if (imgui::Button(expl.filter_case_sensitive ? "s" : "i")) {
        flip_bool(expl.filter_case_sensitive);
        (void) update_cwd_entries(filter, &expl, expl.cwd.data());
        (void) expl.save_to_disk();
    }
    if (imgui::IsItemHovered()) {
        imgui::SetTooltip(
            " Filter case sensitivity: \n"
            " %s insensitive \n"
            " %s sensitive ",
            !expl.filter_case_sensitive ? ">>" : "  ",
                expl.filter_case_sensitive ? ">>" : "  "
        );
    }
}

static
void render_filter_polarity_button(explorer_window &expl) noexcept
{
    if (imgui::Button(expl.filter_polarity ? "+##filter_polarity" : "-##filter_polarity")) {
        flip_bool(expl.filter_polarity);
        (void) update_cwd_entries(filter, &expl, expl.cwd.data());
        (void) expl.save_to_disk();
    }
    if (imgui::IsItemHovered()) {
        imgui::SetTooltip(
            " Filter polarity: \n"
            " %s positive \n"
            " %s negative ",
                expl.filter_polarity ? ">>" : "  ",
            !expl.filter_polarity ? ">>" : "  "
        );
    }
}

static
void render_create_file_popup(explorer_window &expl, wchar_t dir_sep_utf16) noexcept
{
    static char file_name_utf8[MAX_PATH] = {};
    static std::string err_msg = {};

    auto cleanup_and_close_popup = [&]() {
        file_name_utf8[0] = L'\0';
        err_msg.clear();
        imgui::CloseCurrentPopup();
    };

    // set initial focus on input text below
    if (imgui::IsWindowAppearing() && !imgui::IsAnyItemActive() && !imgui::IsMouseClicked(0)) {
        imgui::SetKeyboardFocusHere(0);
    }
    if (imgui::InputTextWithHint(
        "##file_name_input", "File name...", file_name_utf8, lengthof(file_name_utf8),
        ImGuiInputTextFlags_CallbackCharFilter, filter_chars_callback, (void *)windows_illegal_filename_chars())
    ) {
        err_msg.clear();
    }

    imgui::Spacing();

    if (imgui::Button("Create") && file_name_utf8[0] != '\0') {
        wchar_t cwd_utf16[MAX_PATH];        init_empty_cstr(cwd_utf16);
        wchar_t file_name_utf16[MAX_PATH];  init_empty_cstr(file_name_utf16);
        std::wstring create_path = {};
        s32 utf_written = 0;
        HANDLE result = {};

        utf_written = utf8_to_utf16(expl.cwd.data(), cwd_utf16, lengthof(cwd_utf16));
        if (utf_written == 0) {
            debug_log("[ %d ] utf8_to_utf16 failed: expl.cwd -> cwd_utf16", expl.id);
            cleanup_and_close_popup();
            goto end_create_file;
        }

        utf_written = utf8_to_utf16(file_name_utf8, file_name_utf16, lengthof(file_name_utf16));
        if (utf_written == 0) {
            debug_log("[ %d ] utf8_to_utf16 failed: file_name_utf8 -> file_name_utf16", expl.id);
            cleanup_and_close_popup();
            goto end_create_file;
        }

        create_path.reserve(1024);

        create_path = cwd_utf16;
        if (!create_path.ends_with(dir_sep_utf16)) {
            create_path += dir_sep_utf16;
        }
        create_path += file_name_utf16;

        WCOUT_IF_DEBUG("CreateFileW [" << create_path << "]\n");
        result = CreateFileW(
            create_path.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr,
            CREATE_NEW,
            FILE_ATTRIBUTE_NORMAL,
            nullptr
        );

        if (result == INVALID_HANDLE_VALUE) {
            auto error = GetLastError();
            switch (error) {
                case ERROR_ALREADY_EXISTS: err_msg = "File already exists."; break;
                case ERROR_PATH_NOT_FOUND: err_msg = "One or more intermediate directories do not exist; probably a bug. Sorry!"; break;
                default: err_msg = get_last_error_string(); break;
            }
            debug_log("[ %d ] CreateFileW failed: %d, %s", expl.id, result, err_msg.c_str());
        } else {
            cleanup_and_close_popup();
            (void) update_cwd_entries(full_refresh, &expl, expl.cwd.data());
        }

        end_create_file:;
    }

    imgui::SameLine();

    if (imgui::Button("Cancel")) {
        cleanup_and_close_popup();
    }

    if (!err_msg.empty()) {
        imgui::Spacing();
        imgui::TextColored(red(), "Error: %s", err_msg.c_str());
    }

    if (imgui::IsWindowFocused() && imgui::IsKeyPressed(ImGuiKey_Escape)) {
        cleanup_and_close_popup();
    }

    imgui::EndPopup();
}

void render_button_pin_cwd(explorer_window &expl, bool cwd_exists_before_edit) noexcept
{
    u64 pin_idx;
    {
        scoped_timer<timer_unit::MICROSECONDS> check_if_pinned_timer(&expl.check_if_pinned_us);
        pin_idx = find_pin_idx(expl.cwd);
    }
    bool already_pinned = pin_idx != std::string::npos;

    char buffer[4] = {};
    {
#if 0
        s32 written = snprintf(buffer, lengthof(buffer), "[%c]", (already_pinned ? '*' : ' '));
#else
        s32 written = snprintf(buffer, lengthof(buffer), "%s", already_pinned ? ICON_FA_STAR : ICON_FA_STAR_HALF);
#endif
        assert(written < lengthof(buffer));
    }

    imgui_scoped_disabled disabled(!cwd_exists_before_edit && !already_pinned);

    if (imgui::Button(buffer)) {
        if (already_pinned) {
            debug_log("[ %d ] pin_idx = %zu", expl.id, pin_idx);
            scoped_timer<timer_unit::MICROSECONDS> unpin_timer(&expl.unpin_us);
            unpin(pin_idx);
        }
        else {
            swan_open_popup_modal_new_pin(expl.cwd, false);
        }
        bool result = save_pins_to_disk();
        debug_log("save_pins_to_disk: %d", result);
    }
    if (imgui::IsItemHovered()) {
        imgui::SetTooltip(" Click here to %s the current working directory. ", already_pinned ? "unpin" : "pin");
    }
}

void render_up_to_cwd_parent_button(explorer_window &expl, bool cwd_exists_before_edit) noexcept
{
    imgui_scoped_disabled disabled(!cwd_exists_before_edit);

    // if (imgui::Button("..##up")) {
    if (imgui::ArrowButton("..##up", ImGuiDir_Up)) {
        debug_log("[ %d ] (..) button triggered", expl.id);

        auto result = try_ascend_directory(expl);

        if (!result.success) {
            auto cwd_len = path_length(expl.cwd);

            if ( (cwd_len == 2 && IsCharAlphaA(expl.cwd[0]) && expl.cwd[1] == ':') ||
                    (cwd_len == 3 && IsCharAlphaA(expl.cwd[0]) && expl.cwd[1] == ':' && strchr("\\/", expl.cwd[2])) )
            {
                path_clear(expl.cwd);
            }
            else {
                char action[2048]; init_empty_cstr(action);
                s32 written = snprintf(action, lengthof(action), "ascend to directory [%s]", result.parent_dir.data());
                assert(written < lengthof(action));

                swan_open_popup_modal_error(action, "could not find directory");
            }
        }
    }
}

enum cwd_mode : s32
{
    cwd_mode_text_input = 0,
    cwd_mode_clicknav,
    cwd_mode_count
};

void render_button_cwd_mode_toggle(explorer_window &expl, cwd_mode &mode) noexcept
{
    static char const *cwd_modes[] = {
        ICON_FA_I_CURSOR,
        ICON_FA_HAND_POINTER,
    };

    static_assert(lengthof(cwd_modes) == cwd_mode_count);

    static char const *current_mode = cwd_modes[mode];

    char buffer[64]; init_empty_cstr(buffer);
    snprintf(buffer, lengthof(buffer), "%s##%zu", current_mode, expl.filter_mode);

    if (imgui::Button(buffer)) {
        if (mode == cwd_mode_count - 1) {
            mode = cwd_mode(0);
        } else {
            mode = cwd_mode(mode + 1);
        }
        current_mode = cwd_modes[mode];
    }
}

void render_cwd_clicknav(explorer_window &expl, bool cwd_exists_after_edit, char dir_sep_utf8) noexcept
{
    if (cwd_exists_after_edit && !path_is_empty(expl.cwd)) {
        imgui::TableNextColumn();

        static std::vector<char const *> slices = {};
        slices.reserve(50);
        slices.clear();

        swan_path_t sliced_path = expl.cwd;
        char const *slice = strtok(sliced_path.data(), "\\/");
        while (slice != nullptr) {
            slices.push_back(slice);
            slice = strtok(nullptr, "\\/");
        }

        auto cd_to_slice = [&expl, &sliced_path](char const *slice) {
            char const *slice_end = slice;
            while (*slice_end != '\0') {
                ++slice_end;
            }

            u64 len = slice_end - sliced_path.data();

            if (len == path_length(expl.cwd)) {
                debug_log("[ %d ] cd_to_slice: slice == cwd, not updating cwd|history", expl.id);
            }
            else {
                expl.cwd[len] = '\0';
                new_history_from(expl, expl.cwd);
            }

            (void) update_cwd_entries(full_refresh, &expl, expl.cwd.data());
            (void) expl.save_to_disk();
            expl.set_latest_valid_cwd_then_notify(expl.cwd);
        };

        f32 original_spacing = imgui::GetStyle().ItemSpacing.x;

        {
            u64 i = 0;
            for (auto slice_it = slices.begin(); slice_it != slices.end() - 1; ++slice_it, ++i) {
                char buffer[1024]; init_empty_cstr(buffer);
                snprintf(buffer, lengthof(buffer), "%s##slice%zu", *slice_it, i);

                if (imgui::Button(buffer)) {
                    debug_log("[ %d ] clicked slice [%s]", expl.id, *slice_it);
                    cd_to_slice(*slice_it);
                }

                imgui::GetStyle().ItemSpacing.x = 2;
                imgui::SameLine();
                imgui::Text("%c", dir_sep_utf8);
                imgui::SameLine();
            }
        }

        if (imgui::Button(slices.back())) {
            debug_log("[ %d ] clicked slice [%s]", expl.id, slices.back());
            // cd_to_slice(slices.back());
        }

        if (slices.size() > 1) {
            imgui::GetStyle().ItemSpacing.x = original_spacing;
        }
    }
}

void render_cwd_text_input(explorer_window &expl, bool &cwd_exists_after_edit, char dir_sep_utf8, wchar_t dir_sep_utf16) noexcept
{
    cwd_text_input_callback_user_data user_data = { expl.id, dir_sep_utf16, false };

    imgui::PushItemWidth(
        max(imgui::CalcTextSize(expl.cwd.data()).x + (imgui::GetStyle().FramePadding.x * 2),
            imgui::CalcTextSize("123456789_123456789_").x)
        + 60.f
    );

    static swan_path_t cwd_input = {};
    cwd_input = expl.cwd;

    imgui::InputText("##cwd", cwd_input.data(), cwd_input.size(),
        ImGuiInputTextFlags_CallbackCharFilter|ImGuiInputTextFlags_CallbackEdit,
        cwd_text_input_callback, (void *)&user_data);

    if (user_data.edit_occurred) {
        if (!path_loosely_same(expl.cwd, cwd_input)) {
            expl.cwd = path_squish_adjacent_separators(cwd_input);
            path_force_separator(expl.cwd, dir_sep_utf8);

            cwd_exists_after_edit = update_cwd_entries(full_refresh, &expl, expl.cwd.data());

            if (cwd_exists_after_edit && !path_is_empty(expl.cwd)) {
                if (path_is_empty(expl.latest_valid_cwd) || !path_loosely_same(expl.cwd, expl.latest_valid_cwd)) {
                    new_history_from(expl, expl.cwd);
                }
                if (!path_loosely_same(expl.cwd, expl.latest_valid_cwd)) {
                    std::scoped_lock lock(expl.latest_valid_cwd_mutex);
                    expl.latest_valid_cwd = expl.cwd;
                    while (path_pop_back_if(expl.latest_valid_cwd, dir_sep_utf8));
                }
                expl.set_latest_valid_cwd_then_notify(expl.cwd);
            }
        }
        (void) expl.save_to_disk();
    }

    imgui::PopItemWidth();

    // label
    if (get_explorer_options().show_cwd_len) {
        imgui::SameLine();
        imgui::Text("cwd(%3d)", path_length(expl.cwd));
    }
}

#if !defined(NDEBUG)
struct natvis_sanity_check
{
    u64 len;
    char str[128];
};
#endif

void swan_render_window_explorer(explorer_window &expl, bool &open) noexcept
{
#if !defined(NDEBUG)
    natvis_sanity_check nsc = { 13, "Sanity Check." };
#endif

    {
        char buffer[128];
        snprintf(buffer, lengthof(buffer), " %s %s ", ICON_FA_FOLDER_OPEN, expl.name);

        bool is_window_visible = imgui::Begin(buffer, &open);
        expl.is_window_visible.store(is_window_visible);

        if (!is_window_visible) {
            imgui::End();
            return;
        }

        expl.is_window_visible.notify_one();
    }

    auto &io = imgui::GetIO();
    bool any_window_focused = imgui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    explorer_options const &opts = get_explorer_options();
    bool cwd_exists_before_edit = directory_exists(expl.cwd.data());
    bool cwd_exists_after_edit = cwd_exists_before_edit;
    char dir_sep_utf8 = opts.dir_separator_utf8();
    wchar_t dir_sep_utf16 = opts.dir_separator_utf16();
    u64 size_unit_multiplier = opts.size_unit_multiplier();

    bool open_single_rename_popup = false;
    bool open_bulk_rename_popup = false;

    bool any_popups_open = swan_is_popup_modal_open_bulk_rename() || swan_is_popup_modal_open_error() || swan_is_popup_modal_open_single_rename();

    static std::string error_popup_action = {};
    static std::string error_popup_failure = {};

    // if (window_focused) {
    //     save_focused_window(expl.id);
    // }

    static explorer_window::dirent const *dirent_to_be_renamed = nullptr;

    // handle [F2] pressed on cwd entry
    if (any_window_focused && !any_popups_open && imgui::IsKeyPressed(ImGuiKey_F2)) {
        if (explorer_window::NO_SELECTION == expl.cwd_prev_selected_dirent_idx) {
            swan_open_popup_modal_error("[F2] was pressed (function: rename)", "nothing is selected");
        } else {
            auto &dirent_which_f2_was_pressed_on = expl.cwd_entries[expl.cwd_prev_selected_dirent_idx];
            debug_log("[ %d ] pressed F2 on [%s]", expl.id, dirent_which_f2_was_pressed_on.basic.path.data());
            open_single_rename_popup = true;
            dirent_to_be_renamed = &dirent_which_f2_was_pressed_on;
        }
    }

    // refresh logic start
    {
        auto refresh = [&](std::source_location sloc = std::source_location::current()) {
            cwd_exists_before_edit = update_cwd_entries(full_refresh, &expl, expl.cwd.data(), sloc);
            cwd_exists_after_edit = cwd_exists_before_edit;
        };

        if (any_window_focused && io.KeyCtrl && imgui::IsKeyPressed(ImGuiKey_R)) {
            debug_log("[ %d ] Ctrl-R, refresh triggered", expl.id);
            refresh();
        }
        else if (cwd_exists_before_edit) {
            time_point_t now = current_time();
            s64 diff_ms = compute_diff_ms(expl.last_refresh_time, now);
            s32 min_refresh_itv_ms = explorer_options::min_tolerable_refresh_interval_ms;

            if (diff_ms >= max(min_refresh_itv_ms, opts.auto_refresh_interval_ms.load())) {
                auto refresh_notif_time = expl.refresh_notif_time.load(std::memory_order::seq_cst);
                if (expl.last_refresh_time.time_since_epoch().count() < refresh_notif_time.time_since_epoch().count()) {
                    debug_log("[ %d ] refresh notif RECV", expl.id);
                    refresh();
                }
            }
        }
    }
    // refresh logic end

    imgui_spacing(1);

    if (opts.show_debug_info) {
        render_debug_info(expl, size_unit_multiplier);
    }

#if 0
    imgui::TextUnformatted("Bulk ops: {");
    imgui::SameLine();

    imgui::BeginDisabled(expl.num_selected_cwd_entries == 0);
    {
        char buffer[32]; init_empty_cstr(buffer);
        s32 written = snprintf(buffer, lengthof(buffer), "Rename##bulk%c", expl.id[strlen(expl.id)-1]);
        assert(written < lengthof(buffer));
        if (imgui::Button(buffer)) {
            open_bulk_rename_popup = true;
        }
    }
    imgui::EndDisabled();

    imgui::SameLine();

    imgui::BeginDisabled(expl.num_selected_cwd_entries == 0);
    if (imgui::Button("Cut")) {
        s_paste_payload.window_name = expl.id;
        s_paste_payload.items.clear();
        s_paste_payload.keep_src = false;

        for (auto const &dirent : expl.cwd_entries) {
            if (dirent.is_selected && !dirent.basic.is_dotdot()) {
                swan_path_t src = expl.cwd;
                if (path_append(src, dirent.basic.path.data(), dir_sep_utf8, true)) {
                    s_paste_payload.items.push_back({ dirent.basic.size, dirent.basic.type, src });
                    s_paste_payload.bytes += dirent.basic.size;
                    if (dirent.basic.is_directory()) {
                        s_paste_payload.has_directories = true;
                        // TODO: include size of children in s_paste_payload.bytes
                    }
                } else {
                    char action[2048]; init_empty_cstr(action);
                    s32 written = snprintf(action, lengthof(action), "add %s [%s] to Cut clipboard", dirent.basic.kind_cstr(), dirent.basic.path.data());
                    assert(written < lengthof(action));

                    swan_open_popup_modal_error(action, "max path length exceeded when trying to append name to cwd");
                    s_paste_payload.items.clear();

                    break;
                }
            }
        }
    }
    imgui::EndDisabled();

    imgui::SameLine();

    imgui::BeginDisabled(expl.num_selected_cwd_entries == 0);
    if (imgui::Button("Copy")) {
        s_paste_payload.window_name = expl.id;
        s_paste_payload.items.clear();
        s_paste_payload.keep_src = true;

        for (auto const &dirent : expl.cwd_entries) {
            if (dirent.is_selected && !dirent.basic.is_dotdot()) {
                swan_path_t src = expl.cwd;
                if (path_append(src, dirent.basic.path.data(), dir_sep_utf8, true)) {
                    s_paste_payload.items.push_back({ dirent.basic.size, dirent.basic.type, src });
                    s_paste_payload.bytes += dirent.basic.size;
                    if (dirent.basic.is_directory()) {
                        s_paste_payload.has_directories = true;
                        // TODO: include size of children in s_paste_payload.bytes
                    }
                } else {
                    char action[2048]; init_empty_cstr(action);
                    s32 written = snprintf(action, lengthof(action), "add %s [%s] to Copy clipboard", dirent.basic.kind_cstr(), dirent.basic.path.data());
                    assert(written < lengthof(action));

                    swan_open_popup_modal_error(action, "max path length exceeded when trying to append name to cwd");
                    s_paste_payload.items.clear();

                    break;
                }
            }
        }
    }
    imgui::EndDisabled();

    imgui::SameLine();

    imgui::BeginDisabled(expl.num_selected_cwd_entries == 0);
    if (imgui::Button("Delete")) {
        delete_selected_entries(expl, expl.cwd_entries_selected, dir_sep_utf16);
    }
    imgui::EndDisabled();

    imgui::SameLine();
    imgui::TextUnformatted("}");

    imgui_spacing(3);
#endif

    render_back_to_prev_valid_cwd_button(expl);

    imgui::SameLine();

    render_forward_to_next_valid_cwd_button(expl);

    imgui::SameLine();

    if (imgui::Button(ICON_FA_HISTORY "##expl.wd_history")) {
        imgui::OpenPopup("history_popup");
    }
    if (imgui::BeginPopup("history_popup")) {
        bool history_item_clicked = render_history_browser_popup(expl, cwd_exists_before_edit);

        if (history_item_clicked) {
            (void) update_cwd_entries(full_refresh, &expl, expl.cwd.data());
            (void) expl.save_to_disk();
            expl.set_latest_valid_cwd_then_notify(expl.cwd);
        }
    }

    imgui::SameLine();

    render_up_to_cwd_parent_button(expl, cwd_exists_before_edit);

    imgui_sameline_spacing(3);

    render_button_pin_cwd(expl, cwd_exists_before_edit);

    imgui::SameLine();

    {
        static cwd_mode mode = cwd_mode_text_input;
        render_button_cwd_mode_toggle(expl, mode);

        imgui_sameline_spacing(1);

        switch (mode) {
            case cwd_mode_clicknav:
                render_cwd_clicknav(expl, cwd_exists_after_edit, dir_sep_utf8);
                break;
            case cwd_mode_text_input:
                render_cwd_text_input(expl, cwd_exists_after_edit, dir_sep_utf8, dir_sep_utf16);
                break;
        }
    }

    imgui_spacing(3);

    // imgui_sameline_spacing(3);

    if (imgui::Button("Pins")) {
    // if (imgui::Button(ICON_FA_BOOKMARK "##pins")) {
        imgui::OpenPopup("pins_popup");
    }
    if (imgui::BeginPopup("pins_popup")) {
        render_pins_popup(expl);
        imgui::EndPopup();
    }

    imgui_sameline_spacing(2);

    if (imgui::Button(ICON_FA_FOLDER_PLUS "##+dir")) {
        imgui::OpenPopup("Create directory");
    }
    if (imgui::BeginPopupModal("Create directory", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        render_create_directory_popup(expl, dir_sep_utf16);
    }

    imgui_sameline_spacing(0);

    if (imgui::Button(ICON_FA_FILE_MEDICAL "##+file")) {
        imgui::OpenPopup("Create file");
    }
    if (imgui::BeginPopupModal("Create file", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        render_create_file_popup(expl, dir_sep_utf16);
    }

    imgui_sameline_spacing(3);

    render_filter_polarity_button(expl);

    imgui::SameLine();

    render_filter_case_sensitivity_button(expl);

    imgui::SameLine();

    render_filter_mode_toggle(expl);

    imgui::SameLine();

    render_filter_text_input(expl);

    imgui::SameLine();

    render_clear_filter_button(expl);

    // paste payload description start
#if 0
    if (!s_paste_payload.items.empty()) {
        imgui_spacing(3);

        if (imgui::Button("Paste")) {
            bool keep_src = s_paste_payload.keep_src;

            // TODO: setup IFileOperation

            for (auto const &paste_item : s_paste_payload.items) {
                if (paste_item.type == basic_dirent::kind::directory) {
                    if (keep_src) {
                        // copy directory
                    }
                    else {
                        // move directory
                    }
                }
                else {
                    if (keep_src) {
                        // copy file
                    }
                    else {
                        // move file
                    }
                }
            }

            // TODO: cleanup IFileOperation
        }

        imgui::SameLine();

        if (imgui::Button("X##Cancel")) {
            s_paste_payload.items.clear();
        }

        imgui_sameline_spacing(1);

        u64 num_dirs = 0, num_symlinks = 0, num_files = 0;
        for (auto const &item : s_paste_payload.items) {
            num_dirs     += u64(item.type == basic_dirent::kind::directory);
            num_symlinks += u64(item.type == basic_dirent::kind::symlink);
            num_files    += u64(item.type == basic_dirent::kind::file);
        }

        if (num_dirs > 0) {
            imgui::SameLine();
            imgui::TextColored(get_color(basic_dirent::kind::directory), "%zud", num_dirs);
        }
        if (num_symlinks > 0) {
            imgui::SameLine();
            imgui::TextColored(get_color(basic_dirent::kind::symlink), "%zus", num_symlinks);
        }
        if (num_files > 0) {
            imgui::SameLine();
            imgui::TextColored(get_color(basic_dirent::kind::file), "%zuf", num_files);
        }

        imgui::SameLine();
        imgui::Text("ready to be %s from %s", (s_paste_payload.keep_src ? "copied" : "cut"), s_paste_payload.window_name);

        if (imgui::IsItemHovered() && imgui::BeginTooltip()) {
            imgui::TextUnformatted("from");
            imgui::SameLine();
            {
                std::string_view parent_path_view = get_everything_minus_file_name(s_paste_payload.items.front().path.data());
                char parent_path_utf8[2048]; init_empty_cstr(parent_path_utf8);
                strncat(parent_path_utf8, parent_path_view.data(), parent_path_view.size());
                imgui::TextColored(dir_color(), parent_path_utf8);
            }

            imgui::Spacing();
            imgui::Separator();
            imgui::Spacing();

            {
                char pretty_size[32]; init_empty_cstr(pretty_size);
                format_file_size(s_paste_payload.bytes, pretty_size, lengthof(pretty_size), size_unit_multiplier);
                if (s_paste_payload.has_directories) {
                    imgui::Text(">= %s", pretty_size);
                } else {
                    imgui::TextUnformatted(pretty_size);
                }
            }

            imgui::Spacing();
            imgui::Separator();
            imgui::Spacing();

            for (auto const &item : s_paste_payload.items) {
                ImVec4 color;
                switch (item.type) {
                    case basic_dirent::kind::directory: color = dir_color();     break;
                    case basic_dirent::kind::symlink:   color = symlink_color(); break;
                    case basic_dirent::kind::file:      color = file_color();    break;
                    default:
                        ImVec4 white(1.0, 1.0, 1.0, 1.0);
                        color = white;
                        break;
                }
                imgui::TextColored(color, cget_file_name(item.path.data()));
            }

            imgui::EndTooltip();
        }
    }
#endif
    // paste payload description end

    imgui_spacing(2);

    if (expl.filter_error != "") {
        imgui::PushTextWrapPos(imgui::GetColumnWidth());
        imgui::TextColored(red(), "%s", expl.filter_error.c_str());
        imgui::PopTextWrapPos();

        imgui_spacing(2);
    }

    imgui::Separator();
    imgui_spacing(2);

    if (path_is_empty(expl.cwd)) {
        render_drives_table(expl, dir_sep_utf8, size_unit_multiplier);
    }
    else if (!cwd_exists_after_edit) {
        // imgui::Separator();
        // imgui_spacing(2);
        imgui::TextColored(orange(), "Invalid directory.");
    }
    else if (expl.cwd_entries.empty()) {
        // cwd exists but is empty
        // imgui::Separator();
        // imgui_spacing(2);
        imgui::TextColored(orange(), "Empty directory.");
    }
    else {
        u64 num_selected_directories = 0;
        u64 num_selected_symlinks = 0;
        u64 num_selected_files = 0;

        u64 num_filtered_directories = 0;
        u64 num_filtered_symlinks = 0;
        u64 num_filtered_files = 0;

        u64 num_child_dirents = 0;
        u64 num_child_directories = 0;
        u64 num_child_symlinks = 0;
        u64 num_child_files = 0;

        for (auto &dirent : expl.cwd_entries) {
            static_assert(u64(false) == 0);
            static_assert(u64(true)  == 1);

            [[maybe_unused]] char const *path = dirent.basic.path.data();

            bool is_dotdot = dirent.basic.is_dotdot();

            num_filtered_directories += u64(dirent.is_filtered_out && dirent.basic.is_directory() && !is_dotdot);
            num_filtered_symlinks    += u64(dirent.is_filtered_out && dirent.basic.is_symlink());
            num_filtered_files       += u64(dirent.is_filtered_out && dirent.basic.is_non_symlink_file());

            num_child_dirents     += u64(!is_dotdot);
            num_child_directories += u64(dirent.basic.is_directory() && !is_dotdot);
            num_child_symlinks    += u64(dirent.basic.is_symlink());
            num_child_files       += u64(dirent.basic.is_non_symlink_file());

            if (!dirent.is_filtered_out && dirent.is_selected) {
                num_selected_directories += u64(dirent.is_selected && dirent.basic.is_directory() && !is_dotdot);
                num_selected_symlinks    += u64(dirent.is_selected && dirent.basic.is_symlink());
                num_selected_files       += u64(dirent.is_selected && dirent.basic.is_non_symlink_file());
            }
        }

        u64 num_filtered_dirents = num_filtered_directories + num_filtered_symlinks + num_filtered_files;
        u64 num_selected_dirents = num_selected_directories + num_selected_symlinks + num_selected_files;

        // imgui::Separator();
        // imgui_spacing(2);

        imgui::Text("items(%zu)", num_child_dirents);
        if (imgui::IsItemHovered() && imgui::BeginTooltip()) {
            imgui::SeparatorText("Items");

            if (num_child_directories > 0) {
                imgui::TextColored(dir_color(), "%zu director%s", num_child_directories, num_child_directories == 1 ? "y" : "ies");
            }
            if (num_child_symlinks > 0) {
                imgui::TextColored(symlink_color(), "%zu symlink%s", num_child_symlinks, num_child_symlinks == 1 ? "" : "s");
            }
            if (num_child_files > 0) {
                imgui::TextColored(file_color(), "%zu file%s", num_child_files, num_child_files == 1 ? "" : "s");
            }

            imgui::EndTooltip();
        }

        if (expl.filter_error == "" && num_filtered_dirents > 0) {
            imgui_sameline_spacing(1);

            imgui::Text("filtered(%zu)", num_filtered_dirents);

            if (imgui::IsItemHovered() && imgui::BeginTooltip()) {
                imgui::SeparatorText("Filtered");

                if (num_filtered_directories > 0) {
                    imgui::TextColored(dir_color(), "%zu director%s", num_filtered_directories, num_filtered_directories == 1 ? "y" : "ies");
                }
                if (num_filtered_symlinks > 0) {
                    imgui::TextColored(symlink_color(), "%zu symlink%s", num_filtered_symlinks, num_filtered_symlinks == 1 ? "" : "s");
                }
                if (num_filtered_files > 0) {
                    imgui::TextColored(file_color(), "%zu file%s", num_filtered_files, num_filtered_files == 1 ? "" : "s");
                }

                imgui::EndTooltip();
            }
        }

        if (num_selected_dirents > 0) {
            imgui_sameline_spacing(1);

            imgui::Text("selected(%zu)", num_selected_dirents);

            if (imgui::IsItemHovered() && imgui::BeginTooltip()) {
                imgui::SeparatorText("Selected");

                if (num_selected_directories > 0) {
                    imgui::TextColored(dir_color(), "%zu director%s", num_selected_directories, num_selected_directories == 1 ? "y" : "ies");
                }
                if (num_selected_symlinks > 0) {
                    imgui::TextColored(symlink_color(), "%zu symlink%s", num_selected_symlinks, num_selected_symlinks == 1 ? "" : "s");
                }
                if (num_selected_files > 0) {
                    imgui::TextColored(file_color(), "%zu file%s", num_selected_files, num_selected_files == 1 ? "" : "s");
                }

                imgui::EndTooltip();
            }
        }

        imgui_spacing(2);

        if (imgui::BeginChild("cwd_entries_child", ImVec2(0, imgui::GetContentRegionAvail().y))) {
            if (num_filtered_dirents == expl.cwd_entries.size()) {
                imgui::Spacing();

                if (imgui::Button("Clear filter")) {
                    debug_log("[ %d ] clear filter button pressed", expl.id);
                    init_empty_cstr(expl.filter.data());
                    (void) update_cwd_entries(filter, &expl, expl.cwd.data());
                    (void) expl.save_to_disk();
                }

                imgui_sameline_spacing(1);

                imgui::TextColored(orange(), "All items filtered.");
            }
            else if (imgui::BeginTable("cwd_entries", cwd_entries_table_col_count,
                ImGuiTableFlags_SizingStretchProp|ImGuiTableFlags_Hideable|ImGuiTableFlags_Resizable|ImGuiTableFlags_Reorderable|ImGuiTableFlags_Sortable|ImGuiTableFlags_RowBg
            )) {
                imgui::TableSetupColumn("#", ImGuiTableColumnFlags_NoSort, 0.0f, cwd_entries_table_col_number);
                imgui::TableSetupColumn("ID", ImGuiTableColumnFlags_DefaultSort, 0.0f, cwd_entries_table_col_id);
                imgui::TableSetupColumn("Path", ImGuiTableColumnFlags_DefaultSort, 0.0f, cwd_entries_table_col_path);
                imgui::TableSetupColumn("Type", ImGuiTableColumnFlags_DefaultSort, 0.0f, cwd_entries_table_col_type);
                imgui::TableSetupColumn("Size", ImGuiTableColumnFlags_DefaultSort, 0.0f, cwd_entries_table_col_size_pretty);
                imgui::TableSetupColumn("Bytes", ImGuiTableColumnFlags_DefaultSort, 0.0f, cwd_entries_table_col_size_bytes);
                // imgui::TableSetupColumn("Created", ImGuiTableColumnFlags_DefaultSort, 0.0f, cwd_entries_table_col_creation_time);
                imgui::TableSetupColumn("Modified", ImGuiTableColumnFlags_DefaultSort, 0.0f, cwd_entries_table_col_last_write_time);
                imgui::TableHeadersRow();

                expl.sort_specs = imgui::TableGetSortSpecs();

                std::vector<explorer_window::dirent>::iterator first_filtered_cwd_dirent;

                if (expl.sort_specs != nullptr && expl.sort_specs->SpecsDirty) {
                    expl.sort_specs->SpecsDirty = false;
                    scoped_timer<timer_unit::MICROSECONDS> sort_timer(&expl.sort_us);
                    first_filtered_cwd_dirent = sort_cwd_entries(expl);
                } else {
                    first_filtered_cwd_dirent = std::find_if(expl.cwd_entries.begin(),
                                                             expl.cwd_entries.end(),
                                                             [](explorer_window::dirent const &ent) { return ent.is_filtered_out; });
                }

                static explorer_window::dirent const *right_clicked_ent = nullptr;

                ImGuiListClipper clipper;
                {
                    u64 num_dirents_to_render = expl.cwd_entries.size() - num_filtered_dirents;
                    assert(num_dirents_to_render <= (u64)INT32_MAX);
                    clipper.Begin(s32(num_dirents_to_render));
                }

                while (clipper.Step()) {
                    for (u64 i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                        auto &dirent = expl.cwd_entries[i];
                        [[maybe_unused]] char const *path = dirent.basic.path.data();

                        imgui::TableNextRow();

                        if (imgui::TableSetColumnIndex(cwd_entries_table_col_number)) {
                            imgui::Text("%zu", i + 1);
                        }

                        if (imgui::TableSetColumnIndex(cwd_entries_table_col_id)) {
                            imgui::Text("%zu", dirent.basic.id);
                        }

                        if (imgui::TableSetColumnIndex(cwd_entries_table_col_path)) {
                            char buffer[2048]; init_empty_cstr(buffer);
                            snprintf(buffer, lengthof(buffer), "%s##dirent%zu", path, i);

                            imgui::TextColored(get_color(dirent.basic.type), "%s", dirent.basic.kind_icon());

                            imgui::SameLine();

                            if (imgui::Selectable(buffer, dirent.is_selected, ImGuiSelectableFlags_SpanAllColumns)) {
                                debug_log("[ %d ] selected [%s]", expl.id, dirent.basic.path.data());

                                if (!io.KeyCtrl && !io.KeyShift) {
                                    // entry was selected but Ctrl was not held, so deselect everything
                                    expl.deselect_all_cwd_entries();
                                }

                                flip_bool(dirent.is_selected);

                                if (io.KeyShift) {
                                    // shift click, select everything between the current item and the previously clicked item

                                    u64 first_idx, last_idx;

                                    if (expl.cwd_prev_selected_dirent_idx == explorer_window::NO_SELECTION) {
                                        // nothing in cwd has been selected, so start selection from very first entry
                                        expl.cwd_prev_selected_dirent_idx = 0;
                                    }

                                    if (i <= expl.cwd_prev_selected_dirent_idx) {
                                        // prev selected item below current one
                                        first_idx = i;
                                        last_idx = expl.cwd_prev_selected_dirent_idx;
                                    }
                                    else {
                                        first_idx = expl.cwd_prev_selected_dirent_idx;
                                        last_idx = i;
                                    }

                                    debug_log("[ %d ] shift click, [%zu, %zu]", expl.id, first_idx, last_idx);

                                    for (u64 j = first_idx; j <= last_idx; ++j) {
                                        auto &dirent_ = expl.cwd_entries[j];
                                        if (!dirent_.basic.is_dotdot()) {
                                            dirent_.is_selected = true;
                                        }
                                    }
                                }

                                static f64 last_click_time = 0;
                                static swan_path_t last_click_path = {};
                                swan_path_t const &current_click_path = dirent.basic.path;
                                f64 const double_click_window_sec = 0.3;
                                f64 current_time = imgui::GetTime();
                                f64 seconds_between_clicks = current_time - last_click_time;

                                if (seconds_between_clicks <= double_click_window_sec && path_equals_exactly(current_click_path, last_click_path)) {
                                    if (dirent.basic.is_directory()) {
                                        debug_log("[ %d ] double clicked directory [%s]", expl.id, dirent.basic.path.data());

                                        if (dirent.basic.is_dotdot()) {
                                            auto result = try_ascend_directory(expl);

                                            if (!result.success) {
                                                char action[2048]; init_empty_cstr(action);
                                                s32 written = snprintf(action, lengthof(action), "ascend to directory [%s]", result.parent_dir.data());
                                                assert(written < lengthof(action));

                                                swan_open_popup_modal_error(action, "could not find directory");
                                            }

                                            goto exit_cwd_entries_passing_filter_loop;
                                        }
                                        else {
                                            char const *target = dirent.basic.path.data();
                                            auto result = try_descend_to_directory(expl, target);

                                            if (!result.success) {
                                                char action[2048]; init_empty_cstr(action);
                                                s32 written = snprintf(action, lengthof(action), "descend to directory [%s]", target);
                                                assert(written < lengthof(action));

                                                swan_open_popup_modal_error(action, result.err_msg.c_str());
                                            }

                                            goto exit_cwd_entries_passing_filter_loop;
                                        }
                                    }
                                    else if (dirent.basic.is_symlink()) {
                                        debug_log("[ %d ] double clicked symlink [%s]", expl.id, dirent.basic.path.data());

                                        auto res = open_symlink(dirent, expl, dir_sep_utf8);

                                        if (res.success) {
                                            auto const &target_dir_path = res.error_or_utf8_path;
                                            if (!target_dir_path.empty()) {
                                                expl.cwd = path_create(target_dir_path.c_str());
                                                (void) update_cwd_entries(full_refresh, &expl, expl.cwd.data());
                                                (void) expl.save_to_disk();
                                                expl.set_latest_valid_cwd_then_notify(expl.cwd);
                                                new_history_from(expl, expl.cwd);
                                            }
                                        } else {
                                            char action[2048]; init_empty_cstr(action);
                                            s32 written = snprintf(action, lengthof(action), "open symlink [%s]", dirent.basic.path.data());
                                            assert(written < lengthof(action));

                                            swan_open_popup_modal_error(action, res.error_or_utf8_path.c_str());
                                        }
                                    }
                                    else {
                                        debug_log("[ %d ] double clicked file [%s]", expl.id, dirent.basic.path.data());

                                        auto res = open_file(dirent, expl, dir_sep_utf8);

                                        if (res.success) {
                                            if (!res.error_or_utf8_path.empty()) {

                                            }
                                        } else {
                                            char action[2048]; init_empty_cstr(action);
                                            s32 written = snprintf(action, lengthof(action), "open file [%s]", dirent.basic.path.data());
                                            assert(written < lengthof(action));

                                            swan_open_popup_modal_error(action, res.error_or_utf8_path.c_str());
                                        }
                                    }
                                }
                                else if (dirent.basic.is_dotdot()) {
                                    debug_log("[ %d ] selected [%s]", expl.id, dirent.basic.path.data());
                                }

                                last_click_time = current_time;
                                last_click_path = current_click_path;
                                expl.cwd_prev_selected_dirent_idx = i;

                            } // imgui::Selectable

                            if (dirent.basic.is_dotdot()) {
                                dirent.is_selected = false; // do no allow [..] to be selected
                            }

                            if (imgui::IsItemClicked(ImGuiMouseButton_Right) && !dirent.basic.is_dotdot()) {
                                debug_log("[ %d ] right clicked [%s]", expl.id, dirent.basic.path.data());
                                imgui::OpenPopup("Context");
                                right_clicked_ent = &dirent;
                            }

                        } // path column

                        if (imgui::TableSetColumnIndex(cwd_entries_table_col_type)) {
                            if (dirent.basic.is_directory()) {
                                imgui::TextUnformatted("dir");
                            }
                            else if (dirent.basic.is_symlink()) {
                                imgui::TextUnformatted("link");
                            }
                            else {
                                imgui::TextUnformatted("file");
                            }
                        }

                        if (imgui::TableSetColumnIndex(cwd_entries_table_col_size_pretty)) {
                            if (dirent.basic.is_directory()) {
                                imgui::TextUnformatted("");
                            }
                            else {
                                std::array<char, 32> pretty_size = {};
                                format_file_size(dirent.basic.size, pretty_size.data(), pretty_size.size(), size_unit_multiplier);
                                imgui::TextUnformatted(pretty_size.data());
                            }
                        }

                        if (imgui::TableSetColumnIndex(cwd_entries_table_col_size_bytes)) {
                            if (dirent.basic.is_directory()) {
                                imgui::TextUnformatted("");
                            }
                            else {
                                imgui::Text("%zu", dirent.basic.size);
                            }
                        }

                        // TODO: reintroduce column
                        // if (imgui::TableSetColumnIndex(cwd_entries_table_col_creation_time)) {
                        //     auto [result, buffer] = filetime_to_string(&dirent.last_write_time_raw);
                        //     imgui::TextUnformatted(buffer.data());
                        // }

                        if (imgui::TableSetColumnIndex(cwd_entries_table_col_last_write_time)) {
                            auto [result, buffer] = filetime_to_string(&dirent.basic.last_write_time_raw);
                            imgui::TextUnformatted(buffer.data());
                        }

                    }
                }
                exit_cwd_entries_passing_filter_loop:;

                if (imgui::BeginPopup("Context")) {
                    if (num_selected_dirents <= 1) {
                        assert(right_clicked_ent != nullptr);

                        {
                            imgui_scoped_text_color tc(get_color(right_clicked_ent->basic.type));
                            imgui::SeparatorText(right_clicked_ent->basic.path.data());
                        }

                        // bool is_directory = right_clicked_ent->basic.is_directory();

                        if (imgui::Selectable("Copy name")) {
                            imgui::SetClipboardText(right_clicked_ent->basic.path.data());
                        }
                        if (imgui::Selectable("Copy full path")) {
                            swan_path_t full_path = path_create(expl.cwd.data());
                            if (!path_append(full_path, right_clicked_ent->basic.path.data(), dir_sep_utf8, true)) {
                                char action[2048]; init_empty_cstr(action);
                                s32 written = snprintf(action, lengthof(action), "copy full path of [%s]", right_clicked_ent->basic.path.data());
                                assert(written < lengthof(action));

                                char failure[1024]; init_empty_cstr(failure);
                                written = snprintf(failure, lengthof(failure), "max path length exceeded when appending name to cwd");
                                assert(written < lengthof(failure));

                                swan_open_popup_modal_error(action, failure);
                            } else {
                                imgui::SetClipboardText(full_path.data());
                            }
                        }
                        if (imgui::Selectable("Copy size (bytes)")) {
                            imgui::SetClipboardText(std::to_string(right_clicked_ent->basic.size).c_str());
                        }
                        if (imgui::Selectable("Copy size (pretty)")) {
                            char buffer[32]; init_empty_cstr(buffer);
                            format_file_size(right_clicked_ent->basic.size, buffer, lengthof(buffer), size_unit_multiplier);
                            imgui::SetClipboardText(buffer);
                        }
                        if (imgui::Selectable("Reveal in File Explorer")) {
                            auto res = reveal_in_file_explorer(*right_clicked_ent, expl, dir_sep_utf16);

                            if (!res.success) {
                                char action[2048]; init_empty_cstr(action);
                                s32 written = snprintf(action, lengthof(action), "reveal [%s] in File Explorer", right_clicked_ent->basic.path.data());
                                assert(written < lengthof(action));

                                swan_open_popup_modal_error(action, res.error_or_utf8_path.c_str());
                            }
                        }
                        if (imgui::Selectable("Rename##single_open")) {
                            open_single_rename_popup = true;
                            dirent_to_be_renamed = right_clicked_ent;
                        }
                        if (imgui::Selectable("Delete##from_context")) {
                            auto result = delete_selected_entries(expl, expl.cwd_entries, dir_sep_utf16);
                            if (!result.success) {
                                std::stringstream action;
                                action << "delete [" << right_clicked_ent->basic.path.data() << "]";
                                swan_open_popup_modal_error(action.str().c_str(), result.error_or_utf8_path.c_str());
                            }
                        }
                    }
                    else {
                        // right click when > 1 dirents selected

                        if (imgui::Selectable("Cut")) {

                        }
                        if (imgui::Selectable("Copy")) {

                        }
                        if (imgui::Selectable("Delete")) {
                            auto result = delete_selected_entries(expl, expl.cwd_entries, dir_sep_utf16);
                            if (!result.success) {
                                std::stringstream action;
                                action << "delete " << num_selected_dirents << " entries from [" << expl.cwd.data() << "]";
                                swan_open_popup_modal_error(action.str().c_str(), result.error_or_utf8_path.c_str());
                            }
                        }
                        if (imgui::Selectable("Bulk Rename")) {
                            open_bulk_rename_popup = true;
                        }
                    }

                    imgui::EndPopup();
                }

                imgui::EndTable();
            }
            if (ImGui::IsItemHovered() && io.KeyCtrl && imgui::IsKeyPressed(ImGuiKey_A)) {
                expl.select_all_cwd_entries();
            }
            if (ImGui::IsItemHovered() && imgui::IsKeyPressed(ImGuiKey_Escape)) {
                expl.deselect_all_cwd_entries();
            }
        }

        imgui::EndChild();
    }
    // cwd entries stats & table end

    if (open_single_rename_popup) {
        swan_open_popup_modal_single_rename(expl, *dirent_to_be_renamed, [&expl]() {
            /* on rename finished: */
            (void) update_cwd_entries(full_refresh, &expl, expl.cwd.data());
        });
    }
    if (open_bulk_rename_popup) {
        swan_open_popup_modal_bulk_rename(expl, [&]() {
            /* on rename finished: */
            (void) update_cwd_entries(full_refresh, &expl, expl.cwd.data());
        });
    }

    imgui::End();
}

void explorer_change_notif_thread_func(explorer_window &expl, std::atomic<s32> const &window_close_flag) noexcept
{
    // (void) set_thread_priority(THREAD_PRIORITY_BELOW_NORMAL);

    DWORD const notify_filter =
        FILE_NOTIFY_CHANGE_CREATION   |
        FILE_NOTIFY_CHANGE_DIR_NAME   |
        FILE_NOTIFY_CHANGE_FILE_NAME  |
        FILE_NOTIFY_CHANGE_LAST_WRITE |
        FILE_NOTIFY_CHANGE_SIZE
    ;

    HANDLE watch_handle = {};
    wchar_t watch_target_utf16[2048] = {};
    swan_path_t watch_target_utf8 = {};
    time_point_t last_refresh_notif_sent = {};

    {
        std::scoped_lock lock(expl.latest_valid_cwd_mutex);
        watch_target_utf8 = expl.cwd;
    }

    s32 utf_written = utf8_to_utf16(watch_target_utf8.data(), watch_target_utf16, 2048);
    if (utf_written == 0) {
        debug_log("[ %d ] utf8_to_utf16 failed during setup: watch_target_utf8 -> watch_target_utf16", expl.id);
    }

    watch_handle = FindFirstChangeNotificationW(watch_target_utf16, false, notify_filter);
    if (watch_handle == INVALID_HANDLE_VALUE) {
        debug_log("[ %d ] FindFirstChangeNotificationW failed during setup: INVALID_HANDLE_VALUE", expl.id);
    } else {
        debug_log("[ %d ] FindFirstChangeNotificationW initial setup success for [%s]", expl.id, watch_target_utf8.data());
    }

    while (!window_close_flag.load()) {
        expl.is_window_visible.wait(false); // wait for window to become visible

        swan_path_t latest_valid_cwd;
        {
            std::scoped_lock lock(expl.latest_valid_cwd_mutex);
            latest_valid_cwd = expl.latest_valid_cwd;
        }
        if (!path_loosely_same(latest_valid_cwd, watch_target_utf8)) {
            // new watch target
            watch_target_utf8 = latest_valid_cwd;

            if (watch_handle != INVALID_HANDLE_VALUE) {
                // stop watching old directory, if there was one
                FindCloseChangeNotification(watch_handle);
            }

            debug_log("[ %d ] watch target changed, now: [%s]", expl.id, latest_valid_cwd.data());

            if (path_is_empty(latest_valid_cwd)) {
                watch_handle = INVALID_HANDLE_VALUE;
            }
            else {
                utf_written = utf8_to_utf16(latest_valid_cwd.data(), watch_target_utf16, 2048);
                if (utf_written == 0) {
                    debug_log("[ %d ] utf8_to_utf16 failed: latest_valid_cwd -> watch_target_utf16", expl.id);
                } else {
                    watch_handle = FindFirstChangeNotificationW(watch_target_utf16, false, notify_filter);
                    if (watch_handle == INVALID_HANDLE_VALUE) {
                        debug_log("[ %d ] FindFirstChangeNotificationW failed: INVALID_HANDLE_VALUE", expl.id);
                    }
                }
            }
        }

        if (watch_handle == INVALID_HANDLE_VALUE) {
            // latest_valid_cwd is invalid for some reason, wait for it to change.
            // during this time window visibility can change, but it doesn't matter.
            std::unique_lock lock(expl.latest_valid_cwd_mutex);
            expl.latest_valid_cwd_cond.wait(lock, []() { return true; });
        }
        else {
            // latest_valid_cwd is in fact a valid directory, so sit here and wait for a change notification.
            // there is a timeout because latest_valid_cwd may change while we wait, which invalidates the current watch_handle,
            // so we need to occasionally break and re-establish the watch_handle against the correct directory.
            DWORD wait_status = WaitForSingleObject(watch_handle, get_explorer_options().auto_refresh_interval_ms.load());

            swan_path_t latest_target_utf8;
            {
                std::scoped_lock lock(expl.latest_valid_cwd_mutex);
                latest_target_utf8 = expl.cwd;
            }
            if (!path_loosely_same(watch_target_utf8, latest_target_utf8)) {
                // latest_valid_cwd changed as we were waiting for watch_handle,
                // invalidating this change notification setup. do nothing.
            }
            else {
                switch (wait_status) {
                    case WAIT_OBJECT_0: {
                        debug_log("[ %d ] WAIT_OBJECT_0 h=%d target=[%s]", expl.id, watch_handle, watch_target_utf8.data());

                        if (expl.is_window_visible.load()) {
                            time_point_t now = current_time();

                            if (compute_diff_ms(last_refresh_notif_sent, now) >= get_explorer_options().min_tolerable_refresh_interval_ms) {
                                debug_log("[ %d ] refresh notif SEND", expl.id);

                                expl.refresh_notif_time.store(now, std::memory_order::seq_cst);
                                last_refresh_notif_sent = now;

                                if (!FindNextChangeNotification(watch_handle)) {
                                    debug_log("[ %d ] FindNextChangeNotification failed", expl.id);
                                }
                            }
                        }

                        break;
                    }
                    case WAIT_TIMEOUT:
                        break;
                    case WAIT_FAILED:
                        debug_log("[ %d ] WAIT_FAILED h=%d target=[%s]", expl.id, watch_handle, watch_target_utf8.data());
                        break;
                    default:
                        assert(false && "Unhandled wait_status");
                        break;
                }
            }
        }
    }

    BOOL closed = FindCloseChangeNotification(watch_handle);
    assert(closed && "FindCloseChangeNotification failed at program exit");
}
