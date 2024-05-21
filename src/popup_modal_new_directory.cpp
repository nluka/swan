#include "stdafx.hpp"
#include "common_functions.hpp"
#include "imgui_dependent_functions.hpp"

namespace new_directory_modal_global_state
{
    static bool         g_open = false;
    static swan_path    g_parent_path_utf8 = {};
    static s32          g_initiating_expl_id = -1;
}

void swan_popup_modals::open_new_directory(char const *parent_directory_utf8, s32 initiating_expl_id) noexcept
{
    using namespace new_directory_modal_global_state;

    g_open = true;
    g_parent_path_utf8 = path_create(parent_directory_utf8);
    g_initiating_expl_id = initiating_expl_id;
}

void swan_popup_modals::render_new_directory() noexcept
{
    using namespace new_directory_modal_global_state;

    if (g_open) {
        imgui::OpenPopup(swan_popup_modals::label_new_directory);
    }
    if (!imgui::BeginPopupModal(swan_popup_modals::label_new_directory, nullptr, ImGuiWindowFlags_NoResize|ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }

    static swan_path s_dir_name_utf8 = path_create("");
    static std::string s_err_msg = {};

    auto cleanup_and_close_popup = [&]() noexcept {
        g_open = false;
        g_parent_path_utf8 = {};
        g_initiating_expl_id = -1;

        path_clear(s_dir_name_utf8);
        s_err_msg.clear();

        imgui::CloseCurrentPopup();
    };

    auto attempt_create = [&]() noexcept {
        if (path_is_empty(s_dir_name_utf8)) {
            s_err_msg = "Directory name cannot be blank.";
            return;
        }

        wchar_t dir_sep_utf16 = global_state::settings().dir_separator_utf16;
        wchar_t parent_path_utf16[MAX_PATH];
        wchar_t dir_name_utf16[MAX_PATH];
        std::wstring create_path = {};
        BOOL result = {};

        if (!utf8_to_utf16(g_parent_path_utf8.data(), parent_path_utf16, lengthof(parent_path_utf16))) {
            cleanup_and_close_popup();
            return;
        }

        if (!utf8_to_utf16(s_dir_name_utf8.data(), dir_name_utf16, lengthof(dir_name_utf16))) {
            cleanup_and_close_popup();
            return;
        }

        create_path.reserve(1024);

        create_path = parent_path_utf16;
        if (!create_path.ends_with(dir_sep_utf16)) {
            create_path += dir_sep_utf16;
        }
        create_path += dir_name_utf16;

        SECURITY_ATTRIBUTES security = {};
        security.nLength = sizeof(security);
        security.lpSecurityDescriptor = nullptr; // allow all access
        security.bInheritHandle = FALSE;

        WCOUT_IF_DEBUG("CreateDirectoryW [" << create_path << "]\n");
        result = CreateDirectoryW(create_path.c_str(), nullptr);

        if (result == 0) {
            auto winapi_error = get_last_winapi_error();
            switch (winapi_error.code) {
                case ERROR_ALREADY_EXISTS: s_err_msg = "File or directory with same name already exists."; break;
                case ERROR_PATH_NOT_FOUND: s_err_msg = "One or more intermediate directories do not exist. This is probably a bug. Sorry!"; break;
                default: s_err_msg = get_last_winapi_error().formatted_message; break;
            }
            print_debug_msg("FAILED CreateDirectoryW: %d, %s", result, s_err_msg.c_str());
        } else {
            if (g_initiating_expl_id != -1) {
                explorer_window &expl = global_state::explorers()[g_initiating_expl_id];
                expl.deselect_all_cwd_entries();
                std::scoped_lock lock(expl.select_cwd_entries_on_next_update_mutex);
                expl.select_cwd_entries_on_next_update.push_back(s_dir_name_utf8);
            }
            cleanup_and_close_popup();
        }
    };

    imgui::SetKeyboardFocusHere(0);
    {
        imgui::ScopedAvailWidth w(help_indicator_size().x + imgui::GetStyle().ItemSpacing.x);

        if (imgui::InputTextWithHint("##dir_name_input", "Directory name", s_dir_name_utf8.data(), s_dir_name_utf8.max_size(),
                                     ImGuiInputTextFlags_CallbackCharFilter, filter_chars_callback, (void *)windows_illegal_filename_chars()))
        {
            s_err_msg.clear();
        }
    }
    if (imgui::IsItemFocused() && imgui::IsKeyPressed(ImGuiKey_Enter)) {
        attempt_create();
    }

    imgui::SameLine();
    auto help = render_help_indicator(true);
    if (help.hovered) {
        imgui::SetTooltip("[Enter]   Create\n"
                          "[Escape]  Exit");
    }

    if (!s_err_msg.empty()) {
        imgui::TextColored(error_color(), "Error: %s", s_err_msg.c_str());
    }

    if (imgui::IsWindowFocused() && imgui::IsKeyPressed(ImGuiKey_Escape)) {
        cleanup_and_close_popup();
    }

    imgui::EndPopup();
}
