#include "stdafx.hpp"
#include "common_functions.hpp"
#include "imgui_dependent_functions.hpp"

namespace new_file_modal_global_state
{
    static bool         g_open = false;
    static swan_path    g_parent_path_utf8 = {};
    static s32          g_initiating_expl_id = -1;
}

void swan_popup_modals::open_new_file(char const *parent_directory_utf8, s32 initiating_expl_id) noexcept
{
    using namespace new_file_modal_global_state;

    g_open = true;
    bit_set(global_state::popup_modals_open_bit_field(), swan_popup_modals::bit_pos_new_file);

    g_parent_path_utf8 = path_create(parent_directory_utf8);
    g_initiating_expl_id = initiating_expl_id;
}

bool swan_popup_modals::is_open_new_file() noexcept
{
    using namespace new_file_modal_global_state;

    return g_open;
}

void swan_popup_modals::render_new_file() noexcept
{
    using namespace new_file_modal_global_state;

    if (g_open) {
        imgui::OpenPopup(swan_popup_modals::label_new_file);
    }
    if (!imgui::BeginPopupModal(swan_popup_modals::label_new_file, nullptr, ImGuiWindowFlags_NoResize|ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }

    static swan_path s_file_name_utf8 = path_create("");
    static std::string s_err_msg = {};

    auto cleanup_and_close_popup = [&]() noexcept {
        g_open = false;
        bit_clear(global_state::popup_modals_open_bit_field(), swan_popup_modals::bit_pos_new_file);

        g_parent_path_utf8 = {};
        g_initiating_expl_id = -1;

        path_clear(s_file_name_utf8);
        s_err_msg.clear();

        imgui::CloseCurrentPopup();
    };

    auto attempt_create = [&]() noexcept {
        if (path_is_empty(s_file_name_utf8)) {
            s_err_msg = "File name cannot be blank.";
            return;
        }

        wchar_t dir_sep_utf16 = global_state::settings().dir_separator_utf16;
        wchar_t parent_path_utf16[MAX_PATH];
        wchar_t file_name_utf16[MAX_PATH];
        std::wstring create_path_utf16 = {};
        HANDLE result = {};

        if (!utf8_to_utf16(g_parent_path_utf8.data(), parent_path_utf16, lengthof(parent_path_utf16))) {
            cleanup_and_close_popup();
            return;
        }

        if (!utf8_to_utf16(s_file_name_utf8.data(), file_name_utf16, lengthof(file_name_utf16))) {
            cleanup_and_close_popup();
            return;
        }

        create_path_utf16.reserve(1024);

        create_path_utf16 = parent_path_utf16;
        if (!create_path_utf16.ends_with(dir_sep_utf16)) {
            create_path_utf16 += dir_sep_utf16;
        }
        create_path_utf16 += file_name_utf16;

        WCOUT_IF_DEBUG("CreateFileW [" << create_path_utf16 << "]\n");
        result = CreateFileW(
            create_path_utf16.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr,
            CREATE_NEW,
            FILE_ATTRIBUTE_NORMAL,
            nullptr
        );

        if (result == INVALID_HANDLE_VALUE) {
            auto winapi_error = get_last_winapi_error();
            switch (winapi_error.code) {
                case ERROR_ALREADY_EXISTS: s_err_msg = "File or directory with same name already exists."; break;
                case ERROR_PATH_NOT_FOUND: s_err_msg = "One or more intermediate directories do not exist; probably a bug. Sorry!"; break;
                default: s_err_msg = get_last_winapi_error().formatted_message; break;
            }
            print_debug_msg("FAILED CreateFileW: %d, %s", result, s_err_msg.c_str());
        } else {
            swan_path create_path_utf8;

            if (utf16_to_utf8(create_path_utf16.c_str(), create_path_utf8.data(), create_path_utf8.max_size())) {
                global_state::recent_files_add("Created", create_path_utf8.data());
                (void) global_state::recent_files_save_to_disk();
            }

            if (g_initiating_expl_id != -1) {
                explorer_window &expl = global_state::explorers()[g_initiating_expl_id];
                expl.deselect_all_cwd_entries();
                std::scoped_lock lock(expl.select_cwd_entries_on_next_update_mutex);
                expl.select_cwd_entries_on_next_update.push_back(s_file_name_utf8);
            }

            cleanup_and_close_popup();
        }
    };

    imgui::SetKeyboardFocusHere(0);
    {
        imgui::ScopedAvailWidth w(imgui::CalcTextSize("(?)").x + imgui::GetStyle().ItemSpacing.x);

        if (imgui::InputTextWithHint("##file_name_input", "File name", s_file_name_utf8.data(), s_file_name_utf8.max_size(),
                                     ImGuiInputTextFlags_CallbackCharFilter, filter_chars_callback, (void *)windows_illegal_filename_chars()))
        {
            s_err_msg.clear();
        }
    }
    if (imgui::IsItemFocused() && imgui::IsKeyPressed(ImGuiKey_Enter)) {
        attempt_create();
    }

    imgui::SameLine();
    imgui::TextUnformatted("(?)");
    if (imgui::IsItemHovered()) {
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
