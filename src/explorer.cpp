#pragma once

#include <regex>
#include <fstream>
#include <iostream>
#include <cstdio>

#include <windows.h>
#include <shlwapi.h>
#include <shlguid.h>
#include <shobjidl.h>
#include <shlobj_core.h>
#include <timezoneapi.h>
#include <datetimeapi.h>

#include <boost/container/static_vector.hpp>

#include "imgui/imgui.h"

#include "common.hpp"
#include "imgui_specific.hpp"
#include "path.hpp"
#include "on_scope_exit.hpp"
#include "scoped_timer.hpp"
#include "util.hpp"

static IShellLinkW *s_shell_link = nullptr;
static IPersistFile *s_persist_file_interface = nullptr;
static wchar_t const *s_illegal_filename_chars = L"\\/<>\"|?*";
static explorer_options s_explorer_options = {};

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

        static_assert(i8(1) == i8(true));
        static_assert(i8(0) == i8(false));

        out << "auto_refresh_interval_ms " << this->auto_refresh_interval_ms << '\n';
        out << "adaptive_refresh_threshold " << this->adaptive_refresh_threshold << '\n';
        out << "ref_mode " << (i32)this->ref_mode << '\n';
        out << "binary_size_system " << (i32)this->binary_size_system << '\n';
        out << "show_cwd_len " << (i32)this->show_cwd_len << '\n';
        out << "show_debug_info " << (i32)this->show_debug_info << '\n';
        out << "show_dotdot_dir " << (i32)this->show_dotdot_dir << '\n';
        out << "unix_directory_separator " << (i32)this->unix_directory_separator << '\n';

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

        static_assert(i8(1) == i8(true));
        static_assert(i8(0) == i8(false));

        std::string what = {};
        what.reserve(100);
        char bit_ch = 0;

        {
            in >> what;
            assert(what == "auto_refresh_interval_ms");
            in >> (i32 &)this->auto_refresh_interval_ms;
        }
        {
            in >> what;
            assert(what == "adaptive_refresh_threshold");
            in >> (i32 &)this->adaptive_refresh_threshold;
        }
        {
            in >> what;
            assert(what == "ref_mode");
            in >> (i32 &)this->ref_mode;
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

drive_list_t query_drive_list() {
    drive_list_t drive_list;

    i32 drives_mask = GetLogicalDrives();

    for (u64 i = 0; i < 26; ++i) {
        if (drives_mask & (1 << i)) {
            char letter = 'A' + (char)i;

            wchar_t drive_root[] = { wchar_t(letter), L':', L'\\', L'\0' };
            wchar_t volume_name[MAX_PATH + 1] = {};
            wchar_t filesystem_name_utf8[MAX_PATH + 1] = {};
            DWORD serial_num = 0;
            DWORD max_component_length = 0;
            DWORD filesystem_flags = 0;
            i32 utf_written = 0;

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
    for (auto &dirent : cwd_entries) {
        dirent.is_selected = false;
    }

    num_selected_cwd_entries = 0;
}

void explorer_window::select_all_cwd_entries(bool select_dotdot_dir) noexcept
{
    for (auto &dirent : cwd_entries) {
        if (!select_dotdot_dir && dirent.basic.is_dotdot()) {
            continue;
        } else {
            dirent.is_selected = true;
        }
    }

    num_selected_cwd_entries = cwd_entries.size();
}

static
std::pair<i32, std::array<char, 64>> filetime_to_string(FILETIME *time) noexcept
{
    std::array<char, 64> buffer = {};
    DWORD flags = FDTF_SHORTDATE | FDTF_SHORTTIME;
    i32 length = SHFormatDateTimeA(time, &flags, buffer.data(), (u32)buffer.size());

    // for some reason, SHFormatDateTimeA will pad parts of the string with ASCII 63 (?)
    // when using LONGDATE or LONGTIME, so we will simply convert them to spaces
    std::replace(buffer.begin(), buffer.end(), '?', ' ');

    return { length, buffer };
}

bool reveal_in_file_explorer(explorer_window::dirent const &entry, explorer_window const &expl, wchar_t dir_sep_utf16) noexcept
{
    wchar_t select_path_cwd_buffer_utf16[MAX_PATH] = {};
    wchar_t select_path_dirent_buffer_utf16[MAX_PATH] = {};
    std::wstring select_command = {};
    i32 utf_written = 0;

    select_command.reserve(1024);

    utf_written = utf8_to_utf16(expl.cwd.data(), select_path_cwd_buffer_utf16, lengthof(select_path_cwd_buffer_utf16));

    if (utf_written == 0) {
        debug_log("[%s] utf8_to_utf16 failed (expl.cwd -> select_path_cwd_buffer_utf16)", expl.name);
        return false;
    }

    select_command += L"/select,";
    select_command += L'"';
    select_command += select_path_cwd_buffer_utf16;
    if (!select_command.ends_with(dir_sep_utf16)) {
        select_command += dir_sep_utf16;
    }

    utf_written = utf8_to_utf16(entry.basic.path.data(), select_path_dirent_buffer_utf16, lengthof(select_path_dirent_buffer_utf16));

    if (utf_written == 0) {
        debug_log("[%s] utf8_to_utf16 failed (entry.basic.path -> select_path_dirent_buffer_utf16)", expl.name);
        return false;
    }

    select_command += select_path_dirent_buffer_utf16;
    select_command += L'"';

    std::wcout << "select_command: [" << select_command.c_str() << "]\n";

    HINSTANCE result = ShellExecuteW(nullptr, L"open", L"explorer.exe", select_command.c_str(), nullptr, SW_SHOWNORMAL);
    // TODO: check result

    return true;
}

typedef wchar_t* filter_chars_callback_user_data_t;

i32 filter_chars_callback(ImGuiInputTextCallbackData *data) noexcept
{
    if (data->EventFlag == ImGuiInputTextFlags_CallbackCharFilter) {
        wchar_t *chars_to_filter = (filter_chars_callback_user_data_t)data->UserData;
        bool is_forbidden = StrChrW(chars_to_filter, data->EventChar);
        if (is_forbidden) {
            data->EventChar = L'\0';
        }
    }

    return 0;
}

void render_rename_entry_popup_modal(
    explorer_window::dirent const &rename_entry,
    explorer_window &expl,
    wchar_t dir_sep_utf16,
    bool &open) noexcept
{
    namespace imgui = ImGui;

    static swan_path_t new_name_utf8 = {};
    static std::string err_msg = {};

    auto cleanup_and_close_popup = [&]() {
        new_name_utf8[0] = L'\0';
        err_msg.clear();
        imgui::CloseCurrentPopup();
        open = false;
    };

    if (imgui::SmallButton("Use##use_current_name")) {
        new_name_utf8 = path_create(rename_entry.basic.path.data());
    }
    imgui::SameLine();
    imgui::TextUnformatted("Current name:");
    imgui::SameLine();
    imgui::TextColored(get_color(rename_entry.basic.type), rename_entry.basic.path.data());

    imgui::Spacing();

    // set initial focus on input text below
    if (imgui::IsWindowAppearing() && !imgui::IsAnyItemActive() && !imgui::IsMouseClicked(0)) {
        imgui::SetKeyboardFocusHere(0);
        new_name_utf8 = rename_entry.basic.path;
    }
    {
        f32 avail_width = imgui::GetContentRegionAvail().x;
        imgui::PushItemWidth(avail_width);

        if (imgui::InputTextWithHint(
            "##New name", "New name...", new_name_utf8.data(), new_name_utf8.size(),
            ImGuiInputTextFlags_CallbackCharFilter, filter_chars_callback, (void *)s_illegal_filename_chars)
        ) {
            err_msg.clear();
        }

        imgui::PopItemWidth();
    }

    imgui::Spacing();

    if (imgui::Button("Rename##single") && new_name_utf8[0] != '\0') {
        wchar_t buffer_cwd_utf16[MAX_PATH] = {};
        wchar_t buffer_old_name_utf16[MAX_PATH] = {};
        wchar_t buffer_new_name_utf16[MAX_PATH] = {};
        std::wstring old_path_utf16 = {};
        std::wstring new_path_utf16 = {};
        i32 utf_written = 0;
        i32 result = {};

        utf_written = utf8_to_utf16(expl.cwd.data(), buffer_cwd_utf16, lengthof(buffer_cwd_utf16));

        if (utf_written == 0) {
            debug_log("[%s] utf8_to_utf16 failed (expl.cwd -> buffer_cwd_utf16)", expl.name);
            cleanup_and_close_popup();
            goto cancel_rename;
        }

        utf_written = utf8_to_utf16(rename_entry.basic.path.data(), buffer_old_name_utf16, lengthof(buffer_old_name_utf16));

        if (utf_written == 0) {
            debug_log("[%s] utf8_to_utf16 failed (rename_entry.basic.path -> buffer_old_name_utf16)", expl.name);
            cleanup_and_close_popup();
            goto cancel_rename;
        }

        utf_written = utf8_to_utf16(new_name_utf8.data(), buffer_new_name_utf16, lengthof(buffer_new_name_utf16));

        if (utf_written == 0) {
            debug_log("[%s] utf8_to_utf16 failed (new_name_utf8 -> buffer_new_name_utf16)", expl.name);
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
            (void) update_cwd_entries(full_refresh, &expl, expl.cwd.data());
            cleanup_and_close_popup();
        }

        cancel_rename:;
    }

    imgui::SameLine();

    if (imgui::Button("Cancel")) {
        cleanup_and_close_popup();
    }

    if (!err_msg.empty()) {
        imgui::Spacing();
        ImVec4 const red(1, 0.2f, 0, 1);
        imgui::TextColored(red, "Error: %s", err_msg.c_str());
    }

    if (imgui::IsWindowFocused() && imgui::IsKeyPressed(ImGuiKey_Escape)) {
        cleanup_and_close_popup();
    }

    imgui::EndPopup();
}

void render_bulk_rename_popup_modal(explorer_window &expl, wchar_t dir_sep_utf16, bool &open) noexcept
{
    namespace imgui = ImGui;

    ImVec4 red(1.0, 0, 0, 1.0);

    static char pattern_utf8[512] = "<name>.<ext>";
    static i32 counter_start = 1;
    static i32 counter_step = 1;
    static bool squish_adjacent_spaces = true;

    enum class bulk_rename_state : i32 {
        nil,
        in_progress,
        done,
        cancelled,
    };

    static std::atomic<bulk_rename_state> rename_state = bulk_rename_state::nil;
    static std::atomic<u64> num_renames_success(0);
    static std::atomic<u64> num_renames_fail(0);
    static std::atomic<u64> num_renames_total = 0;

    auto cleanup_and_close_popup = [&]() {
        open = false;
        imgui::CloseCurrentPopup();

        // pattern_utf8[0] = '\0';
        rename_state.store(bulk_rename_state::nil);
        num_renames_success.store(0);
        num_renames_fail.store(0);
        num_renames_total.store(0);
    };

    {
        f32 avail_width = imgui::GetContentRegionAvail().x;
        imgui::PushItemWidth(avail_width);

        imgui::InputTextWithHint(
            "##bulk_rename_pattern", "Rename pattern...", pattern_utf8, lengthof(pattern_utf8),
            ImGuiInputTextFlags_CallbackCharFilter, filter_chars_callback, (void *)L"\\/\"|?*"
            // don't filter <>, we use them for interpolating the pattern with name, counter, etc.
        );

        imgui::PopItemWidth();
    }

    imgui::Spacing();

    imgui::InputInt(" Counter start ", &counter_start);

    imgui::Spacing();

    imgui::InputInt(" Counter step ", &counter_step);

    imgui::Spacing();

    imgui::Checkbox("Squish adjacent spaces", &squish_adjacent_spaces);

    imgui::Spacing();
    imgui::Separator();
    imgui::Spacing();

    std::vector<bulk_rename_op> renames = {};
    {
        u64 num_pattern_errors = 0;
        i32 counter = counter_start;

        for (auto &dirent : expl.cwd_entries) {
            if (dirent.is_selected) {
                std::array<char, 1025> after = {};
                file_name_ext name_ext(dirent.basic.path.data());

                auto result = bulk_rename_transform(
                    name_ext.name, name_ext.ext,
                    after, pattern_utf8, counter,
                    dirent.basic.size, squish_adjacent_spaces
                );

                if (result.success) {
                    renames.emplace_back(&dirent.basic, path_create(after.data()));
                } else {
                    ++num_pattern_errors;
                }

                counter += counter_step;
            }
        }
    }

    std::vector<bulk_rename_collision> collisions;
    {
        // scoped_timer<timer_unit::MICROSECONDS> find_collisions_us();
        collisions = bulk_rename_find_collisions(expl.cwd_entries, renames);
    }

    bulk_rename_state state = rename_state.load();
    u64 success = num_renames_success.load();
    u64 fail = num_renames_fail.load();
    u64 total = num_renames_total.load();

    imgui::BeginDisabled(!collisions.empty() || pattern_utf8[0] == '\0' || state != bulk_rename_state::nil);
    bool rename_button_pressed = imgui::Button("Rename##bulk_perform");
    imgui::EndDisabled();

    imgui::SameLine();

    if (state == bulk_rename_state::in_progress) {
        if (imgui::Button("Abort##bulk_perform")) {
            rename_state.store(bulk_rename_state::cancelled);
        }
    }
    else {
        if (imgui::Button("Exit##bulk_rename")) {
            cleanup_and_close_popup();
        }
    }

    // imgui::BeginDisabled(state != bulk_rename_state::in_progress);
    // if (imgui::Button("Cancel##bulk_perform")) {
    //     rename_state.store(bulk_rename_state::cancelled);
    // }
    // imgui::EndDisabled();

    state = rename_state.load();

    switch (state) {
        default:
        case bulk_rename_state::nil: {
            break;
        }
        case bulk_rename_state::cancelled:
        case bulk_rename_state::in_progress: {
            f32 progress = f64(success + fail) / f64(total);
            imgui::SameLine();
            imgui::ProgressBar(progress);
            break;
        }
        case bulk_rename_state::done: {
            if (fail > 0 || ((success + fail) < total)) {
                f32 progress = f64(success + fail) / f64(total);
                imgui::SameLine();
                imgui::ProgressBar(progress);
            } else {
                cleanup_and_close_popup();
            }
            break;
        }
    }

    imgui::Spacing();
    imgui::Separator();

    bool catastrophic_failure = (state == bulk_rename_state::done) && ( (success + fail) < total );

    if (state == bulk_rename_state::cancelled) {
        imgui::TextColored(red, "Operation cancelled.");
    }
    else if (catastrophic_failure) {
        imgui::TextColored(red, "Catastrophic failure, unable to attempt all renames!");
    }
    else if (!catastrophic_failure && fail > 0) {
        imgui::TextColored(red, "%zu renames failed!", fail);
    }
    else if (!collisions.empty()) {
        // show collisions

        imgui::Spacing();

        enum collisions_table_col_id : i32 {
            collisions_table_col_problem,
            collisions_table_col_after,
            collisions_table_col_before,
            collisions_table_col_count,
        };

        // imgui::SeparatorText("Collisions");

        if (imgui::BeginTable("Collisions", collisions_table_col_count, ImGuiTableFlags_SizingStretchProp)) {
            imgui::TableSetupColumn("Problem");
            imgui::TableSetupColumn("After");
            imgui::TableSetupColumn("Before");
            imgui::TableHeadersRow();

            for (auto const &c : collisions) {
                u64 first = c.first_rename_pair_idx;
                u64 last = c.last_rename_pair_idx;

                imgui::TableNextRow();

                if (imgui::TableSetColumnIndex(collisions_table_col_problem)) {
                    imgui::TextColored(red, c.dest_dirent ? "Name taken" : "Same result");
                }
                if (imgui::TableSetColumnIndex(collisions_table_col_after)) {
                    for (u64 i = first; i < last; ++i) {
                        imgui::TextColored(get_color(renames[i].before->type), "%s\n", renames[i].after.data());
                    }
                    imgui::TextColored(get_color(renames[last].before->type), "%s", renames[last].after.data());
                }
                if (imgui::TableSetColumnIndex(collisions_table_col_before)) {
                    for (u64 i = first; i < last; ++i) {
                        imgui::TextColored(get_color(renames[i].before->type), "%s\n", renames[i].before->path.data());
                    }
                    imgui::TextColored(get_color(renames[last].before->type), "%s", renames[last].before->path.data());
                }
            }
            imgui::EndTable();
        }
    }
    else {
        // show preview

        imgui::Spacing();

        u64 preview_cnt = min(5, expl.cwd_entries.size());
        auto const &style = imgui::GetStyle();
        auto preview_cnt_rows_dimensions = ImVec2(-1, ( imgui::CalcTextSize("Y").y + (style.FramePadding.y * 2) + style.ItemSpacing.y ) * f32(preview_cnt));

        // imgui::SeparatorText("Preview");

        if (
            imgui::BeginTable(
                "bulk_rename_preview", 2,
                ImGuiTableFlags_Resizable|ImGuiTableFlags_SizingStretchProp,
                preview_cnt_rows_dimensions
            )
        ) {
            imgui::TableSetupColumn("Before");
            imgui::TableSetupColumn("After");
            imgui::TableHeadersRow();

            i32 counter = counter_start;

            // try to show preview_cnt # of previews, with simulated counter
            for (
                u64 i = 0, previews_shown = 0;
                i < expl.cwd_entries.size() && previews_shown < preview_cnt;
                ++i, counter += counter_step
            ) {
                auto &dirent = expl.cwd_entries[i];

                if (dirent.is_selected) {
                    imgui::TableNextColumn();
                    imgui::TextColored(get_color(dirent.basic.type), dirent.basic.path.data());

                    imgui::TableNextColumn();
                    file_name_ext name_ext(dirent.basic.path.data());

                    std::array<char, 1025> after;

                    auto result = bulk_rename_transform(
                        name_ext.name,
                        name_ext.ext,
                        after,
                        pattern_utf8,
                        counter,
                        dirent.basic.size,
                        squish_adjacent_spaces
                    );

                    if (result.success) {
                        imgui::TextColored(get_color(dirent.basic.type), after.data());
                    } else {
                        auto &err_msg = result.error_msg;
                        err_msg.front() = (char)toupper(err_msg.front());
                        imgui::TextColored(red, err_msg.data());
                    }

                    ++previews_shown;
                }
            }

            imgui::EndTable();
        }
    }

    if (collisions.empty() && rename_state.load() == bulk_rename_state::nil && rename_button_pressed) {
        auto bulk_rename_task = [&expl](std::vector<bulk_rename_op> rename_ops, wchar_t dir_sep_utf16) {
            rename_state.store(bulk_rename_state::in_progress);
            num_renames_total.store(rename_ops.size());

            try {
                std::wstring before_path_utf16 = {};
                std::wstring after_path_utf16 = {};

                for (u64 i = 0; i < rename_ops.size(); ++i) {
                    if (rename_state.load() == bulk_rename_state::cancelled) {
                        return;
                    }
                    if (chance(1/100.f)) {
                        ++num_renames_fail;
                        continue;
                    }
                    // if (i == 1) {
                    //     throw "test";
                    // }

                    auto const &rename = rename_ops[i];
                    wchar_t buffer_cwd_utf16[MAX_PATH] = {};
                    wchar_t buffer_before_utf16[MAX_PATH] = {};
                    wchar_t buffer_after_utf16[MAX_PATH] = {};
                    i32 utf_written = 0;

                    utf_written = utf8_to_utf16(expl.cwd.data(), buffer_cwd_utf16, lengthof(buffer_cwd_utf16));

                    if (utf_written == 0) {
                        debug_log("[%s] utf8_to_utf16 failed (expl.cwd -> buffer_cwd_utf16)", expl.name);
                        ++num_renames_fail;
                        continue;
                    }

                    assert(rename.before != nullptr);
                    utf_written = utf8_to_utf16(rename.before->path.data(), buffer_before_utf16, lengthof(buffer_before_utf16));

                    if (utf_written == 0) {
                        debug_log("[%s] utf8_to_utf16 failed (rename.before.path -> buffer_before_utf16)", expl.name);
                        ++num_renames_fail;
                        continue;
                    }

                    before_path_utf16 = buffer_cwd_utf16;
                    if (!before_path_utf16.ends_with(dir_sep_utf16)) {
                        before_path_utf16 += dir_sep_utf16;
                    }
                    before_path_utf16 += buffer_before_utf16;

                    utf_written = utf8_to_utf16(rename.after.data(), buffer_after_utf16, lengthof(buffer_after_utf16));

                    if (utf_written == 0) {
                        debug_log("[%s] utf8_to_utf16 failed (rename.after -> buffer_after_utf16)", expl.name);
                        ++num_renames_fail;
                        continue;
                    }

                    after_path_utf16 = buffer_cwd_utf16;
                    if (!after_path_utf16.ends_with(dir_sep_utf16)) {
                        after_path_utf16 += dir_sep_utf16;
                    }
                    after_path_utf16 += buffer_after_utf16;

                    std::wcout << "[" << before_path_utf16.c_str() << "] -> [" << after_path_utf16.c_str() << "]\n";
                    // result = _wrename(old_path_utf16.c_str(), new_path_utf16.c_str());

                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    ++num_renames_success;
                }
            }
            catch (...) {}

            rename_state.store(bulk_rename_state::done);
        };

        get_thread_pool().push_task(bulk_rename_task, renames, dir_sep_utf16);
    }

    if (imgui::IsWindowFocused() && imgui::IsKeyPressed(ImGuiKey_Escape) && rename_state.load() != bulk_rename_state::in_progress) {
        cleanup_and_close_popup();
    }

    imgui::EndPopup();
}

bool open_file(explorer_window::dirent const &file, explorer_window &expl, char dir_sep_utf8) noexcept
{
    swan_path_t target_full_path_utf8 = expl.cwd;

    if (!path_append(target_full_path_utf8, file.basic.path.data(), dir_sep_utf8, true)) {
        debug_log("[%s] path_append failed, cwd = [%s], append data = [\\%s]", expl.name, expl.cwd.data(), file.basic.path.data());
        return false;
    }

    wchar_t target_full_path_utf16[MAX_PATH] = {};

    i32 utf_written = utf8_to_utf16(target_full_path_utf8.data(), target_full_path_utf16, lengthof(target_full_path_utf16));

    if (utf_written == 0) {
        debug_log("[%s] utf8_to_utf16 failed: target_full_path_utf8 -> target_full_path_utf16", expl.name);
        return false;
    }

    debug_log("[%s] utf8_to_utf16 wrote %d characters (target_full_path_utf8 -> target_full_path_utf16)", expl.name, utf_written);
    debug_log("[%s] target_full_path = [%s]", expl.name, target_full_path_utf8.data());

    HINSTANCE result = ShellExecuteW(nullptr, L"open", target_full_path_utf16, nullptr, nullptr, SW_SHOWNORMAL);

    intptr_t ec = (intptr_t)result;

    if (ec > HINSTANCE_ERROR) {
        debug_log("[%s] ShellExecuteW success", expl.name);
        return true;
    }
    else if (ec == SE_ERR_NOASSOC) {
        debug_log("[%s] ShellExecuteW: SE_ERR_NOASSOC", expl.name);
        return false;
    }
    else if (ec == SE_ERR_FNF) {
        debug_log("[%s] ShellExecuteW: SE_ERR_FNF", expl.name);
        return false;
    }
    else {
        debug_log("[%s] ShellExecuteW: some sort of error", expl.name);
        return false;
    }
}

bool open_symlink(explorer_window::dirent const &dir_ent, explorer_window &expl, char dir_sep_utf8) noexcept
{
    swan_path_t symlink_self_path_utf8 = expl.cwd;
    swan_path_t symlink_target_path_utf8 = {};
    wchar_t symlink_self_path_utf16[MAX_PATH] = {};
    wchar_t symlink_target_path_utf16[MAX_PATH] = {};
    wchar_t working_dir_utf16[MAX_PATH] = {};
    wchar_t command_line_utf16[2048] = {};
    i32 show_command = SW_SHOWNORMAL;
    HRESULT com_handle = {};
    LPITEMIDLIST item_id_list = nullptr;
    i32 utf_written = {};

    if (!path_append(symlink_self_path_utf8, dir_ent.basic.path.data(), dir_sep_utf8, true)) {
        debug_log("[%s] path_append(symlink_self_path_utf8, dir_ent.basic.path.data() failed", expl.name);
        return false;
    }

    debug_log("[%s] double clicked link [%s]", expl.name, symlink_self_path_utf8);

    utf_written = utf8_to_utf16(symlink_self_path_utf8.data(), symlink_self_path_utf16, lengthof(symlink_self_path_utf16));

    if (utf_written == 0) {
        debug_log("[%s] utf8_to_utf16 failed: symlink_self_path_utf8 -> symlink_self_path_utf16", expl.name);
        return false;
    } else {
        debug_log("[%s] utf8_to_utf16 wrote %d characters", expl.name, utf_written);
        std::wcout << "symlink_self_path_utf16 = [" << symlink_self_path_utf16 << "]\n";
    }

    com_handle = s_persist_file_interface->Load(symlink_self_path_utf16, STGM_READ);

    if (com_handle != S_OK) {
        debug_log("[%s] s_persist_file_interface->Load [%s] failed: %s", expl.name, symlink_self_path_utf8, get_last_error_string().c_str());
        return false;
    } else {
        std::wcout << "s_persist_file_interface->Load [" << symlink_self_path_utf16 << "]\n";
    }

    com_handle = s_shell_link->GetIDList(&item_id_list);

    if (com_handle != S_OK) {
        debug_log("[%s] s_shell_link->GetIDList failed: %s", expl.name, get_last_error_string().c_str());
        return false;
    }

    if (!SHGetPathFromIDListW(item_id_list, symlink_target_path_utf16)) {
        debug_log("[%s] SHGetPathFromIDListW failed: %s", expl.name, get_last_error_string().c_str());
        return false;
    } else {
        std::wcout << "symlink_target_path_utf16 = [" << symlink_target_path_utf16 << "]\n";
    }

    if (com_handle != S_OK) {
        debug_log("[%s] s_shell_link->GetPath failed: %s", expl.name, get_last_error_string().c_str());
        return false;
    }

    utf_written = utf16_to_utf8(symlink_target_path_utf16, symlink_target_path_utf8.data(), symlink_target_path_utf8.size());

    if (utf_written == 0) {
        debug_log("[%s] utf16_to_utf8 failed", expl.name);
        return false;
    } else {
        debug_log("[%s] utf16_to_utf8 wrote %d characters", expl.name, utf_written);
        debug_log("[%s] symlink_target_path_utf8 = [%s]", expl.name, symlink_target_path_utf8.data());
    }

    if (directory_exists(symlink_target_path_utf8.data())) {
        // symlink to a directory, let's navigate there

        expl.cwd = symlink_target_path_utf8;
        (void) update_cwd_entries(full_refresh, &expl, expl.cwd.data());
        new_history_from(expl, expl.cwd);

        return true;
    }
    else {
        // symlink to a file, let's open it

        com_handle = s_shell_link->GetWorkingDirectory(working_dir_utf16, lengthof(working_dir_utf16));

        if (com_handle != S_OK) {
            debug_log("[%s] s_shell_link->GetWorkingDirectory failed: %s", expl.name, get_last_error_string().c_str());
            return false;
        } else {
            std::wcout << "s_shell_link->GetWorkingDirectory [" << working_dir_utf16 << "]\n";
        }

        com_handle = s_shell_link->GetArguments(command_line_utf16, lengthof(command_line_utf16));

        if (com_handle != S_OK) {
            debug_log("[%s] s_shell_link->GetArguments failed: %s", expl.name, get_last_error_string().c_str());
            return false;
        } else {
            std::wcout << "s_shell_link->GetArguments [" << command_line_utf16 << "]\n";
        }

        com_handle = s_shell_link->GetShowCmd(&show_command);

        if (com_handle != S_OK) {
            debug_log("[%s] s_shell_link->GetShowCmd failed: %s", expl.name, get_last_error_string().c_str());
            return false;
        } else {
            std::wcout << "s_shell_link->GetShowCmd [" << show_command << "]\n";
        }

        HINSTANCE result = ShellExecuteW(nullptr, L"open", symlink_target_path_utf16,
                                         command_line_utf16, working_dir_utf16, show_command);

        intptr_t err_code = (intptr_t)result;

        if (err_code > HINSTANCE_ERROR) {
            debug_log("[%s] ShellExecuteW success", expl.name);
            return true;
        }
        else if (err_code == SE_ERR_NOASSOC) {
            debug_log("[%s] ShellExecuteW error: SE_ERR_NOASSOC", expl.name);
            return false;
        }
        else if (err_code == SE_ERR_FNF) {
            debug_log("[%s] ShellExecuteW error: SE_ERR_FNF", expl.name);
            return false;
        }
        else {
            debug_log("[%s] ShellExecuteW error: unexpected error", expl.name);
            return false;
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

static
void sort_cwd_entries(explorer_window &expl, ImGuiTableSortSpecs *sort_specs)
{
    assert(sort_specs != nullptr);

    scoped_timer<timer_unit::MICROSECONDS> sort_timer(&expl.sort_us);

    using dir_ent_t = explorer_window::dirent;

    // start with a preliminary sort by path.
    // this ensures no matter the initial state, the final state is always same (deterministic).
    // necessary for avoiding unexpected movement from a refresh (especially unsightly with auto refresh).
    std::sort(expl.cwd_entries.begin(), expl.cwd_entries.end(), [](dir_ent_t const &left, dir_ent_t const &right) {
        return lstrcmpiA(left.basic.path.data(), right.basic.path.data()) < 0;
    });

    // needs to return true when left < right
    auto compare = [&](dir_ent_t const &left, dir_ent_t const &right) {
        bool left_lt_right = false;

        for (i32 i = 0; i < sort_specs->SpecsCount; ++i) {
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
                // TODO: fix incorrect sort behaviour in "C:\Code\swan\dummy"
                case cwd_entries_table_col_size_pretty:
                case cwd_entries_table_col_size_bytes: {
                    left_lt_right = (left.basic.size < right.basic.size) == direction_flipper;
                    break;
                }
                // case cwd_entries_table_col_creation_time: {
                //     i32 cmp = CompareFileTime(&left.creation_time_raw, &right.creation_time_raw);
                //     left_lt_right = (cmp <= 0) == direction_flipper;
                //     break;
                // }
                case cwd_entries_table_col_last_write_time: {
                    i32 cmp = CompareFileTime(&left.basic.last_write_time_raw, &right.basic.last_write_time_raw);
                    left_lt_right = (cmp <= 0) == direction_flipper;
                    break;
                }
            }
        }

        return left_lt_right;
    };

    std::sort(expl.cwd_entries.begin(), expl.cwd_entries.end(), compare);
}

bool update_cwd_entries(
    u8 actions,
    explorer_window *expl_ptr,
    std::string_view parent_dir,
    std::source_location sloc)
{
    debug_log("[%s] update_cwd_entries() called from [%s:%d]",
        expl_ptr->name, cget_file_name(sloc.file_name()), sloc.line());

    IM_ASSERT(expl_ptr != nullptr);

    scoped_timer<timer_unit::MICROSECONDS> function_timer(&expl_ptr->update_cwd_entries_total_us);

    bool parent_dir_exists = false;

    char dir_sep_utf8 = get_explorer_options().dir_separator_utf8();

    explorer_window &expl = *expl_ptr;
    expl.needs_sort = true;
    expl.update_cwd_entries_total_us = 0;
    expl.update_cwd_entries_searchpath_setup_us = 0;
    expl.update_cwd_entries_filesystem_us = 0;
    expl.update_cwd_entries_filter_us = 0;
    expl.update_cwd_entries_regex_ctor_us = 0;

    if (actions & query_filesystem) {
        static std::vector<swan_path_t> selected_entries = {};
        selected_entries.clear();

        for (auto const &dir_ent : expl.cwd_entries) {
            if (dir_ent.is_selected) {
                selected_entries.push_back(dir_ent.basic.path);
            }
        }

        expl.cwd_entries.clear();

        // this seems inefficient but was measured to be faster than the "efficient" way,
        // or maybe both are so fast that it doesn't matter...
        while (parent_dir.ends_with(' ')) {
            parent_dir = std::string_view(parent_dir.data(), parent_dir.size() - 1);
        }

        if (parent_dir != "") {
            wchar_t search_path_utf16[512] = {};
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
                char utf8_buffer[2048] = {};

                u64 utf_written = utf16_to_utf8(search_path_utf16,  utf8_buffer, lengthof(utf8_buffer));
                // TODO: check utf_written

                debug_log("[%s] querying filesystem, search_path = [%s]", expl.name, utf8_buffer);
            }

            scoped_timer<timer_unit::MICROSECONDS> filesystem_timer(&expl.update_cwd_entries_filesystem_us);

            WIN32_FIND_DATAW find_data;
            HANDLE find_handle = FindFirstFileW(search_path_utf16, &find_data);

            auto find_handle_cleanup_routine = make_on_scope_exit([&find_handle] { FindClose(find_handle); });

            if (find_handle == INVALID_HANDLE_VALUE) {
                debug_log("[%s] find_handle == INVALID_HANDLE_VALUE", expl.name);
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
                    // TODO: check utf_written
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

                    expl.cwd_entries.emplace_back(entry);
                }

                ++expl.num_file_finds;
                ++id;
            }
            while (FindNextFileW(find_handle, &find_data));
        }
    }

    if (actions & filter) {
        scoped_timer<timer_unit::MICROSECONDS> filter_timer(&expl.update_cwd_entries_filter_us);

        expl.filter_error.clear();

        for (auto &dir_ent : expl.cwd_entries) {
            dir_ent.is_filtered_out = false;
        }

        bool filter_is_blank = expl.filter.data()[0] == '\0';

        if (!filter_is_blank) {
            static std::regex filter_regex;

            switch (expl.filter_mode) {
                default:
                case explorer_window::filter_mode::contains: {
                    auto matcher = expl.filter_case_sensitive ? StrStrA : StrStrIA;

                    for (auto &dir_ent : expl.cwd_entries) {
                        dir_ent.is_filtered_out = !matcher(dir_ent.basic.path.data(), expl.filter.data());
                    }
                    break;
                }

                case explorer_window::filter_mode::regex: {
                    try {
                        scoped_timer<timer_unit::MICROSECONDS> regex_ctor_timer(&expl.update_cwd_entries_regex_ctor_us);
                        filter_regex = expl.filter.data();
                    }
                    catch (std::exception const &except) {
                        debug_log("[%s] error constructing std::regex, %s", expl.name, except.what());
                        expl.filter_error = except.what();
                        break;
                    }

                    auto match_flags = std::regex_constants::match_default | (
                        std::regex_constants::icase * (expl.filter_case_sensitive == 0)
                    );

                    for (auto &dir_ent : expl.cwd_entries) {
                        dir_ent.is_filtered_out = !std::regex_match(
                            dir_ent.basic.path.data(),
                            filter_regex,
                            (std::regex_constants::match_flag_type)match_flags
                        );
                    }

                    break;
                }
            }
        }
    }

exit_update_cwd_entries:
    expl.last_refresh_time = current_time();
    return parent_dir_exists;
}

bool explorer_window::save_to_disk() const noexcept
{
    scoped_timer<timer_unit::MICROSECONDS> save_to_disk_timer(&(this->save_to_disk_us));

    char file_name[32];
    snprintf(file_name, lengthof(file_name), "data/Explorer_%c.txt", this->name[9]);

    bool result = true;

    try {
        std::ofstream out(file_name);
        if (!out) {
            result = false;
        } else {
            out << "cwd " << path_length(cwd) << ' ' << cwd.data() << '\n';

            out << "filter " << strlen(filter.data()) << ' ' << filter.data() << '\n';

            out << "filter_mode " << (i32)filter_mode << '\n';

            out << "filter_case_sensitive " << (i32)filter_case_sensitive << '\n';

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
    this->latest_save_to_disk_result = (i8)result;

    return result;
}

bool explorer_window::load_from_disk(char dir_separator) noexcept
{
    assert(this->name != nullptr);

    char file_name[32];
    snprintf(file_name, lengthof(file_name), "data/Explorer_%c.txt", this->name[9]);

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

            cwd_last_frame = cwd;
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

            in >> (i32 &)filter_mode;
            debug_log("[%s] filter_mode = %d", file_name, filter_mode);
        }
        {
            in >> what;
            assert(what == "filter_case_sensitive");

            i32 val = 0;
            in >> val;

            filter_case_sensitive = (bool)val;
            debug_log("[%s] filter_case_sensitive = %d", file_name, filter_case_sensitive);
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
    path_pop_back_if(new_latest_entry_clean, '\\');
    path_pop_back_if(new_latest_entry_clean, '/');

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

// TODO: make noexcept
bool try_ascend_directory(explorer_window &expl)
{
    auto &cwd = expl.cwd;

    char dir_separator = get_explorer_options().dir_separator_utf8();

    // if there is a trailing separator, remove it
    path_pop_back_if(cwd, dir_separator);

    // remove anything between end and final separator
    while (path_pop_back_if_not(cwd, dir_separator));

    (void) update_cwd_entries(full_refresh, &expl, expl.cwd.data());

    if (!path_is_empty(expl.cwd)) {
        new_history_from(expl, expl.cwd);
    }

    expl.cwd_prev_selected_dirent_idx = explorer_window::NO_SELECTION;
    expl.filter_error.clear();

    (void) expl.save_to_disk();

    return true;
}

// TODO: make noexcept
bool try_descend_to_directory(explorer_window &expl, char const *child_dir)
{
    swan_path_t new_cwd = expl.cwd;
    char dir_sep_utf8 = get_explorer_options().dir_separator_utf8();

    if (path_append(expl.cwd, child_dir, dir_sep_utf8, true)) {
        if (PathCanonicalizeA(new_cwd.data(), expl.cwd.data())) {
            debug_log("[%s] PathCanonicalizeA success: new_cwd = [%s]", expl.name, new_cwd.data());

            (void) update_cwd_entries(full_refresh, &expl, new_cwd.data());

            new_history_from(expl, new_cwd);
            expl.cwd = new_cwd;
            expl.cwd_prev_selected_dirent_idx = explorer_window::NO_SELECTION;
            expl.filter_error.clear();

            (void) expl.save_to_disk();

            return true;
        }
        else {
            debug_log("[%s] PathCanonicalizeA failed", expl.name);
            return false;
        }
    }
    else {
        debug_log("[%s] path_append failed, new_cwd = [%s], append data = [%c%s]", expl.name, new_cwd.data(), dir_sep_utf8, child_dir);
        expl.cwd = new_cwd;
        return false;
    }
}

struct cwd_text_input_callback_user_data
{
    explorer_window *expl_ptr;
    explorer_options const *opts_ptr;
    wchar_t dir_sep_utf16;
};

i32 cwd_text_input_callback(ImGuiInputTextCallbackData *data)
{
    auto user_data = (cwd_text_input_callback_user_data *)(data->UserData);
    auto &expl = *user_data->expl_ptr;
    auto &cwd = user_data->expl_ptr->cwd;

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
        debug_log("[%s] ImGuiInputTextFlags_CallbackEdit, data->Buf = [%s], cwd = [%s]", expl.name, data->Buf, cwd);

        auto const &new_cwd = data->Buf;

        bool cwd_exists_after_edit = update_cwd_entries(full_refresh, &expl, new_cwd);

        if (cwd_exists_after_edit && !path_is_empty(expl.cwd)) {
            expl.cwd = path_create(data->Buf);

            if (path_is_empty(expl.prev_valid_cwd) || !path_loosely_same(expl.cwd, expl.prev_valid_cwd)) {
                new_history_from(expl, expl.cwd);
            }

            if (!path_loosely_same(expl.cwd, expl.prev_valid_cwd)) {
                expl.prev_valid_cwd = expl.cwd;
            }

            (void) expl.save_to_disk();
        }
    }

    return 0;
}

void render_history_browser_popup(explorer_window &expl, bool cwd_exists_before_edit, ImVec4 const &dir_color) noexcept
{
    namespace imgui = ImGui;

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
        imgui::CloseCurrentPopup();
    }
    imgui::EndDisabled();

    imgui::Spacing();
    imgui::Separator();
    imgui::Spacing();

    if (expl.wd_history.empty()) {
        imgui::TextUnformatted("(empty)");
    }

    u64 i = expl.wd_history.size() - 1;
    u64 i_inverse = 0;

    for (auto iter = expl.wd_history.rbegin(); iter != expl.wd_history.rend(); ++iter, --i, ++i_inverse) {
        swan_path_t const &hist_path = *iter;

        i32 const history_pos_max_digits = 3;
        char buffer[512];

        snprintf(buffer, lengthof(buffer), "%s %*zu  %s ",
            (i == expl.wd_history_pos ? "->" : "  "),
            history_pos_max_digits,
            i_inverse + 1,
            hist_path.data());

        imgui::PushStyleColor(ImGuiCol_Text, dir_color);
        if (imgui::Selectable(buffer, false)) {
            expl.wd_history_pos = i;
            expl.cwd = expl.wd_history[i];
            (void) update_cwd_entries(full_refresh, &expl, expl.cwd.data());
            (void) expl.save_to_disk();
        }
        imgui::PopStyleColor();
    }

    imgui::EndPopup();
}

void swan_render_window_explorer(explorer_window &expl)
{
    namespace imgui = ImGui;

    if (!imgui::Begin(expl.name)) {
        imgui::End();
        return;
    }

    ImVec4 const orange(1, 0.5f, 0, 1);
    ImVec4 const red(1, 0.2f, 0, 1);
    ImVec4 const dir_color = get_color(basic_dirent::kind::directory);
    ImVec4 const symlink_color = get_color(basic_dirent::kind::symlink);
    ImVec4 const file_color = get_color(basic_dirent::kind::file);

    auto &io = imgui::GetIO();
    bool window_focused = imgui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    explorer_options const &opts = get_explorer_options();
    bool cwd_exists_before_edit = directory_exists(expl.cwd.data());
    char dir_sep_utf8 = opts.dir_separator_utf8();
    wchar_t dir_sep_utf16 = dir_sep_utf8;
    u64 size_unit_multiplier = opts.size_unit_multiplier();

    static bool open_rename_popup = false;
    static bool open_bulk_rename_popup = false;

    bool any_popups_open = open_rename_popup || open_bulk_rename_popup;

    // if (window_focused) {
    //     save_focused_window(expl.name);
    // }

    path_force_separator(expl.cwd, dir_sep_utf8);

    // handle Enter key pressed on cwd entry
    if (window_focused && !any_popups_open && imgui::IsKeyPressed(ImGuiKey_Enter)) {
        if (explorer_window::NO_SELECTION == expl.cwd_prev_selected_dirent_idx) {
            debug_log("[%s] pressed enter but cwd_prev_selected_dirent_idx = NO_SELECTION", expl.name);
            // TODO: notify user of failure
        } else {
            auto dirent_which_enter_was_pressed_on = expl.cwd_entries[expl.cwd_prev_selected_dirent_idx];
            debug_log("[%s] pressed enter on [%s]", expl.name, dirent_which_enter_was_pressed_on.basic.path.data());
            if (dirent_which_enter_was_pressed_on.basic.is_directory()) {
                try_descend_to_directory(expl, dirent_which_enter_was_pressed_on.basic.path.data());
            }
        }
    }

    static explorer_window::dirent const *dirent_to_be_renamed = nullptr;

    // handle F2 key pressed on cwd entry
    if (window_focused && !any_popups_open && imgui::IsKeyPressed(ImGuiKey_F2)) {
        if (explorer_window::NO_SELECTION == expl.cwd_prev_selected_dirent_idx) {
            debug_log("[%s] pressed F2 but cwd_prev_selected_dirent_idx = NO_SELECTION", expl.name);
            // TODO: notify user of failure
        } else {
            auto dirent_which_f2_was_pressed_on = expl.cwd_entries[expl.cwd_prev_selected_dirent_idx];
            debug_log("[%s] pressed F2 on [%s]", expl.name, dirent_which_f2_was_pressed_on.basic.path.data());
            open_rename_popup = true;
            dirent_to_be_renamed = &dirent_which_f2_was_pressed_on;
        }
    }

    // debug info start
    if (opts.show_debug_info) {
        auto calc_perc_total_time = [&expl](f64 time) {
            return time == 0.f
                ? 0.f
                : ( (time / expl.update_cwd_entries_total_us) * 100.f );
        };

        imgui::Text("prev_valid_cwd = [%s]", expl.prev_valid_cwd.data());

        if (imgui::BeginTable("timers", 3, ImGuiTableFlags_BordersInnerV|ImGuiTableFlags_Resizable)) {
            imgui::TableNextColumn();
            imgui::SeparatorText("misc. state");
            imgui::Text("num_file_finds");
            imgui::Text("cwd_prev_selected_dirent_idx");
            imgui::Text("num_selected_cwd_entries");
            imgui::Text("latest_save_to_disk_result");

            imgui::TableNextColumn();
            imgui::SeparatorText("");
            imgui::Text("%zu", expl.num_file_finds);
            imgui::Text("%lld", expl.cwd_prev_selected_dirent_idx);
            imgui::Text("%zu", expl.num_selected_cwd_entries);
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
    }
    // debug info end

    imgui::Spacing();
    imgui::Separator();
    imgui::Spacing();

    imgui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(0, 10.f));
    if (imgui::BeginTable("first_3_control_rows", 1, ImGuiTableFlags_SizingFixedFit)) {

        imgui::TableNextColumn();

        // refresh button, ctrl-r refresh logic, automatic refreshing
        {
            bool refreshed = false; // to avoid refreshing twice in one frame

            auto refresh = [&](std::source_location sloc = std::source_location::current()) {
                if (!refreshed) {
                    (void) update_cwd_entries(full_refresh, &expl, expl.cwd.data(), sloc);
                    refreshed = true;
                }
            };

            if (
                opts.ref_mode == explorer_options::refresh_mode::manual ||
                (opts.ref_mode == explorer_options::refresh_mode::adaptive && expl.cwd_entries.size() > opts.adaptive_refresh_threshold)
            ) {
                if (imgui::Button("Refresh") && !refreshed) {
                    debug_log("[%s] refresh button pressed", expl.name);
                    refresh();
                }

                imgui_sameline_spacing(2);
            }
            else {
                if (!refreshed && cwd_exists_before_edit) {
                    // see if it's time for an automatic refresh

                    time_point_t now = current_time();

                    i64 diff_ms = compute_diff_ms(expl.last_refresh_time, now);

                    auto min_refresh_itv_ms = explorer_options::min_tolerable_refresh_interval_ms;

                    if (diff_ms >= max(min_refresh_itv_ms, opts.auto_refresh_interval_ms)) {
                        debug_log("[%s] auto refresh, diff_ms = %lld", expl.name, diff_ms);
                        refresh();
                    }
                }
            }

            if (window_focused && io.KeyCtrl && imgui::IsKeyPressed(ImGuiKey_R)) {
                debug_log("[%s] ctrl-r, refresh triggered", expl.name);
                refresh();
            }
        }
        // end of refresh button, ctrl-r refresh logic, automatic refreshing

        // pin cwd button start
        {
            u64 pin_idx;
            {
                scoped_timer<timer_unit::MICROSECONDS> check_if_pinned_timer(&expl.check_if_pinned_us);
                pin_idx = find_pin_idx(expl.cwd);
            }
            bool already_pinned = pin_idx != std::string::npos;

            char buffer[4];
            snprintf(buffer, lengthof(buffer), "[%c]", (already_pinned ? '*' : ' '));

            imgui::BeginDisabled(!cwd_exists_before_edit && !already_pinned);

            if (imgui::Button(buffer)) {
                if (already_pinned) {
                    debug_log("[%s] pin_idx = %zu", expl.name, pin_idx);
                    scoped_timer<timer_unit::MICROSECONDS> unpin_timer(&expl.unpin_us);
                    unpin(pin_idx);
                }
                else {
                    (void) pin(expl.cwd, dir_sep_utf8);
                }
                bool result = save_pins_to_disk();
                debug_log("save_pins_to_disk result = %d", result);
            }
            if (imgui::IsItemHovered()) {
                imgui::SetTooltip(" Click here to %s the current working directory. ",
                    already_pinned ? "unpin" : "pin");
            }

            imgui::EndDisabled();
        }
        // pin cwd button end

        imgui_sameline_spacing(2);

        // up a directory arrow start
        {
            imgui::BeginDisabled(!cwd_exists_before_edit);

            if (imgui::ArrowButton("Up", ImGuiDir_Up)) {
                debug_log("[%s] up arrow button triggered", expl.name);
                try_ascend_directory(expl);
            }

            imgui::EndDisabled();
        }
        // up a directory arrow end

        imgui::SameLine();

        // history back (left) arrow start
        {
            imgui::BeginDisabled(expl.wd_history_pos == 0);

            if (imgui::ArrowButton("Back", ImGuiDir_Left)) {
                debug_log("[%s] back arrow button triggered", expl.name);

                if (io.KeyShift || io.KeyCtrl) {
                    expl.wd_history_pos = 0;
                } else {
                    expl.wd_history_pos -= 1;
                }

                expl.cwd = expl.wd_history[expl.wd_history_pos];
                (void) update_cwd_entries(full_refresh, &expl, expl.cwd.data());
            }

            imgui::EndDisabled();
        }
        // history back (left) arrow end

        imgui::SameLine();

        // history forward (right) arrow
        {
            u64 wd_history_last_idx = expl.wd_history.empty() ? 0 : expl.wd_history.size() - 1;

            imgui::BeginDisabled(expl.wd_history_pos == wd_history_last_idx);

            if (imgui::ArrowButton("Forward", ImGuiDir_Right)) {
                debug_log("[%s] forward arrow button triggered", expl.name);

                if (io.KeyShift || io.KeyCtrl) {
                    expl.wd_history_pos = wd_history_last_idx;
                } else {
                    expl.wd_history_pos += 1;
                }

                expl.cwd = expl.wd_history[expl.wd_history_pos];
                (void) update_cwd_entries(full_refresh, &expl, expl.cwd.data());
            }

            imgui::EndDisabled();
        }
        // history forward (right) arrow end

        imgui::SameLine();

        // history browser start
        if (imgui::Button("History")) {
            imgui::OpenPopup("history_popup");
        }
        if (imgui::BeginPopup("history_popup")) {
            render_history_browser_popup(expl, cwd_exists_before_edit, dir_color);
        }
        // history browser end

        imgui_sameline_spacing(2);

        // filter type start
        {
            // important that they are all the same length,
            // this assumption is leveraged for calculation of combo box width
            static char const *filter_modes[] = {
                "Contains",
                "RegExp  ",
            };

            static_assert(lengthof(filter_modes) == (u64)explorer_window::filter_mode::count);

            ImVec2 max_dropdown_elem_size = imgui::CalcTextSize(filter_modes[0]);

            imgui::PushItemWidth(max_dropdown_elem_size.x + 30.f); // some extra for the dropdown button
            imgui::Combo("##filter_mode", (i32 *)(&expl.filter_mode), filter_modes, lengthof(filter_modes));
            imgui::PopItemWidth();
        }
        // filter type end

        imgui::SameLine();

        // filter case sensitivity button start
        {
            if (imgui::Button(expl.filter_case_sensitive ? "s" : "i")) {
                flip_bool(expl.filter_case_sensitive);
                (void) update_cwd_entries(filter, &expl, expl.cwd.data());
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
        // filter case sensitivity button start

        imgui::SameLine();

        // filter text input start
        {
            imgui::PushItemWidth(max(
                imgui::CalcTextSize(expl.filter.data()).x + (imgui::GetStyle().FramePadding.x * 2) + 10.f,
                imgui::CalcTextSize("123456789012345").x
            ));

            if (imgui::InputTextWithHint("##filter", "Filter", expl.filter.data(), expl.filter.size())) {
                (void) update_cwd_entries(filter, &expl, expl.cwd.data());
                (void) expl.save_to_disk();
            }

            imgui::PopItemWidth();
        }
        // filter text input end

        imgui::TableNextColumn();

        if (imgui::Button("+dir")) {
            imgui::OpenPopup("Create directory");
        }
        if (imgui::BeginPopupModal("Create directory", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
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
                ImGuiInputTextFlags_CallbackCharFilter, filter_chars_callback, (void *)s_illegal_filename_chars)
            ) {
                err_msg.clear();
            }

            imgui::Spacing();

            if (imgui::Button("Create") && dir_name_utf8[0] != '\0') {
                std::wstring create_path = {};
                wchar_t cwd_utf16[MAX_PATH] = {};
                wchar_t dir_name_utf16[MAX_PATH] = {};
                i32 utf_written = 0;
                BOOL result = {};

                utf_written = utf8_to_utf16(expl.cwd.data(), cwd_utf16, lengthof(cwd_utf16));
                if (utf_written == 0) {
                    debug_log("[%s] utf8_to_utf16 failed: expl.cwd -> cwd_utf16", expl.name);
                    cleanup_and_close_popup();
                    goto end_create_dir;
                }

                utf_written = utf8_to_utf16(dir_name_utf8, dir_name_utf16, lengthof(dir_name_utf16));
                if (utf_written == 0) {
                    debug_log("[%s] utf8_to_utf16 failed: dir_name_utf8 -> dir_name_utf16", expl.name);
                    cleanup_and_close_popup();
                    goto end_create_dir;
                }

                create_path.reserve(1024);

                create_path = cwd_utf16;
                if (!create_path.ends_with(dir_sep_utf16)) {
                    create_path += dir_sep_utf16;
                }
                create_path += dir_name_utf16;

                std::wcout << "CreateDirectoryW [" << create_path << "]\n";
                result = CreateDirectoryW(create_path.c_str(), nullptr);

                if (result == 0) {
                    auto error = GetLastError();
                    switch (error) {
                        case ERROR_ALREADY_EXISTS: err_msg = "Directory already exists."; break;
                        case ERROR_PATH_NOT_FOUND: err_msg = "One or more intermediate directories do not exist; probably a bug. Sorry!"; break;
                        default: err_msg = get_last_error_string(); break;
                    }
                    debug_log("[%s] CreateDirectoryW failed: %d, %s", expl.name, result, err_msg.c_str());
                } else {
                    cleanup_and_close_popup();
                    update_cwd_entries(full_refresh, &expl, expl.cwd.data());
                }

                end_create_dir:;
            }

            imgui::SameLine();

            if (imgui::Button("Cancel")) {
                cleanup_and_close_popup();
            }

            if (!err_msg.empty()) {
                imgui::Spacing();
                imgui::TextColored(red, "Error: %s", err_msg.c_str());
            }

            if (imgui::IsWindowFocused() && imgui::IsKeyPressed(ImGuiKey_Escape)) {
                cleanup_and_close_popup();
            }

            imgui::EndPopup();
        }

        imgui::SameLine();

        if (imgui::Button("+file")) {
            imgui::OpenPopup("Create file");
        }
        if (imgui::BeginPopupModal("Create file", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
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
                ImGuiInputTextFlags_CallbackCharFilter, filter_chars_callback, (void *)s_illegal_filename_chars)
            ) {
                err_msg.clear();
            }

            imgui::Spacing();

            if (imgui::Button("Create") && file_name_utf8[0] != '\0') {
                std::wstring create_path = {};
                wchar_t cwd_utf16[MAX_PATH] = {};
                wchar_t file_name_utf16[MAX_PATH] = {};
                i32 utf_written = 0;
                HANDLE result = {};

                utf_written = utf8_to_utf16(expl.cwd.data(), cwd_utf16, lengthof(cwd_utf16));
                if (utf_written == 0) {
                    debug_log("[%s] utf8_to_utf16 failed: expl.cwd -> cwd_utf16", expl.name);
                    cleanup_and_close_popup();
                    goto end_create_file;
                }

                utf_written = utf8_to_utf16(file_name_utf8, file_name_utf16, lengthof(file_name_utf16));
                if (utf_written == 0) {
                    debug_log("[%s] utf8_to_utf16 failed: file_name_utf8 -> file_name_utf16", expl.name);
                    cleanup_and_close_popup();
                    goto end_create_file;
                }

                create_path.reserve(1024);

                create_path = cwd_utf16;
                if (!create_path.ends_with(dir_sep_utf16)) {
                    create_path += dir_sep_utf16;
                }
                create_path += file_name_utf16;

                std::wcout << "CreateFileW [" << create_path << "]\n";
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
                    debug_log("[%s] CreateFileW failed: %d, %s", expl.name, result, err_msg.c_str());
                } else {
                    cleanup_and_close_popup();
                    update_cwd_entries(full_refresh, &expl, expl.cwd.data());
                }

                end_create_file:;
            }

            imgui::SameLine();

            if (imgui::Button("Cancel")) {
                cleanup_and_close_popup();
            }

            if (!err_msg.empty()) {
                imgui::Spacing();
                imgui::TextColored(red, "Error: %s", err_msg.c_str());
            }

            if (imgui::IsWindowFocused() && imgui::IsKeyPressed(ImGuiKey_Escape)) {
                cleanup_and_close_popup();
            }

            imgui::EndPopup();
        }

        imgui_sameline_spacing(1);

        imgui::Text("Bulk ops:");
        imgui::SameLine();

        imgui::BeginDisabled(expl.num_selected_cwd_entries == 0);
        if (imgui::Button("Rename##bulk_open")) {
            open_bulk_rename_popup = true;
        }
        imgui::EndDisabled();

        imgui::SameLine();

        imgui::BeginDisabled(expl.num_selected_cwd_entries == 0);
        if (imgui::Button("Cut")) {
            s_paste_payload.window_name = expl.name;
            s_paste_payload.items.clear();
            s_paste_payload.keep_src = false;

            for (auto const &dir_ent : expl.cwd_entries) {
                if (dir_ent.is_selected && !dir_ent.basic.is_dotdot()) {
                    swan_path_t src = expl.cwd;
                    if (path_append(src, dir_ent.basic.path.data(), dir_sep_utf8, true)) {
                        s_paste_payload.items.push_back({ dir_ent.basic.size, dir_ent.basic.type, src });
                        s_paste_payload.bytes += dir_ent.basic.size;
                        if (dir_ent.basic.is_directory()) {
                            s_paste_payload.has_directories = true;
                            // TODO: include size of children in s_paste_payload.bytes
                        }
                    } else {
                        // TODO: handle error
                    }
                }
            }
        }
        imgui::EndDisabled();

        imgui::SameLine();

        imgui::BeginDisabled(expl.num_selected_cwd_entries == 0);
        if (imgui::Button("Copy")) {
            s_paste_payload.window_name = expl.name;
            s_paste_payload.items.clear();
            s_paste_payload.keep_src = true;

            for (auto const &dir_ent : expl.cwd_entries) {
                if (dir_ent.is_selected && !dir_ent.basic.is_dotdot()) {
                    swan_path_t src = expl.cwd;
                    if (path_append(src, dir_ent.basic.path.data(), dir_sep_utf8, true)) {
                        s_paste_payload.items.push_back({ dir_ent.basic.size, dir_ent.basic.type, src });
                        s_paste_payload.bytes += dir_ent.basic.size;
                        if (dir_ent.basic.is_directory()) {
                            s_paste_payload.has_directories = true;
                            // TODO: include size of children in s_paste_payload.bytes
                        }
                    } else {
                        // TODO: handle error
                    }
                }
            }
        }
        imgui::EndDisabled();

        imgui::SameLine();

        imgui::BeginDisabled(expl.num_selected_cwd_entries == 0);
        if (imgui::Button("Delete")) {
            // TODO: setup IFileOperation

            for (auto const &dir_ent : expl.cwd_entries) {
                if (!dir_ent.is_selected || dir_ent.basic.is_dotdot()) {
                    continue;
                }

                if (dir_ent.basic.is_directory()) {
                    // delete directory
                }
                else {
                    // delete file
                }
            }

            // TODO: cleanup IFIleOperation
        }
        imgui::EndDisabled();

        imgui::SameLine();

        // paste payload description start
        if (!s_paste_payload.items.empty()) {
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
                    char parent_path_utf8[2048] = {};
                    strncat(parent_path_utf8, parent_path_view.data(), parent_path_view.size());
                    imgui::TextColored(dir_color, parent_path_utf8);
                }

                imgui::Spacing();
                imgui::Separator();
                imgui::Spacing();

                {
                    char pretty_size[32] = {};
                    format_file_size(s_paste_payload.bytes, pretty_size, lengthof(pretty_size), size_unit_multiplier);
                    if (s_paste_payload.has_directories) {
                        imgui::Text("> %s", pretty_size);
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
                        case basic_dirent::kind::directory: color = dir_color;     break;
                        case basic_dirent::kind::symlink:   color = symlink_color; break;
                        case basic_dirent::kind::file:      color = file_color;    break;
                        default:
                            ImVec4 white(1.0, 1.0, 1.0, 1.0);
                            color = white;
                            break;
                    }
                    imgui::TextColored(color, cget_file_name(item.path.data()));
                }

                imgui::EndTooltip();
            }

            imgui_sameline_spacing(1);

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

                // TODO: cleanup IFIleOperation
            }

            imgui::SameLine();

            if (imgui::Button("X##Cancel")) {
                s_paste_payload.items.clear();
            }
        }
        // paste payload description end

        imgui::TableNextColumn();

        // cwd text input start
        {
            cwd_text_input_callback_user_data user_data;
            user_data.expl_ptr = &expl;
            user_data.opts_ptr = &opts;
            user_data.dir_sep_utf16 = dir_sep_utf16;

            imgui::PushItemWidth(
                max(imgui::CalcTextSize(expl.cwd.data()).x + (imgui::GetStyle().FramePadding.x * 2),
                    imgui::CalcTextSize("123456789_123456789_").x)
                + 60.f
            );

            imgui::InputText("##cwd", expl.cwd.data(), expl.cwd.size(),
                ImGuiInputTextFlags_CallbackCharFilter|ImGuiInputTextFlags_CallbackEdit,
                cwd_text_input_callback, (void *)&user_data);

            expl.cwd = path_squish_adjacent_separators(expl.cwd);

            imgui::PopItemWidth();

            imgui::SameLine();

            // label
            if (opts.show_cwd_len) {
                imgui::Text("cwd(%3d)", path_length(expl.cwd));
            }
        }
        // cwd text input end

        // clicknav start
        if (cwd_exists_before_edit && !path_is_empty(expl.cwd)) {
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
                    debug_log("[%s] cd_to_slice: slice == cwd, not updating cwd|history", expl.name);
                }
                else {
                    expl.cwd[len] = '\0';
                    new_history_from(expl, expl.cwd);
                }
            };

            f32 original_spacing = imgui::GetStyle().ItemSpacing.x;

            for (auto slice_it = slices.begin(); slice_it != slices.end() - 1; ++slice_it) {
                if (imgui::Button(*slice_it)) {
                    debug_log("[%s] clicked slice [%s]", expl.name, *slice_it);
                    cd_to_slice(*slice_it);
                    (void) update_cwd_entries(full_refresh, &expl, expl.cwd.data());
                    (void) expl.save_to_disk();
                }
                imgui::GetStyle().ItemSpacing.x = 2;
                imgui::SameLine();
                imgui::Text("%c", dir_sep_utf8);
                imgui::SameLine();
            }

            if (imgui::Button(slices.back())) {
                debug_log("[%s] clicked slice [%s]", expl.name, slices.back());
                cd_to_slice(slices.back());
                (void) update_cwd_entries(full_refresh, &expl, expl.cwd.data());
            }

            if (slices.size() > 1) {
                imgui::GetStyle().ItemSpacing.x = original_spacing;
            }
        }
        // clicknav end
    }
    imgui::EndTable();
    imgui::PopStyleVar();

    imgui::Spacing();
    imgui::Spacing();
    imgui::Spacing();

    // cwd entries stats & table start

    if (path_is_empty(expl.cwd)) {
        static time_point_t last_refresh_time = {};
        static drive_list_t drives = {};

        // refresh drives once per second
        {
            time_point_t now = current_time();
            i64 diff_ms = compute_diff_ms(last_refresh_time, now);
            if (diff_ms >= 1000) {
                drives = query_drive_list();
                debug_log("[%s] refresh drives, diff = %lld ms", expl.name, diff_ms);
                last_refresh_time = current_time();
            }
        }

        enum drive_table_col_id : i32
        {
            drive_table_col_id_letter,
            drive_table_col_id_name,
            drive_table_col_id_filesystem,
            drive_table_col_id_free_space,
            drive_table_col_id_used_percent,
            drive_table_col_id_total_space,
            drive_table_col_id_count,
        };

        if (imgui::BeginTable("drives", drive_table_col_id_count)) {
            imgui::TableSetupColumn("Drive", ImGuiTableColumnFlags_NoSort, 0.0f, drive_table_col_id_letter);
            imgui::TableSetupColumn("Name", ImGuiTableColumnFlags_NoSort, 0.0f, drive_table_col_id_name);
            imgui::TableSetupColumn("Filesystem", ImGuiTableColumnFlags_NoSort, 0.0f, drive_table_col_id_filesystem);
            imgui::TableSetupColumn("Free Space", ImGuiTableColumnFlags_NoSort, 0.0f, drive_table_col_id_free_space);
            imgui::TableSetupColumn("Used", ImGuiTableColumnFlags_NoSort, 0.0f, drive_table_col_id_used_percent);
            imgui::TableSetupColumn("Total Space", ImGuiTableColumnFlags_NoSort, 0.0f, drive_table_col_id_total_space);
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
                        update_cwd_entries(full_refresh, &expl, expl.cwd.data());
                        new_history_from(expl, expl.cwd);
                    }

                    selected = false;
                }

                if (imgui::TableSetColumnIndex(drive_table_col_id_filesystem)) {
                    imgui::TextUnformatted(drive.filesystem_name_utf8);
                }

                if (imgui::TableSetColumnIndex(drive_table_col_id_free_space)) {
                    std::array<char, 32> formatted = {};
                    format_file_size(drive.available_bytes, formatted.data(), formatted.size(), size_unit_multiplier);
                    imgui::Text("%s", formatted.data());
                }

                if (imgui::TableSetColumnIndex(drive_table_col_id_used_percent)) {
                    u64 used_bytes = drive.total_bytes - drive.available_bytes;
                    f64 percent_used = ( f64(used_bytes) / f64(drive.total_bytes) ) * 100.0;
                    imgui::Text("%.1lf %%", percent_used);
                }

                if (imgui::TableSetColumnIndex(drive_table_col_id_total_space)) {
                    std::array<char, 32> formatted = {};
                    format_file_size(drive.total_bytes, formatted.data(), formatted.size(), size_unit_multiplier);
                    imgui::Text("%s", formatted.data());
                }
            }

            imgui::EndTable();
        }
    }
    else if (!cwd_exists_before_edit) {
        imgui::TextColored(orange, "Invalid directory.");
    }
    else if (expl.cwd_entries.empty()) {
        // cwd exists but is empty
        imgui::TextColored(orange, "Empty directory.");
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

        for (auto const &dir_ent : expl.cwd_entries) {
            static_assert(false == 0);
            static_assert(true == 1);

            bool is_dotdot = dir_ent.basic.is_dotdot();

            num_selected_directories += u64(dir_ent.is_selected && dir_ent.basic.is_directory() && !is_dotdot);
            num_selected_symlinks    += u64(dir_ent.is_selected && dir_ent.basic.is_symlink());
            num_selected_files       += u64(dir_ent.is_selected && dir_ent.basic.is_non_symlink_file());

            num_filtered_directories += u64(dir_ent.is_filtered_out && dir_ent.basic.is_directory() && !is_dotdot);
            num_filtered_symlinks    += u64(dir_ent.is_filtered_out && dir_ent.basic.is_symlink());
            num_filtered_files       += u64(dir_ent.is_filtered_out && dir_ent.basic.is_non_symlink_file());

            num_child_dirents     += u64(!is_dotdot);
            num_child_directories += u64(dir_ent.basic.is_directory() && !is_dotdot);
            num_child_symlinks    += u64(dir_ent.basic.is_symlink());
            num_child_files       += u64(dir_ent.basic.is_non_symlink_file());
        }

        u64 num_filtered_dirents = num_filtered_directories + num_filtered_symlinks + num_filtered_files;
        u64 num_selected_dirents = num_selected_directories + num_selected_symlinks + num_selected_files;

        if (expl.filter_error != "") {
            imgui::PushTextWrapPos(imgui::GetColumnWidth());
            imgui::TextColored(red, "%s", expl.filter_error.c_str());
            imgui::PopTextWrapPos();

            imgui::Spacing();
            imgui::Spacing();
            imgui::Spacing();
        }

        imgui::Text("items(%zu)", num_child_dirents);
        if (imgui::IsItemHovered() && imgui::BeginTooltip()) {
            imgui::SeparatorText("Items");

            if (num_child_directories > 0) {
                imgui::TextColored(dir_color, "%zu director%s", num_child_directories, num_child_directories == 1 ? "y" : "ies");
            }
            if (num_child_symlinks > 0) {
                imgui::TextColored(symlink_color, "%zu symlink%s", num_child_symlinks, num_child_symlinks == 1 ? "" : "s");
            }
            if (num_child_files > 0) {
                imgui::TextColored(file_color, "%zu file%s", num_child_files, num_child_files == 1 ? "" : "s");
            }

            imgui::EndTooltip();
        }

        if (expl.filter_error == "" && num_filtered_dirents > 0) {
            imgui_sameline_spacing(1);

            imgui::Text("filtered(%zu)", num_filtered_dirents);

            if (imgui::IsItemHovered() && imgui::BeginTooltip()) {
                imgui::SeparatorText("Filtered");

                if (num_filtered_directories > 0) {
                    imgui::TextColored(dir_color, "%zu director%s", num_filtered_directories, num_filtered_directories == 1 ? "y" : "ies");
                }
                if (num_filtered_symlinks > 0) {
                    imgui::TextColored(symlink_color, "%zu symlink%s", num_filtered_symlinks, num_filtered_symlinks == 1 ? "" : "s");
                }
                if (num_filtered_files > 0) {
                    imgui::TextColored(file_color, "%zu file%s", num_filtered_files, num_filtered_files == 1 ? "" : "s");
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
                    imgui::TextColored(dir_color, "%zu director%s", num_selected_directories, num_selected_directories == 1 ? "y" : "ies");
                }
                if (num_selected_symlinks > 0) {
                    imgui::TextColored(symlink_color, "%zu symlink%s", num_selected_symlinks, num_selected_symlinks == 1 ? "" : "s");
                }
                if (num_selected_files > 0) {
                    imgui::TextColored(file_color, "%zu file%s", num_selected_files, num_selected_files == 1 ? "" : "s");
                }

                imgui::EndTooltip();
            }
        }

        imgui::Spacing();
        imgui::Spacing();

        expl.num_selected_cwd_entries = 0; // will get computed as we render cwd_entries table

        if (imgui::BeginChild("cwd_entries_child", ImVec2(0, imgui::GetContentRegionAvail().y))) {
            if (num_filtered_dirents == expl.cwd_entries.size()) {
                if (imgui::Button("Clear filter")) {
                    debug_log("[%s] clear filter button pressed", expl.name);
                    expl.filter[0] = '\0';
                    (void) update_cwd_entries(filter, &expl, expl.cwd.data());
                    (void) expl.save_to_disk();
                }

                imgui_sameline_spacing(1);

                imgui::TextColored(orange, "All items filtered.");
            }
            else if (imgui::BeginTable("cwd_entries", cwd_entries_table_col_count,
                ImGuiTableFlags_SizingStretchProp|ImGuiTableFlags_Hideable|ImGuiTableFlags_Resizable|ImGuiTableFlags_Reorderable|ImGuiTableFlags_Sortable
                // ImVec2(-1, imgui::GetContentRegionAvail().y)
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

                ImGuiTableSortSpecs *sort_specs = imgui::TableGetSortSpecs();
                if (sort_specs != nullptr && (expl.needs_sort || sort_specs->SpecsDirty)) {
                    sort_cwd_entries(expl, sort_specs);
                    sort_specs->SpecsDirty = false;
                    expl.needs_sort = false;
                }

                static explorer_window::dirent const *right_clicked_ent = nullptr;

                for (u64 i = 0; i < expl.cwd_entries.size(); ++i) {
                    auto &dir_ent = expl.cwd_entries[i];

                    if (dir_ent.is_filtered_out) {
                        ++num_filtered_dirents;
                        continue;
                    }

                    imgui::TableNextRow();

                    if (imgui::TableSetColumnIndex(cwd_entries_table_col_number)) {
                        imgui::Text("%zu", i + 1);
                    }

                    if (imgui::TableSetColumnIndex(cwd_entries_table_col_id)) {
                        imgui::Text("%zu", dir_ent.basic.id);
                    }

                    if (imgui::TableSetColumnIndex(cwd_entries_table_col_path)) {
                        imgui::PushStyleColor(ImGuiCol_Text, get_color(dir_ent.basic.type));

                        // TODO: fix
                        if (imgui::Selectable(dir_ent.basic.path.data(), dir_ent.is_selected, ImGuiSelectableFlags_SpanAllColumns)) {
                            debug_log("[%s] selected [%s]", expl.name, dir_ent.basic.path.data());

                            bool set_selection_data = true;

                            if (!io.KeyCtrl && !io.KeyShift) {
                                // entry was selected but Ctrl was not held, so deselect everything
                                for (auto &dir_ent2 : expl.cwd_entries)
                                    dir_ent2.is_selected = false;
                            }

                            flip_bool(dir_ent.is_selected);

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

                                debug_log("[%s] shift click, [%zu, %zu]", expl.name, first_idx, last_idx);

                                for (u64 j = first_idx; j <= last_idx; ++j) {
                                    if (!expl.cwd_entries[j].basic.is_dotdot()) {
                                        expl.cwd_entries[j].is_selected = true;
                                    }
                                }
                            }

                            static f64 last_click_time = 0;
                            static swan_path_t last_click_path = {};
                            swan_path_t const &current_click_path = dir_ent.basic.path;
                            f64 const double_click_window_sec = 0.3;
                            f64 current_time = imgui::GetTime();
                            f64 seconds_between_clicks = current_time - last_click_time;

                            if (seconds_between_clicks <= double_click_window_sec && path_equals_exactly(current_click_path, last_click_path)) {
                                if (dir_ent.basic.is_directory()) {
                                    debug_log("[%s] double clicked directory [%s]", expl.name, dir_ent.basic.path.data());

                                    bool cd_success = false;

                                    if (dir_ent.basic.is_dotdot()) {
                                        cd_success = try_ascend_directory(expl);
                                    } else {
                                        cd_success = try_descend_to_directory(expl, dir_ent.basic.path.data());
                                    }

                                    if (cd_success) {
                                        set_selection_data = false;
                                        // don't set selection data, because we just navigated to a different directory
                                        // which cleared the selection state - we need to maintain this cleared state
                                    }
                                }
                                else if (dir_ent.basic.is_symlink()) {
                                    debug_log("[%s] double clicked symlink [%s]", expl.name, dir_ent.basic.path.data());

                                    open_symlink(dir_ent, expl, dir_sep_utf8);
                                    // TODO: handle open failure
                                }
                                else {
                                    debug_log("[%s] double clicked file [%s]", expl.name, dir_ent.basic.path.data());

                                    open_file(dir_ent, expl, dir_sep_utf8);
                                    // TODO: handle open failure
                                }
                            }
                            else if (dir_ent.basic.is_dotdot()) {
                                debug_log("[%s] selected [%s]", expl.name, dir_ent.basic.path.data());
                            }

                            if (set_selection_data) {
                                last_click_time = current_time;
                                last_click_path = current_click_path;
                                expl.cwd_prev_selected_dirent_idx = i;
                            }

                        } // imgui::Selectable

                        if (dir_ent.basic.is_dotdot()) {
                            dir_ent.is_selected = false; // do no allow .. to be selected
                        }

                        if (imgui::IsItemClicked(ImGuiMouseButton_Right) && !dir_ent.basic.is_dotdot()) {
                            debug_log("[%s] right clicked [%s]", expl.name, dir_ent.basic.path.data());
                            imgui::OpenPopup("Context");
                            right_clicked_ent = &dir_ent;
                        }

                        imgui::PopStyleColor();

                    } // path col

                    if (imgui::TableSetColumnIndex(cwd_entries_table_col_type)) {
                        if (dir_ent.basic.is_directory()) {
                            imgui::TextUnformatted("dir");
                        }
                        else if (dir_ent.basic.is_symlink()) {
                            imgui::TextUnformatted("link");
                        }
                        else {
                            imgui::TextUnformatted("file");
                        }
                    }

                    if (imgui::TableSetColumnIndex(cwd_entries_table_col_size_pretty)) {
                        if (dir_ent.basic.is_directory()) {
                            imgui::Text("");
                        }
                        else {
                            std::array<char, 32> pretty_size = {};
                            format_file_size(dir_ent.basic.size, pretty_size.data(), pretty_size.size(), size_unit_multiplier);
                            imgui::TextUnformatted(pretty_size.data());
                        }
                    }

                    if (imgui::TableSetColumnIndex(cwd_entries_table_col_size_bytes)) {
                        if (dir_ent.basic.is_directory()) {
                            imgui::TextUnformatted("");
                        }
                        else {
                            imgui::Text("%zu", dir_ent.basic.size);
                        }
                    }

                    // if (imgui::TableSetColumnIndex(cwd_entries_table_col_creation_time)) {
                    //     auto [result, buffer] = filetime_to_string(&dir_ent.last_write_time_raw);
                    //     imgui::TextUnformatted(buffer.data());
                    // }

                    if (imgui::TableSetColumnIndex(cwd_entries_table_col_last_write_time)) {
                        auto [result, buffer] = filetime_to_string(&dir_ent.basic.last_write_time_raw);
                        imgui::TextUnformatted(buffer.data());
                    }

                    expl.num_selected_cwd_entries += u64(dir_ent.is_selected);

                } // cwd_entries loop

                if (imgui::BeginPopup("Context")) {
                    assert(right_clicked_ent != nullptr);
                    imgui::PushStyleColor(ImGuiCol_Text, get_color(right_clicked_ent->basic.type));
                    imgui::SeparatorText(right_clicked_ent->basic.path.data());
                    imgui::PopStyleColor();

                    // bool is_directory = right_clicked_ent->basic.is_directory();

                    if (imgui::Selectable("Copy name")) {
                        imgui::SetClipboardText(right_clicked_ent->basic.path.data());
                    }
                    if (imgui::Selectable("Copy full path")) {
                        swan_path_t full_path = path_create(expl.cwd.data());
                        if (!path_append(full_path, right_clicked_ent->basic.path.data(), dir_sep_utf8, true)) {
                            // TODO: handle error
                        } else {
                            imgui::SetClipboardText(full_path.data());
                        }
                    }
                    if (imgui::Selectable("Copy size (bytes)")) {
                        imgui::SetClipboardText(std::to_string(right_clicked_ent->basic.size).c_str());
                    }
                    if (imgui::Selectable("Copy size (pretty)")) {
                        char buffer[32] = {};
                        format_file_size(right_clicked_ent->basic.size, buffer, lengthof(buffer), size_unit_multiplier);
                        imgui::SetClipboardText(buffer);
                    }
                    if (imgui::Selectable("Rename##single_open")) {
                        open_rename_popup = true;
                        dirent_to_be_renamed = right_clicked_ent;
                    }
                    if (imgui::Selectable("Reveal in File Explorer")) {
                        reveal_in_file_explorer(*right_clicked_ent, expl, dir_sep_utf16);
                    }

                    imgui::EndPopup();
                }

                imgui::EndTable();
            }

            if (imgui::IsItemHovered() && io.KeyCtrl && imgui::IsKeyPressed(ImGuiKey_A)) {
                expl.select_all_cwd_entries();
            }
            if (window_focused && imgui::IsKeyPressed(ImGuiKey_Escape)) {
                expl.deselect_all_cwd_entries();
            }
        }

        imgui::EndChild();
    }
    // cwd entries stats & table end

    if (open_rename_popup) {
        imgui::OpenPopup("Rename entry");
    }
    if (imgui::BeginPopupModal("Rename entry", &open_rename_popup, ImGuiWindowFlags_AlwaysAutoResize)) {
        render_rename_entry_popup_modal(*dirent_to_be_renamed, expl, dir_sep_utf16, open_rename_popup);
    }

    if (open_bulk_rename_popup) {
        imgui::OpenPopup("Bulk rename");
    }
    if (imgui::BeginPopupModal("Bulk rename", nullptr)) {
        render_bulk_rename_popup_modal(expl, dir_sep_utf16, open_bulk_rename_popup);
    }

    imgui::End();

    if (cwd_exists_before_edit && !path_loosely_same(expl.cwd, expl.prev_valid_cwd)) {
        expl.prev_valid_cwd = expl.cwd;
    }
}

// HRESULT handle = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

// if (FAILED(handle)) {
//     goto end_paste;
// }

// IFileOperation *file_op = NULL;
// handle = CoCreateInstance(CLSID_FileOperation, NULL, CLSCTX_ALL, IID_PPV_ARGS(&file_op));
// if (FAILED(handle)) {
//     CoUninitialize();
//     goto end_paste;
// }

// // Set the operation flags
// handle = file_op->SetOperationFlags(FOF_NOCONFIRMATION | FOFX_NOCOPYHOOKS);
// if (FAILED(handle)) {
//     file_op->Release();
//     CoUninitialize();
//     goto end_paste;
// }

// // Set the destination directory
// IShellItem* psiTo = NULL;
// hr = SHCreateItemFromParsingName(destinationDir.c_str(), NULL, IID_PPV_ARGS(&psiTo));
// if (FAILED(hr)) {
//     psiFrom->Release();
//     pfo->Release();
//     CoUninitialize();
//     return false;
// }

// // Add the move operation
// hr = pfo->MoveItem(psiFrom, psiTo, NULL, NULL);
// if (FAILED(hr)) {
//     psiTo->Release();
//     psiFrom->Release();
//     pfo->Release();
//     CoUninitialize();
//     return false;
// }

// // Perform the operation
// hr = pfo->PerformOperations();
// if (FAILED(hr)) {
//     psiTo->Release();
//     psiFrom->Release();
//     pfo->Release();
//     CoUninitialize();
//     return false;
// }

// psiTo->Release();
// psiFrom->Release();
// pfo->Release();
// CoUninitialize();
// return true;
