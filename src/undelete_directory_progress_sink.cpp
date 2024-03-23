#include "stdafx.hpp"
#include "data_types.hpp"
#include "imgui_dependent_functions.hpp"

HRESULT undelete_directory_progress_sink::StartOperations() noexcept { print_debug_msg("undelete_directory_progress_sink :: StartOperations"); return S_OK; }
HRESULT undelete_directory_progress_sink::PauseTimer()      noexcept { print_debug_msg("undelete_directory_progress_sink :: PauseTimer");      return S_OK; }
HRESULT undelete_directory_progress_sink::ResetTimer()      noexcept { print_debug_msg("undelete_directory_progress_sink :: ResetTimer");      return S_OK; }
HRESULT undelete_directory_progress_sink::ResumeTimer()     noexcept { print_debug_msg("undelete_directory_progress_sink :: ResumeTimer");     return S_OK; }

HRESULT undelete_directory_progress_sink::PostNewItem(DWORD, IShellItem *, LPCWSTR, LPCWSTR, DWORD, HRESULT, IShellItem *) noexcept { print_debug_msg("undelete_directory_progress_sink :: PostNewItem");    return S_OK; }
HRESULT undelete_directory_progress_sink::PostRenameItem(DWORD, IShellItem *, LPCWSTR, HRESULT, IShellItem *)              noexcept { print_debug_msg("undelete_directory_progress_sink :: PostRenameItem"); return S_OK; }

HRESULT undelete_directory_progress_sink::PreCopyItem(DWORD, IShellItem *, IShellItem *, LPCWSTR) noexcept { print_debug_msg("undelete_directory_progress_sink :: PreCopyItem");   return S_OK; }
HRESULT undelete_directory_progress_sink::PreDeleteItem(DWORD, IShellItem *)                      noexcept { print_debug_msg("undelete_directory_progress_sink :: PreDeleteItem"); return S_OK; }
HRESULT undelete_directory_progress_sink::PreMoveItem(DWORD, IShellItem *, IShellItem *, LPCWSTR) noexcept { print_debug_msg("undelete_directory_progress_sink :: PreMoveItem");   return S_OK; }
HRESULT undelete_directory_progress_sink::PreNewItem(DWORD, IShellItem *, LPCWSTR)                noexcept { print_debug_msg("undelete_directory_progress_sink :: PreNewItem");    return S_OK; }
HRESULT undelete_directory_progress_sink::PreRenameItem(DWORD, IShellItem *, LPCWSTR)             noexcept { print_debug_msg("undelete_directory_progress_sink :: PreRenameItem"); return S_OK; }

HRESULT undelete_directory_progress_sink::PostMoveItem(DWORD, IShellItem *, IShellItem *, LPCWSTR, HRESULT, IShellItem *) noexcept { print_debug_msg("undelete_directory_progress_sink :: PostMoveItem");   return S_OK; }
HRESULT undelete_directory_progress_sink::PostDeleteItem(DWORD, IShellItem *, HRESULT, IShellItem *)                      noexcept { print_debug_msg("undelete_directory_progress_sink :: PostDeleteItem"); return S_OK; }
HRESULT undelete_directory_progress_sink::PostCopyItem(DWORD, IShellItem *, IShellItem *, LPCWSTR, HRESULT, IShellItem *) noexcept { print_debug_msg("undelete_directory_progress_sink :: PostCopyItem");   return S_OK; }

HRESULT undelete_directory_progress_sink::UpdateProgress(UINT work_total, UINT work_so_far) noexcept
{
    print_debug_msg("undelete_directory_progress_sink :: UpdateProgress %zu/%zu", work_so_far, work_total);
    return S_OK;
}

HRESULT undelete_directory_progress_sink::FinishOperations(HRESULT) noexcept
{
    print_debug_msg("undelete_directory_progress_sink :: FinishOperations");

    // path_force_separator(src_path_utf8, global_state::settings().dir_separator_utf8); //! UB
    path_force_separator(this->destination_full_path_utf8, global_state::settings().dir_separator_utf8); //! UB

    {
        auto completed_file_operations = global_state::completed_file_operations_get();

        std::scoped_lock lock(*completed_file_operations.mutex);

        auto found = std::find_if(
            completed_file_operations.container->begin(),
            completed_file_operations.container->end(),
            [&](completed_file_operation const &cfo) noexcept {
                return path_equals_exactly(cfo.src_path, this->destination_full_path_utf8);
            }
        );

        if (found != completed_file_operations.container->end()) {
            found->undo_time = current_time_system();
            (void) global_state::completed_file_operations_save_to_disk(&lock);
        }
    }

    return S_OK;
}

ULONG undelete_directory_progress_sink::AddRef()  noexcept { return 1; }
ULONG undelete_directory_progress_sink::Release() noexcept { return 1; }

HRESULT undelete_directory_progress_sink::QueryInterface(const IID &riid, void **ppv) noexcept
{
    print_debug_msg("undelete_directory_progress_sink :: QueryInterface");

    if (riid == IID_IUnknown || riid == IID_IFileOperationProgressSink) {
        *ppv = static_cast<IFileOperationProgressSink*>(this);
        return S_OK;
    }

    *ppv = nullptr;
    return E_NOINTERFACE;
}
