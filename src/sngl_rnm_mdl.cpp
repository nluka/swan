#include "imgui/imgui.h"

#include "common.hpp"
#include "imgui_specific.hpp"

namespace imgui = ImGui;

static bool s_single_rename_open = false;
static explorer_window *s_single_rename_expl = nullptr;
static explorer_window::dirent const *s_single_rename_entry_to_be_renamed = nullptr;
static std::function<void ()> s_single_rename_on_rename_finish_callback = {};

char const *swan_id_single_rename_popup_modal() noexcept
{
    return "Rename Entry";
}

void swan_open_popup_modal_single_rename(
    explorer_window &expl,
    explorer_window::dirent const &entry_to_be_renamed,
    std::function<void ()> on_rename_finish_callback) noexcept
{
    s_single_rename_open = true;

    assert(s_single_rename_entry_to_be_renamed == nullptr);
    s_single_rename_entry_to_be_renamed = &entry_to_be_renamed;

    assert(s_single_rename_expl == nullptr);
    s_single_rename_expl = &expl;

    s_single_rename_on_rename_finish_callback = on_rename_finish_callback;
}

bool swan_is_popup_modal_open_single_rename() noexcept
{
    return s_single_rename_open;
}

void swan_render_popup_modal_single_rename() noexcept
{
    if (s_single_rename_open) {
        imgui::OpenPopup(swan_id_single_rename_popup_modal());
    }
    if (!imgui::BeginPopupModal(swan_id_single_rename_popup_modal(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }

    assert(s_single_rename_expl != nullptr);
    assert(s_single_rename_entry_to_be_renamed != nullptr);


    wchar_t dir_sep_utf16 = get_explorer_options().dir_separator_utf16();

    static swan_path_t new_name_utf8 = {};
    static std::string err_msg = {};

    auto cleanup_and_close_popup = [&]() {
        s_single_rename_open = false;
        s_single_rename_expl = nullptr;
        s_single_rename_entry_to_be_renamed = nullptr;
        s_single_rename_on_rename_finish_callback = {};

        new_name_utf8[0] = L'\0';
        err_msg.clear();

        imgui::CloseCurrentPopup();
    };

    imgui::TextUnformatted("Current name:");
    imgui::SameLine();
    imgui::TextColored(get_color(s_single_rename_entry_to_be_renamed->basic.type), s_single_rename_entry_to_be_renamed->basic.path.data());

    // set initial focus on input text below
    if (imgui::IsWindowAppearing() && !imgui::IsAnyItemActive() && !imgui::IsMouseClicked(0)) {
        imgui::SetKeyboardFocusHere(0);
        new_name_utf8 = s_single_rename_entry_to_be_renamed->basic.path;
    }
    {
        [[maybe_unused]] imgui_scoped_avail_width width = {};

        if (imgui::InputTextWithHint(
            "##New name", "New name...", new_name_utf8.data(), new_name_utf8.size(),
            ImGuiInputTextFlags_CallbackCharFilter, filter_chars_callback, (void *)windows_illegal_filename_chars())
        ) {
            err_msg.clear();
        }
    }

    if (imgui::Button("Rename##single") && !strempty(new_name_utf8.data())) {
        wchar_t buffer_cwd_utf16[MAX_PATH] = {};
        wchar_t buffer_old_name_utf16[MAX_PATH] = {};
        wchar_t buffer_new_name_utf16[MAX_PATH] = {};
        std::wstring old_path_utf16 = {};
        std::wstring new_path_utf16 = {};
        s32 utf_written = 0;
        s32 result = {};

        utf_written = utf8_to_utf16(s_single_rename_expl->cwd.data(), buffer_cwd_utf16, lengthof(buffer_cwd_utf16));

        if (utf_written == 0) {
            debug_log("[%s] utf8_to_utf16 failed (expl.cwd -> buffer_cwd_utf16)", s_single_rename_expl->name);
            cleanup_and_close_popup();
            goto cancel_rename;
        }

        utf_written = utf8_to_utf16(s_single_rename_entry_to_be_renamed->basic.path.data(), buffer_old_name_utf16, lengthof(buffer_old_name_utf16));

        if (utf_written == 0) {
            debug_log("[%s] utf8_to_utf16 failed (s_entry_to_be_renamed.basic.path -> buffer_old_name_utf16)", s_single_rename_expl->name);
            cleanup_and_close_popup();
            goto cancel_rename;
        }

        utf_written = utf8_to_utf16(new_name_utf8.data(), buffer_new_name_utf16, lengthof(buffer_new_name_utf16));

        if (utf_written == 0) {
            debug_log("[%s] utf8_to_utf16 failed (new_name_utf8 -> buffer_new_name_utf16)", s_single_rename_expl->name);
            cleanup_and_close_popup();
            goto cancel_rename;
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
                case EACCES: err_msg = "New path already exists or couldn't be created."; break;
                case ENOENT: err_msg = "Old path not found, probably a bug. Sorry!"; break;
                case EINVAL: err_msg = "Name contains invalid characters."; break;
                default: err_msg = get_last_error_string(); break;
            }
        }
        else {
            s_single_rename_on_rename_finish_callback();
            cleanup_and_close_popup();
        }

        cancel_rename:;
    }

    imgui::SameLine();

    if (imgui::Button("Restore##restore_current_name")) {
        new_name_utf8 = path_create(s_single_rename_entry_to_be_renamed->basic.path.data());
    }

    imgui::SameLine();

    if (imgui::Button("Cancel")) {
        cleanup_and_close_popup();
    }

    if (!err_msg.empty()) {
        imgui::TextColored(red(), "Error: %s", err_msg.c_str());
    }

    if (imgui::IsWindowFocused() && imgui::IsKeyPressed(ImGuiKey_Escape)) {
        cleanup_and_close_popup();
    }

    imgui::EndPopup();
}
