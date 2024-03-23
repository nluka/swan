#include "common_functions.hpp"
#include "imgui_dependent_functions.hpp"

namespace single_rename_modal_global_state
{
    static bool                             g_open = false;
    static explorer_window *                g_expl_opened_from = nullptr;
    static explorer_window::dirent const *  g_expl_dirent_to_rename = nullptr;
    static std::function<void ()>           g_on_rename_callback = {};
}

void swan_popup_modals::open_single_rename(
    explorer_window &expl,
    explorer_window::dirent const &entry_to_be_renamed,
    std::function<void ()> on_rename_finish_callback) noexcept
{
    using namespace single_rename_modal_global_state;

    g_open = true;
    bit_set(global_state::popup_modals_open_bit_field(), swan_popup_modals::bit_pos_single_rename);

    assert(g_expl_dirent_to_rename == nullptr);
    g_expl_dirent_to_rename = &entry_to_be_renamed;

    assert(g_expl_opened_from == nullptr);
    g_expl_opened_from = &expl;

    g_on_rename_callback = on_rename_finish_callback;
}

bool swan_popup_modals::is_open_single_rename() noexcept
{
    using namespace single_rename_modal_global_state;

    return g_open;
}

void swan_popup_modals::render_single_rename() noexcept
{
    using namespace single_rename_modal_global_state;

    if (g_open) {
        imgui::OpenPopup(swan_popup_modals::label_single_rename);
    }
    if (!imgui::BeginPopupModal(swan_popup_modals::label_single_rename, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }

    assert(g_expl_opened_from != nullptr);
    assert(g_expl_dirent_to_rename != nullptr);

    char dir_sep_utf8 = global_state::settings().dir_separator_utf8;
    wchar_t dir_sep_utf16 = global_state::settings().dir_separator_utf16;

    static swan_path s_new_name_utf8 = {};
    static std::string s_err_msg = {};

    auto cleanup_and_close_popup = [&]() noexcept {
        g_open = false;
        bit_clear(global_state::popup_modals_open_bit_field(), swan_popup_modals::bit_pos_single_rename);

        g_expl_opened_from = nullptr;
        g_expl_dirent_to_rename = nullptr;
        g_on_rename_callback = {};

        s_new_name_utf8[0] = L'\0';
        s_err_msg.clear();

        imgui::CloseCurrentPopup();
    };

    auto attempt_rename = [&]() noexcept {
        wchar_t buffer_cwd_utf16[MAX_PATH] = {};
        wchar_t buffer_old_name_utf16[MAX_PATH] = {};
        wchar_t buffer_new_name_utf16[MAX_PATH] = {};
        std::wstring old_path_utf16 = {};
        std::wstring new_path_utf16 = {};
        s32 result = {};

        swan_path old_path_utf8 = g_expl_opened_from->cwd;
        if (!path_append(old_path_utf8, g_expl_dirent_to_rename->basic.path.data(), dir_sep_utf8, true)) {
            cleanup_and_close_popup();
            return;
        }

        swan_path new_path_utf8 = g_expl_opened_from->cwd;
        if (!path_append(new_path_utf8, s_new_name_utf8.data(), dir_sep_utf8, true)) {
            cleanup_and_close_popup();
            return;
        }

        if (!utf8_to_utf16(g_expl_opened_from->cwd.data(), buffer_cwd_utf16, lengthof(buffer_cwd_utf16))) {
            cleanup_and_close_popup();
            return;
        }

        if (!utf8_to_utf16(g_expl_dirent_to_rename->basic.path.data(), buffer_old_name_utf16, lengthof(buffer_old_name_utf16))) {
            cleanup_and_close_popup();
            return;
        }

        if (!utf8_to_utf16(s_new_name_utf8.data(), buffer_new_name_utf16, lengthof(buffer_new_name_utf16))) {
            cleanup_and_close_popup();
            return;
        }

        old_path_utf16 = buffer_cwd_utf16;
        if (!old_path_utf16.ends_with(dir_sep_utf16)) {
            old_path_utf16 += dir_sep_utf16;
        }
        old_path_utf16 += buffer_old_name_utf16;

        new_path_utf16 = buffer_cwd_utf16;
        if (!new_path_utf16.ends_with(dir_sep_utf16)) {
            new_path_utf16 += dir_sep_utf16;
        }
        new_path_utf16 += buffer_new_name_utf16;

        result = _wrename(old_path_utf16.c_str(), new_path_utf16.c_str());

        if (result != 0) {
            auto err_code = errno;
            switch (err_code) {
                case EACCES: s_err_msg = "New path already exists or couldn't be created."; break;
                case ENOENT: s_err_msg = "Old path not found, probably a bug. Sorry!"; break;
                case EINVAL: s_err_msg = "Name contains invalid characters."; break;
                default: s_err_msg = get_last_winapi_error().formatted_message; break;
            }
        }
        else {
            auto recent_files = global_state::recent_files_get();
            {
                std::scoped_lock recent_files_lock(*recent_files.mutex);

                for (auto &rf : *recent_files.container) {
                    if (path_loosely_same(rf.path, old_path_utf8)) {
                        rf.path = new_path_utf8;
                        break;
                    }
                }
            }

            (void) global_state::recent_files_save_to_disk();

            g_on_rename_callback();
            cleanup_and_close_popup();
        }
    };

    // set initial focus on input text below
    if (imgui::IsWindowAppearing() && !imgui::IsAnyItemActive() && !imgui::IsMouseClicked(0)) {
        imgui::SetKeyboardFocusHere(0);
        s_new_name_utf8 = g_expl_dirent_to_rename->basic.path;
    }
    {
        auto style = imgui::GetStyle();
        imgui::ScopedAvailWidth w(imgui::CalcTextSize(ICON_CI_DEBUG_RESTART).x + style.FramePadding.x*2 + style.ItemSpacing.x*2 + imgui::CalcTextSize("(?)").x);

        if (imgui::InputTextWithHint(
            "##New name", "New name...", s_new_name_utf8.data(), s_new_name_utf8.size(),
            ImGuiInputTextFlags_CallbackCharFilter, filter_chars_callback, (void *)windows_illegal_filename_chars())
        ) {
            s_err_msg.clear();
        }
    }
    if (imgui::IsItemFocused() && imgui::IsKeyPressed(ImGuiKey_Enter) && !strempty(s_new_name_utf8.data())) {
        attempt_rename();
    }

    imgui::SameLine();

    if (imgui::Button(ICON_CI_DEBUG_RESTART "##reset_name")) {
        s_new_name_utf8 = path_create(g_expl_dirent_to_rename->basic.path.data());
    }
    if (imgui::IsItemHovered()) {
        if (imgui::BeginTooltip()) {
            imgui::TextUnformatted("Click to reset name to:");
            imgui::TextColored(get_color(g_expl_dirent_to_rename->basic.type), g_expl_dirent_to_rename->basic.path.data());
            imgui::EndTooltip();
        }
    }

    imgui::SameLine();

    imgui::TextUnformatted("(?)");
    if (imgui::IsItemHovered()) {
        imgui::SetTooltip("[Enter]   Rename\n"
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
