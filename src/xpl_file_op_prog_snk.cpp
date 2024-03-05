#include "stdafx.hpp"
#include "data_types.hpp"
#include "imgui_specific.hpp"

basic_dirent::kind derive_obj_type(DWORD attributes) noexcept
{
    if (attributes & SFGAO_LINK) {
        return basic_dirent::kind::symlink_ambiguous;
    } else {
        return attributes & SFGAO_FOLDER ? basic_dirent::kind::directory : basic_dirent::kind::file;
    }
}

HRESULT explorer_file_op_progress_sink::StartOperations() noexcept { print_debug_msg("explorer_file_op_progress_sink :: StartOperations"); return S_OK; }
HRESULT explorer_file_op_progress_sink::PauseTimer()      noexcept { print_debug_msg("explorer_file_op_progress_sink :: PauseTimer");      return S_OK; }
HRESULT explorer_file_op_progress_sink::ResetTimer()      noexcept { print_debug_msg("explorer_file_op_progress_sink :: ResetTimer");      return S_OK; }
HRESULT explorer_file_op_progress_sink::ResumeTimer()     noexcept { print_debug_msg("explorer_file_op_progress_sink :: ResumeTimer");     return S_OK; }

HRESULT explorer_file_op_progress_sink::PostNewItem(DWORD, IShellItem *, LPCWSTR, LPCWSTR, DWORD, HRESULT, IShellItem *) noexcept { print_debug_msg("explorer_file_op_progress_sink :: PostNewItem");    return S_OK; }
HRESULT explorer_file_op_progress_sink::PostRenameItem(DWORD, IShellItem *, LPCWSTR, HRESULT, IShellItem *)              noexcept { print_debug_msg("explorer_file_op_progress_sink :: PostRenameItem"); return S_OK; }

HRESULT explorer_file_op_progress_sink::PreCopyItem(DWORD, IShellItem *, IShellItem *, LPCWSTR) noexcept { print_debug_msg("explorer_file_op_progress_sink :: PreCopyItem");   return S_OK; }
HRESULT explorer_file_op_progress_sink::PreDeleteItem(DWORD, IShellItem *)                      noexcept { print_debug_msg("explorer_file_op_progress_sink :: PreDeleteItem"); return S_OK; }
HRESULT explorer_file_op_progress_sink::PreMoveItem(DWORD, IShellItem *, IShellItem *, LPCWSTR) noexcept { print_debug_msg("explorer_file_op_progress_sink :: PreMoveItem");   return S_OK; }
HRESULT explorer_file_op_progress_sink::PreNewItem(DWORD, IShellItem *, LPCWSTR)                noexcept { print_debug_msg("explorer_file_op_progress_sink :: PreNewItem");    return S_OK; }
HRESULT explorer_file_op_progress_sink::PreRenameItem(DWORD, IShellItem *, LPCWSTR)             noexcept { print_debug_msg("explorer_file_op_progress_sink :: PreRenameItem"); return S_OK; }

HRESULT explorer_file_op_progress_sink::PostMoveItem(
    [[maybe_unused]] DWORD flags,
    [[maybe_unused]] IShellItem *src_item,
    [[maybe_unused]] IShellItem *destination,
    LPCWSTR new_name_utf16,
    HRESULT result,
    [[maybe_unused]] IShellItem *dst_item) noexcept
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
    bool free_dst_path_utf16 = false;
    swan_path_t dst_path_utf8 = path_create("");

    if (dst_item != nullptr) {
        if (FAILED(dst_item->GetDisplayName(SIGDN_FILESYSPATH, &dst_path_utf16))) {
            return S_OK;
        }
        free_dst_path_utf16 = true;

        if (!utf16_to_utf8(dst_path_utf16, dst_path_utf8.data(), dst_path_utf8.max_size())) {
            return S_OK;
        }
    }
    SCOPE_EXIT { if (free_dst_path_utf16) CoTaskMemFree(dst_path_utf16); };

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

    path_force_separator(src_path_utf8, global_state::settings().dir_separator_utf8);
    path_force_separator(dst_path_utf8, global_state::settings().dir_separator_utf8);

    {
        auto pair = global_state::completed_file_ops();
        auto &completed_file_ops = *pair.first;
        auto &mutex = *pair.second;

        std::scoped_lock lock(mutex);

        basic_dirent::kind obj_type = derive_obj_type(attributes);
        auto completion_time = current_time_system();
        completed_file_ops.emplace_front(completion_time, file_operation_type::move, src_path_utf8.data(), dst_path_utf8.data(), obj_type, this->group_id);
    }

    return S_OK;
}

HRESULT explorer_file_op_progress_sink::PostDeleteItem(DWORD, IShellItem *item, HRESULT result, IShellItem *item_newly_created) noexcept
{
    if (FAILED(result)) {
        return S_OK;
    }

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

    // Extract recycle bin item path
    wchar_t *recycle_bin_hardlink_path_utf16 = nullptr;
    bool free_recycle_bin_hardlink_path_utf16 = false;
    swan_path_t recycle_bin_item_path_utf8 = path_create("");

    if (item_newly_created != nullptr) {
        if (FAILED(item_newly_created->GetDisplayName(SIGDN_FILESYSPATH, &recycle_bin_hardlink_path_utf16))) {
            return S_OK;
        }
        free_recycle_bin_hardlink_path_utf16 = true;

        if (!utf16_to_utf8(recycle_bin_hardlink_path_utf16, recycle_bin_item_path_utf8.data(), recycle_bin_item_path_utf8.max_size())) {
            return S_OK;
        }
    }
    SCOPE_EXIT { if (free_recycle_bin_hardlink_path_utf16) CoTaskMemFree(recycle_bin_hardlink_path_utf16); };

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

        basic_dirent::kind obj_type = derive_obj_type(attributes);
        auto completion_time = current_time_system();
        completed_file_ops.emplace_front(completion_time, file_operation_type::del, deleted_item_path_utf8.data(), recycle_bin_item_path_utf8.data(), obj_type, this->group_id);
    }

    print_debug_msg("PostDeleteItem [%s] [%s]", deleted_item_path_utf8.data(), recycle_bin_item_path_utf8.data());

    return S_OK;
}

HRESULT explorer_file_op_progress_sink::PostCopyItem(DWORD, IShellItem *src_item, IShellItem *, LPCWSTR new_name_utf16, HRESULT result, IShellItem *dst_item) noexcept
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

    print_debug_msg("PostCopyItem [%s] -> [%s]", src_path_utf8.data(), dst_path_utf8.data());

    swan_path_t new_name_utf8;

    if (!utf16_to_utf8(new_name_utf16, new_name_utf8.data(), new_name_utf8.size())) {
        return S_OK;
    }

    path_force_separator(src_path_utf8, global_state::settings().dir_separator_utf8);
    path_force_separator(dst_path_utf8, global_state::settings().dir_separator_utf8);

    {
        auto pair = global_state::completed_file_ops();
        auto &completed_file_ops = *pair.first;
        auto &mutex = *pair.second;

        std::scoped_lock lock(mutex);

        basic_dirent::kind obj_type = derive_obj_type(attributes);
        auto completion_time = current_time_system();
        completed_file_ops.emplace_front(completion_time, file_operation_type::copy, src_path_utf8.data(), dst_path_utf8.data(), obj_type, this->group_id);
    }

    return S_OK;
}

HRESULT explorer_file_op_progress_sink::UpdateProgress(UINT work_total, UINT work_so_far) noexcept
{
    print_debug_msg("explorer_file_op_progress_sink :: UpdateProgress %zu/%zu", work_so_far, work_total);
    return S_OK;
}

HRESULT explorer_file_op_progress_sink::FinishOperations(HRESULT) noexcept
{
    print_debug_msg("explorer_file_op_progress_sink :: FinishOperations");

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
                    if (PathFileExistsW(recent_file_path_utf16)) { // TODO: move out of lock scope
                        new_buffer.push_back(recent_file);
                    }
                }
            }

            recent_files = new_buffer;
        }

        global_state::save_recent_files_to_disk();
    }

    (void) global_state::save_completed_file_ops_to_disk(nullptr);

    return S_OK;
}

ULONG explorer_file_op_progress_sink::AddRef()  noexcept { return 1; }
ULONG explorer_file_op_progress_sink::Release() noexcept { return 1; }

HRESULT explorer_file_op_progress_sink::QueryInterface(const IID &riid, void **ppv) noexcept
{
    print_debug_msg("explorer_file_op_progress_sink :: QueryInterface");

    if (riid == IID_IUnknown || riid == IID_IFileOperationProgressSink) {
        *ppv = static_cast<IFileOperationProgressSink*>(this);
        return S_OK;
    }

    *ppv = nullptr;
    return E_NOINTERFACE;
}
