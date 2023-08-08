#ifndef SWAN_EXPLORER_WINDOW_CPP
#define SWAN_EXPLORER_WINDOW_CPP

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

#include "imgui/imgui.h"

#include "primitives.hpp"
#include "on_scope_exit.hpp"
#include "common.hpp"
#include "scoped_timer.hpp"
#include "util.hpp"
#include "path.hpp"

#define MAX_EXPLORER_WD_HISTORY 15

using namespace swan;

static IShellLinkW *s_shell_link = nullptr;
static IPersistFile *s_persist_file_interface = nullptr;

static
bool init_windows_shell_com_garbage()
{
    // COM has to be one of the dumbest things I've ever seen...
    // what's wrong with just having some functions? Why on earth does this stuff need to be OO?

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

static
void cleanup_windows_shell_com_garbage()
{
    s_persist_file_interface->Release();
    s_shell_link->Release();
    CoUninitialize();
}

struct paste_payload
{
    struct item
    {
        u64 size;
        basic_dir_ent::kind type;
        path_t path;
    };

    char const *window_name = nullptr;
    std::vector<item> items = {};
    // false indicates a cut, true indicates a copy
    bool keep_src = {};
};

static paste_payload s_paste_payload = {};

static
std::pair<i32, std::array<char, 64>> filetime_to_string(FILETIME *time) noexcept(true)
{
    std::array<char, 64> buffer = {};
    DWORD flags = FDTF_SHORTDATE | FDTF_SHORTTIME;
    i32 length = SHFormatDateTimeA(time, &flags, buffer.data(), (u32)buffer.size());

    // for some reason, SHFormatDateTimeA will pad parts of the string with ASCII 63 (?)
    // when using LONGDATE or LONGTIME, so we will simply convert them to spaces
    std::replace(buffer.begin(), buffer.end(), '?', ' ');

    return { length, buffer };
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

    using dir_ent_t = explorer_window::dir_ent;

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
                    auto compute_precedence = [](explorer_window::dir_ent const &ent) -> u32 {
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
    explorer_options const &opts,
    std::source_location sloc)
{
    debug_log("[%s] update_cwd_entries() called from [%s:%d]",
        expl_ptr->name, get_just_file_name(sloc.file_name()), sloc.line());

    IM_ASSERT(expl_ptr != nullptr);

    scoped_timer<timer_unit::MICROSECONDS> function_timer(&expl_ptr->update_cwd_entries_total_us);

    bool parent_dir_exists = false;

    char dir_sep = opts.dir_separator();

    explorer_window &expl = *expl_ptr;
    expl.needs_sort = true;
    expl.update_cwd_entries_total_us = 0;
    expl.update_cwd_entries_searchpath_setup_us = 0;
    expl.update_cwd_entries_filesystem_us = 0;
    expl.update_cwd_entries_filter_us = 0;
    expl.update_cwd_entries_regex_ctor_us = 0;

    if (actions & query_filesystem) {
        static std::vector<path_t> selected_entries = {};
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
                scoped_timer<timer_unit::MICROSECONDS> search_path_timer(&expl.update_cwd_entries_searchpath_setup_us);

                utf8_to_utf16(parent_dir.data(), search_path_utf16, lengthof(search_path_utf16));

                wchar_t dir_sep_w[] = { (wchar_t)dir_sep, L'\0' };

                if (!parent_dir.ends_with(dir_sep)) {
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
                goto exit;
            } else {
                parent_dir_exists = true;
            }

            u32 id = 0;

            do {
                explorer_window::dir_ent entry = {};
                entry.basic.id = id;
                entry.basic.size = two_u32_to_one_u64(find_data.nFileSizeLow, find_data.nFileSizeHigh);
                entry.basic.creation_time_raw = find_data.ftCreationTime;
                entry.basic.last_write_time_raw = find_data.ftLastWriteTime;

                {
                    u64 utf_written = utf16_to_utf8(find_data.cFileName, entry.basic.path.data(), entry.basic.path.size());
                    // TODO: check utf_written
                }

                if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    entry.basic.type = basic_dir_ent::kind::directory;
                }
                else if (path_ends_with(entry.basic.path, ".lnk")) {
                    entry.basic.type = basic_dir_ent::kind::symlink;
                }
                else {
                    entry.basic.type = basic_dir_ent::kind::file;
                }

                if (path_equals_exactly(entry.basic.path, ".")) {
                    continue;
                }

                if (path_equals_exactly(entry.basic.path, "..")) {
                    if (opts.show_dotdot_dir) {
                        expl.cwd_entries.emplace_back(entry);
                        std::swap(expl.cwd_entries.back(), expl.cwd_entries.front());
                    }
                } else {
                    for (auto const &prev_selected_entry : selected_entries) {
                        bool was_selected_before_refresh = path_strictly_same(entry.basic.path, prev_selected_entry);
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

exit:
    expl.last_refresh_time = current_time();
    return parent_dir_exists;
}

bool explorer_window::save_to_disk() const noexcept(true)
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

bool explorer_window::load_from_disk(char dir_separator) noexcept(true)
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

void new_history_from(explorer_window &expl, path_t const &new_latest_entry)
{
    path_t new_latest_entry_clean;
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

        if (MAX_EXPLORER_WD_HISTORY == expl.wd_history.size()) {
            expl.wd_history.pop_front();
        } else {
            ++expl.wd_history_pos;
        }
    }

    expl.wd_history.push_back(new_latest_entry_clean);
}

void try_ascend_directory(explorer_window &expl, explorer_options const &opts)
{
    auto &cwd = expl.cwd;

    char dir_separator = opts.dir_separator();

    // if there is a trailing separator, remove it
    path_pop_back_if(cwd, dir_separator);

    // remove anything between end and final separator
    while (path_pop_back_if_not(cwd, dir_separator));

    (void) update_cwd_entries(full_refresh, &expl, expl.cwd.data(), opts);

    new_history_from(expl, expl.cwd);
    expl.cwd_prev_selected_dirent_idx = explorer_window::NO_SELECTION;
    expl.filter_error.clear();

    (void) expl.save_to_disk();
}

void try_descend_to_directory(explorer_window &expl, char const *child_dir, explorer_options const &opts)
{
    path_t new_cwd = expl.cwd;
    char dir_separator = opts.dir_separator();

    if (path_append(expl.cwd, child_dir, dir_separator, true)) {
        if (PathCanonicalizeA(new_cwd.data(), expl.cwd.data())) {
            debug_log("[%s] PathCanonicalizeA success: new_cwd = [%s]", expl.name, new_cwd.data());

            (void) update_cwd_entries(full_refresh, &expl, new_cwd.data(), opts);

            new_history_from(expl, new_cwd);
            expl.cwd = new_cwd;
            expl.cwd_prev_selected_dirent_idx = explorer_window::NO_SELECTION;
            expl.filter_error.clear();

            (void) expl.save_to_disk();
        }
        else {
            debug_log("[%s] PathCanonicalizeA failed", expl.name);
        }
    }
    else {
        debug_log("[%s] path_append failed, new_cwd = [%s], append data = [%c%s]", expl.name, new_cwd.data(), dir_separator, child_dir);
        expl.cwd = new_cwd;
    }
}

struct cwd_text_input_callback_user_data
{
    explorer_window *expl_ptr;
    explorer_options *opts_ptr;
    wchar_t dir_sep_utf16;
};

i32 cwd_text_input_callback(ImGuiInputTextCallbackData *data)
{
    auto user_data = (cwd_text_input_callback_user_data *)(data->UserData);
    auto &expl = *user_data->expl_ptr;
    auto &opts = *user_data->opts_ptr;
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

        bool cwd_exists_after_edit = update_cwd_entries(full_refresh, &expl, new_cwd, opts);

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

void render_explorer_window(explorer_window &expl, explorer_options &opts)
{
    if (!ImGui::Begin(expl.name)) {
        ImGui::End();
        return;
    }

    ImVec4 const orange(1, 0.5f, 0, 1);
    ImVec4 const red(1, 0.2f, 0, 1);

    auto &io = ImGui::GetIO();
    bool window_focused = ImGui::IsWindowFocused();

    // if (window_focused) {
    //     save_focused_window(expl.name);
    // }

    bool cwd_exists_before_edit = directory_exists(expl.cwd.data());
    char dir_sep_utf8 = opts.dir_separator();
    wchar_t dir_sep_utf16 = dir_sep_utf8;

    path_force_separator(expl.cwd, dir_sep_utf8);

    // handle enter key pressed on cwd entry
    if (window_focused && ImGui::IsKeyPressed(ImGuiKey_Enter)) {
        if (explorer_window::NO_SELECTION == expl.cwd_prev_selected_dirent_idx) {
            debug_log("[%s] pressed enter but cwd_prev_selected_dirent_idx was NO_SELECTION", expl.name);
        } else {
            auto dirent_which_enter_pressed_on = expl.cwd_entries[expl.cwd_prev_selected_dirent_idx];
            debug_log("[%s] pressed enter on [%s]", expl.name, dirent_which_enter_pressed_on.basic.path.data());
            if (dirent_which_enter_pressed_on.basic.is_directory()) {
                try_descend_to_directory(expl, dirent_which_enter_pressed_on.basic.path.data(), opts);
            }
        }
    }

    // debug info start
    if (opts.show_debug_info) {
        auto calc_perc_total_time = [&expl](f64 time) {
            return time == 0.f
                ? 0.f
                : ( (time / expl.update_cwd_entries_total_us) * 100.f );
        };

        ImGui::Text("prev_valid_cwd = [%s]", expl.prev_valid_cwd.data());

        if (ImGui::BeginTable("timers", 3, ImGuiTableFlags_BordersInnerV|ImGuiTableFlags_Resizable)) {
            ImGui::TableNextColumn();
            ImGui::SeparatorText("misc. state");
            ImGui::Text("num_file_finds");
            ImGui::Text("cwd_prev_selected_dirent_idx");
            ImGui::Text("num_selected_cwd_entries");
            ImGui::Text("latest_save_to_disk_result");

            ImGui::TableNextColumn();
            ImGui::SeparatorText("");
            ImGui::Text("%zu", expl.num_file_finds);
            ImGui::Text("%lld", expl.cwd_prev_selected_dirent_idx);
            ImGui::Text("%zu", expl.num_selected_cwd_entries);
            ImGui::Text("%d", expl.latest_save_to_disk_result);

            ImGui::TableNextColumn();
            ImGui::SeparatorText("");

            ImGui::TableNextColumn();
            ImGui::SeparatorText("update_cwd_entries timers");
            ImGui::TextUnformatted("total_us");
            ImGui::TextUnformatted("searchpath_setup_us");
            ImGui::TextUnformatted("filesystem_us");
            ImGui::TextUnformatted("filter_us");
            ImGui::TextUnformatted("regex_ctor_us");

            ImGui::TableNextColumn();
            ImGui::SeparatorText("");
            ImGui::Text("%.1lf", expl.update_cwd_entries_total_us);
            ImGui::Text("%.1lf", expl.update_cwd_entries_searchpath_setup_us);
            ImGui::Text("%.1lf", expl.update_cwd_entries_filesystem_us);
            ImGui::Text("%.1lf", expl.update_cwd_entries_filter_us);
            ImGui::Text("%.1lf", expl.update_cwd_entries_regex_ctor_us);

            ImGui::TableNextColumn();
            ImGui::SeparatorText("");
            ImGui::Text("%.1lf ms", expl.update_cwd_entries_total_us / 1000.f);
            ImGui::Text("%.1lf %%", calc_perc_total_time(expl.update_cwd_entries_searchpath_setup_us));
            ImGui::Text("%.1lf %%", calc_perc_total_time(expl.update_cwd_entries_filesystem_us));
            ImGui::Text("%.1lf %%", calc_perc_total_time(expl.update_cwd_entries_filter_us));
            ImGui::Text("%.1lf %%", calc_perc_total_time(expl.update_cwd_entries_regex_ctor_us));

            ImGui::TableNextColumn();
            ImGui::SeparatorText("other timers");
            ImGui::TextUnformatted("sort_us");
            ImGui::TextUnformatted("unpin_us");
            ImGui::TextUnformatted("save_to_disk_us");

            ImGui::TableNextColumn();
            ImGui::SeparatorText("");
            ImGui::Text("%.1lf", expl.sort_us);
            ImGui::Text("%.1lf", expl.unpin_us);
            ImGui::Text("%.1lf", expl.save_to_disk_us);

            ImGui::TableNextColumn();
            ImGui::SeparatorText("");

            ImGui::EndTable();
        }
    }
    // debug info end

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(0, 10.f));
    if (ImGui::BeginTable("first_3_control_rows", 1, ImGuiTableFlags_SizingFixedFit)) {

        ImGui::TableNextColumn();

        // refresh button, ctrl-r refresh logic, automatic refreshing
        {
            bool refreshed = false; // to avoid refreshing twice in one frame

            auto refresh = [&](std::source_location sloc = std::source_location::current()) {
                if (!refreshed) {
                    (void) update_cwd_entries(full_refresh, &expl, expl.cwd.data(), opts, sloc);
                    refreshed = true;
                }
            };

            if (
                opts.ref_mode == explorer_options::refresh_mode::manual ||
                (opts.ref_mode == explorer_options::refresh_mode::adaptive && expl.cwd_entries.size() > opts.adaptive_refresh_threshold)
            ) {
                if (ImGui::Button("Refresh") && !refreshed) {
                    debug_log("[%s] refresh button pressed", expl.name);
                    refresh();
                }

                ImGui::SameLine();
                ImGui::Spacing();
                ImGui::SameLine();
                ImGui::Spacing();
                ImGui::SameLine();
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

            if (window_focused && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_R)) {
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

            ImGui::BeginDisabled(!cwd_exists_before_edit && !already_pinned);

            if (ImGui::Button(buffer)) {
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
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip(" Click here to %s the current working directory. ",
                    already_pinned ? "unpin" : "pin");
            }

            ImGui::EndDisabled();
        }
        // pin cwd button end

        ImGui::SameLine();
        ImGui::Spacing();
        ImGui::SameLine();
        ImGui::Spacing();
        ImGui::SameLine();

        // up a directory arrow start
        {
            ImGui::BeginDisabled(!cwd_exists_before_edit);

            if (ImGui::ArrowButton("Up", ImGuiDir_Up)) {
                debug_log("[%s] up arrow button triggered", expl.name);
                try_ascend_directory(expl, opts);
            }

            ImGui::EndDisabled();
        }
        // up a directory arrow end

        ImGui::SameLine();

        // history back (left) arrow start
        {
            ImGui::BeginDisabled(expl.wd_history_pos == 0);

            if (ImGui::ArrowButton("Back", ImGuiDir_Left)) {
                debug_log("[%s] back arrow button triggered", expl.name);

                if (io.KeyShift || io.KeyCtrl) {
                    expl.wd_history_pos = 0;
                } else {
                    expl.wd_history_pos -= 1;
                }

                expl.cwd = expl.wd_history[expl.wd_history_pos];
                (void) update_cwd_entries(full_refresh, &expl, expl.cwd.data(), opts);
            }

            ImGui::EndDisabled();
        }
        // history back (left) arrow end

        ImGui::SameLine();

        // history forward (right) arrow
        {
            // assert(!expl.wd_history.empty());

            u64 wd_history_last_idx = expl.wd_history.empty() ? 0 : expl.wd_history.size() - 1;

            ImGui::BeginDisabled(expl.wd_history_pos == wd_history_last_idx);

            if (ImGui::ArrowButton("Forward", ImGuiDir_Right)) {
                debug_log("[%s] forward arrow button triggered", expl.name);

                if (io.KeyShift || io.KeyCtrl) {
                    expl.wd_history_pos = wd_history_last_idx;
                } else {
                    expl.wd_history_pos += 1;
                }

                expl.cwd = expl.wd_history[expl.wd_history_pos];
                (void) update_cwd_entries(full_refresh, &expl, expl.cwd.data(), opts);
            }

            ImGui::EndDisabled();
        }
        // history forward (right) arrow end

        ImGui::SameLine();

        // history browser start
        {
            if (ImGui::Button("History")) {
                ImGui::OpenPopup("history_popup");
            }

            if (ImGui::BeginPopup("history_popup")) {
                ImGui::TextUnformatted("History");

                ImGui::SameLine();

                ImGui::BeginDisabled(expl.wd_history.empty());
                if (ImGui::SmallButton("Clear")) {
                    expl.wd_history.clear();
                    expl.wd_history_pos = 0;

                    if (cwd_exists_before_edit) {
                        new_history_from(expl, expl.cwd);
                    }

                    expl.save_to_disk();
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndDisabled();

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                if (expl.wd_history.empty()) {
                    ImGui::TextUnformatted("(empty)");
                }

                u64 i = expl.wd_history.size() - 1;
                u64 i_inverse = 0;

                for (auto iter = expl.wd_history.rbegin(); iter != expl.wd_history.rend(); ++iter, --i, ++i_inverse) {
                    path_t const &hist_path = *iter;

                    i32 const history_pos_max_digits = 3;
                    char buffer[512];

                    snprintf(buffer, lengthof(buffer), "%s %-*zu %s ",
                        (i == expl.wd_history_pos ? "->" : "  "),
                        history_pos_max_digits,
                        i_inverse + 1,
                        hist_path.data());

                    if (ImGui::Selectable(buffer, false)) {
                        expl.wd_history_pos = i;
                        expl.cwd = expl.wd_history[i];
                        (void) update_cwd_entries(full_refresh, &expl, expl.cwd.data(), opts);
                        (void) expl.save_to_disk();
                    }
                }

                ImGui::EndPopup();
            }
        }
        // history browser end

        ImGui::SameLine();
        ImGui::Spacing();
        ImGui::SameLine();
        ImGui::Spacing();
        ImGui::SameLine();

        // filter type start
        {
            // important that they are all the same length,
            // this assumption is leveraged for calculation of combo box width
            static char const *filter_modes[] = {
                "Contains",
                "RegExp  ",
            };

            static_assert(lengthof(filter_modes) == (u64)explorer_window::filter_mode::count);

            ImVec2 max_dropdown_elem_size = ImGui::CalcTextSize(filter_modes[0]);

            ImGui::PushItemWidth(max_dropdown_elem_size.x + 30.f); // some extra for the dropdown button
            ImGui::Combo("##filter_mode", (i32 *)(&expl.filter_mode), filter_modes, lengthof(filter_modes));
            ImGui::PopItemWidth();
        }
        // filter type end

        ImGui::SameLine();

        // filter case sensitivity button start
        {
            if (ImGui::Button(expl.filter_case_sensitive ? "s" : "i")) {
                flip_bool(expl.filter_case_sensitive);
                (void) update_cwd_entries(filter, &expl, expl.cwd.data(), opts);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip(
                    " Toggle filter case sensitivity \n"
                    " ------------------------------ \n"
                    " %sinsensitive%s      %ssensitive%s ",
                    !expl.filter_case_sensitive ? "[" : " ", !expl.filter_case_sensitive ? "]" : " ",
                    expl.filter_case_sensitive ? "[" : " ", expl.filter_case_sensitive ? "]" : " ");
            }
        }
        // filter case sensitivity button start

        ImGui::SameLine();

        // filter text input start
        {
            ImGui::PushItemWidth(max(
                ImGui::CalcTextSize(expl.filter.data()).x + (ImGui::GetStyle().FramePadding.x * 2) + 10.f,
                ImGui::CalcTextSize("123456789012345").x
            ));
            // TODO: apply callback to filter illegal characters
            if (ImGui::InputTextWithHint("##filter", "Filter", expl.filter.data(), expl.filter.size())) {
                (void) update_cwd_entries(filter, &expl, expl.cwd.data(), opts);
                (void) expl.save_to_disk();
            }
            ImGui::PopItemWidth();
        }
        // filter text input end

        ImGui::TableNextColumn();

        if (ImGui::Button("+dir")) {
            ImGui::OpenPopup("Create directory");
        }
        if (ImGui::BeginPopupModal("Create directory", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            static char dir_name_utf8[MAX_PATH] = {};
            static std::string err_msg = {};

            auto cleanup_and_close_popup = [&]() {
                dir_name_utf8[0] = L'\0';
                err_msg.clear();
                ImGui::CloseCurrentPopup();
            };

            // set initial focus on input text below
            if (ImGui::IsWindowAppearing() && !ImGui::IsAnyItemActive() && !ImGui::IsMouseClicked(0)) {
                ImGui::SetKeyboardFocusHere(0);
            }
            // TODO: apply callback to filter illegal characters
            if (ImGui::InputTextWithHint("##dir_name_input", "Directory name...", dir_name_utf8, lengthof(dir_name_utf8))) {
                err_msg.clear();
            }

            ImGui::Spacing();

            if (ImGui::Button("Create") && dir_name_utf8[0] != '\0') {
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
                    update_cwd_entries(full_refresh, &expl, expl.cwd.data(), opts);
                }

                end_create_dir:;
            }

            ImGui::SameLine();

            if (ImGui::Button("Cancel")) {
                cleanup_and_close_popup();
            }

            if (!err_msg.empty()) {
                ImGui::Spacing();
                ImGui::TextColored(red, "Error: %s", err_msg.c_str());
            }

            if (ImGui::IsWindowFocused() && ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                cleanup_and_close_popup();
            }

            ImGui::EndPopup();
        }

        ImGui::SameLine();

        if (ImGui::Button("+file")) {
            ImGui::OpenPopup("Create file");
        }
        if (ImGui::BeginPopupModal("Create file", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            static char file_name_utf8[MAX_PATH] = {};
            static std::string err_msg = {};

            auto cleanup_and_close_popup = [&]() {
                file_name_utf8[0] = L'\0';
                err_msg.clear();
                ImGui::CloseCurrentPopup();
            };

            // set initial focus on input text below
            if (ImGui::IsWindowAppearing() && !ImGui::IsAnyItemActive() && !ImGui::IsMouseClicked(0)) {
                ImGui::SetKeyboardFocusHere(0);
            }
            // TODO: apply callback to filter illegal characters
            if (ImGui::InputTextWithHint("##file_name_input", "File name...", file_name_utf8, lengthof(file_name_utf8))) {
                err_msg.clear();
            }

            ImGui::Spacing();

            if (ImGui::Button("Create") && file_name_utf8[0] != '\0') {
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
                    update_cwd_entries(full_refresh, &expl, expl.cwd.data(), opts);
                }

                end_create_file:;
            }

            ImGui::SameLine();

            if (ImGui::Button("Cancel")) {
                cleanup_and_close_popup();
            }

            if (!err_msg.empty()) {
                ImGui::Spacing();
                ImGui::TextColored(red, "Error: %s", err_msg.c_str());
            }

            if (ImGui::IsWindowFocused() && ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                cleanup_and_close_popup();
            }

            ImGui::EndPopup();
        }

        ImGui::SameLine();
        ImGui::Spacing();
        ImGui::SameLine();

        ImGui::Text("Bulk ops:");
        ImGui::SameLine();

        ImGui::BeginDisabled(expl.num_selected_cwd_entries == 0);
        if (ImGui::Button("Cut")) {
            s_paste_payload.window_name = expl.name;
            s_paste_payload.items.clear();
            s_paste_payload.keep_src = false;

            for (auto const &dir_ent : expl.cwd_entries) {
                if (dir_ent.is_selected) {
                    path_t src = expl.cwd;
                    if (path_append(src, dir_ent.basic.path.data(), dir_sep_utf8, true)) {
                        s_paste_payload.items.push_back({ dir_ent.basic.size, dir_ent.basic.type, src });
                    } else {
                        // TODO: handle error
                    }
                }
            }
        }
        ImGui::EndDisabled();

        ImGui::SameLine();

        ImGui::BeginDisabled(expl.num_selected_cwd_entries == 0);
        if (ImGui::Button("Copy")) {
            s_paste_payload.window_name = expl.name;
            s_paste_payload.items.clear();
            s_paste_payload.keep_src = true;

            for (auto const &dir_ent : expl.cwd_entries) {
                if (dir_ent.is_selected) {
                    path_t src = expl.cwd;
                    if (path_append(src, dir_ent.basic.path.data(), dir_sep_utf8, true)) {
                        s_paste_payload.items.push_back({ dir_ent.basic.size, dir_ent.basic.type, src });
                    } else {
                        // TODO: handle error
                    }
                }
            }
        }
        ImGui::EndDisabled();

        ImGui::SameLine();

        ImGui::BeginDisabled(expl.num_selected_cwd_entries == 0);
        if (ImGui::Button("Delete")) {
            // TODO: setup IFileOperation

            for (auto const &dir_ent : expl.cwd_entries) {
                if (!dir_ent.is_selected) {
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
        ImGui::EndDisabled();

        ImGui::SameLine();

        // paste payload description start
        if (!s_paste_payload.items.empty()) {
            ImGui::SameLine();
            ImGui::Spacing();
            ImGui::SameLine();

            u64 num_dirs = 0, num_symlinks = 0, num_files = 0;
            for (auto const &item : s_paste_payload.items) {
                num_dirs     += u64(item.type == basic_dir_ent::kind::directory);
                num_symlinks += u64(item.type == basic_dir_ent::kind::symlink);
                num_files    += u64(item.type == basic_dir_ent::kind::file);
            }

            if (num_dirs > 0) {
                ImGui::SameLine();
                ImGui::TextColored(basic_dir_ent::get_color(basic_dir_ent::kind::directory), "%zud", num_dirs);
            }
            if (num_symlinks > 0) {
                ImGui::SameLine();
                ImGui::TextColored(basic_dir_ent::get_color(basic_dir_ent::kind::symlink), "%zus", num_symlinks);
            }
            if (num_files > 0) {
                ImGui::SameLine();
                ImGui::TextColored(basic_dir_ent::get_color(basic_dir_ent::kind::file), "%zuf", num_files);
            }

            ImGui::SameLine();
            ImGui::Text("ready to be %s from %s", (s_paste_payload.keep_src ? "copied" : "cut"), s_paste_payload.window_name);

            ImGui::SameLine();
            ImGui::Spacing();
            ImGui::SameLine();

            if (ImGui::Button("Paste")) {
                bool keep_src = s_paste_payload.keep_src;

                // TODO: setup IFileOperation

                for (auto const &paste_item : s_paste_payload.items) {
                    if (paste_item.type == basic_dir_ent::kind::directory) {
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

            ImGui::SameLine();

            if (ImGui::Button("X##Cancel")) {
                s_paste_payload.items.clear();
            }
        }
        // paste payload description end

        ImGui::TableNextColumn();

        // cwd text input start
        {
            cwd_text_input_callback_user_data user_data;
            user_data.expl_ptr = &expl;
            user_data.opts_ptr = &opts;
            user_data.dir_sep_utf16 = dir_sep_utf16;

            ImGui::PushItemWidth(
                max(ImGui::CalcTextSize(expl.cwd.data()).x + (ImGui::GetStyle().FramePadding.x * 2),
                    ImGui::CalcTextSize("123456789_123456789_").x)
                + 60.f
            );

            ImGui::InputText("##cwd", expl.cwd.data(), expl.cwd.size(),
                ImGuiInputTextFlags_CallbackCharFilter|ImGuiInputTextFlags_CallbackEdit,
                cwd_text_input_callback, (void *)&user_data);

            expl.cwd = path_squish_adjacent_separators(expl.cwd);

            ImGui::PopItemWidth();

            ImGui::SameLine();

            // label
            if (opts.show_cwd_len) {
                ImGui::Text("cwd(%3d)", path_length(expl.cwd));
            }
        }
        // cwd text input end

        // clicknav start
        if (cwd_exists_before_edit && !path_is_empty(expl.cwd)) {
            ImGui::TableNextColumn();

            static std::vector<char const *> slices = {};
            slices.reserve(50);
            slices.clear();

            path_t sliced_path = expl.cwd;
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

            f32 original_spacing = ImGui::GetStyle().ItemSpacing.x;

            for (auto slice_it = slices.begin(); slice_it != slices.end() - 1; ++slice_it) {
                if (ImGui::Button(*slice_it)) {
                    debug_log("[%s] clicked slice [%s]", expl.name, *slice_it);
                    cd_to_slice(*slice_it);
                    (void) update_cwd_entries(full_refresh, &expl, expl.cwd.data(), opts);
                    (void) expl.save_to_disk();
                }
                ImGui::GetStyle().ItemSpacing.x = 2;
                ImGui::SameLine();
                ImGui::Text("%c", dir_sep_utf8);
                ImGui::SameLine();
            }

            if (ImGui::Button(slices.back())) {
                debug_log("[%s] clicked slice [%s]", expl.name, slices.back());
                cd_to_slice(slices.back());
                (void) update_cwd_entries(full_refresh, &expl, expl.cwd.data(), opts);
            }

            if (slices.size() > 1) {
                ImGui::GetStyle().ItemSpacing.x = original_spacing;
            }
        }
        // clicknav end
    }
    ImGui::EndTable();
    ImGui::PopStyleVar();

    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();

    // cwd entries stats & table start

    if (!cwd_exists_before_edit) {
        ImGui::TextColored(orange, "Invalid directory.");
    }
    else if (expl.cwd_entries.empty()) {
        // cwd exists but is empty
        ImGui::TextColored(orange, "Empty directory.");
    }
    else {
        u64 num_selected_directories = 0;
        u64 num_selected_symlinks = 0;
        u64 num_selected_files = 0;

        u64 num_filtered_directories = 0;
        u64 num_filtered_symlinks = 0;
        u64 num_filtered_files = 0;

        u64 num_child_directories = 0;
        u64 num_child_symlinks = 0;

        for (auto const &dir_ent : expl.cwd_entries) {
            static_assert(false == 0);
            static_assert(true == 1);

            num_selected_directories += u64(dir_ent.is_selected && dir_ent.basic.is_directory());
            num_selected_symlinks    += u64(dir_ent.is_selected && dir_ent.basic.is_symlink());
            num_selected_files       += u64(dir_ent.is_selected && dir_ent.basic.is_non_symlink_file());

            num_filtered_directories += u64(dir_ent.is_filtered_out && dir_ent.basic.is_directory());
            num_filtered_symlinks    += u64(dir_ent.is_filtered_out && dir_ent.basic.is_symlink());
            num_filtered_files       += u64(dir_ent.is_filtered_out && dir_ent.basic.is_non_symlink_file());

            num_child_directories += u64(dir_ent.basic.is_directory());
            num_child_symlinks    += u64(dir_ent.basic.is_symlink());
        }

        u64 num_filtered_dirents = num_filtered_directories + num_filtered_symlinks + num_filtered_files;
        u64 num_selected_dirents = num_selected_directories + num_selected_symlinks + num_selected_files;
        u64 num_child_dirents = expl.cwd_entries.size();
        u64 num_child_files = num_child_dirents - num_child_directories - num_child_symlinks;

        if (expl.filter_error != "") {
            ImGui::PushTextWrapPos(ImGui::GetColumnWidth());
            ImGui::TextColored(red, "%s", expl.filter_error.c_str());
            ImGui::PopTextWrapPos();

            ImGui::Spacing();
            ImGui::Spacing();
            ImGui::Spacing();
        }

    #if 1
        (void) num_selected_dirents;

        {
            ImGui::Text("%zu items", num_child_dirents);
            if (num_child_dirents > 0) {
                ImGui::SameLine();
                ImGui::Text(" ");
            }
            if (num_child_directories > 0) {
                ImGui::SameLine();
                ImGui::TextColored(
                    basic_dir_ent::get_color(basic_dir_ent::kind::directory),
                    "%zu director%s", num_child_directories, num_child_directories == 1 ? "y" : "ies");
            }
            if (num_child_symlinks > 0) {
                ImGui::SameLine();
                ImGui::TextColored(
                    basic_dir_ent::get_color(basic_dir_ent::kind::symlink),
                    " %zu symlink%s", num_child_symlinks, num_child_symlinks == 1 ? "" : "s");
            }
            if (num_child_files > 0) {
                ImGui::SameLine();
                ImGui::TextColored(
                    basic_dir_ent::get_color(basic_dir_ent::kind::file),
                    " %zu file%s", num_child_files, num_child_files == 1 ? "" : "s");
            }
        }

    #if 0
        ImGui::SameLine();
        ImGui::Text(" ");
        ImGui::SameLine();

        {
            time_point_t now = current_time();
            ImGui::Text("refreshed %s", compute_when_str(expl.last_refresh_time, now).data());
        }
    #endif

        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::Spacing();
    #else
        if (expl.filter_error == "") {
            ImGui::Text("%zu filtered", num_filtered_dirents);
            if (num_filtered_dirents > 0) {
                ImGui::SameLine();
                ImGui::Text(":");
            }
            if (num_filtered_files > 0) {
                ImGui::SameLine();
                ImGui::Text("%zu file%s", num_filtered_files, num_filtered_files == 1 ? "" : "s");
            }
            if (num_filtered_directories > 0) {
                ImGui::SameLine();
                ImGui::Text("%zu director%s", num_filtered_directories, num_filtered_directories == 1 ? "y" : "ies");
            }
            if (num_filtered_symlinks > 0) {
                ImGui::SameLine();
                ImGui::Text("%zu shortcut%s", num_filtered_symlinks, num_filtered_symlinks == 1 ? "" : "s");
            }
        }

        ImGui::Text("%zu selected", num_selected_dirents);
        if (num_selected_dirents > 0) {
            ImGui::SameLine();
            ImGui::Text(":");
        }
        if (num_selected_files > 0) {
            ImGui::SameLine();
            ImGui::Text("%zu file%s", num_selected_files, num_selected_files == 1 ? "" : "s");
        }
        if (num_selected_directories > 0) {
            ImGui::SameLine();
            ImGui::Text("%zu director%s", num_selected_directories, num_selected_directories == 1 ? "y" : "ies");
        }
        if (num_selected_symlinks > 0) {
            ImGui::SameLine();
            ImGui::Text("%zu shortcut%s", num_selected_symlinks, num_selected_symlinks == 1 ? "" : "s");
        }

        ImGui::Spacing();
        ImGui::Spacing();
    #endif
        expl.num_selected_cwd_entries = 0; // will get computed as we render cwd_entries table

        if (ImGui::BeginChild("cwd_entries_child", ImVec2(0, ImGui::GetContentRegionAvail().y))) {
            if (num_filtered_dirents == expl.cwd_entries.size()) {
                if (ImGui::Button("Clear filter")) {
                    debug_log("[%s] clear filter button pressed", expl.name);
                    expl.filter[0] = '\0';
                    (void) update_cwd_entries(filter, &expl, expl.cwd.data(), opts);
                    (void) expl.save_to_disk();
                }

                ImGui::SameLine();
                ImGui::Spacing();
                ImGui::SameLine();

                ImGui::TextColored(orange, "All items filtered.");
            }
            else if (ImGui::BeginTable("cwd_entries", cwd_entries_table_col_count,
                ImGuiTableFlags_SizingStretchProp|ImGuiTableFlags_Hideable|ImGuiTableFlags_Resizable|ImGuiTableFlags_Reorderable|ImGuiTableFlags_Sortable
                // ImVec2(-1, ImGui::GetContentRegionAvail().y)
            )) {
                ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_NoSort, 0.0f, cwd_entries_table_col_number);
                ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_DefaultSort, 0.0f, cwd_entries_table_col_id);
                ImGui::TableSetupColumn("Path", ImGuiTableColumnFlags_DefaultSort, 0.0f, cwd_entries_table_col_path);
                ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_DefaultSort, 0.0f, cwd_entries_table_col_type);
                ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_DefaultSort, 0.0f, cwd_entries_table_col_size_pretty);
                ImGui::TableSetupColumn("Bytes", ImGuiTableColumnFlags_DefaultSort, 0.0f, cwd_entries_table_col_size_bytes);
                // ImGui::TableSetupColumn("Created", ImGuiTableColumnFlags_DefaultSort, 0.0f, cwd_entries_table_col_creation_time);
                ImGui::TableSetupColumn("Last Edited", ImGuiTableColumnFlags_DefaultSort, 0.0f, cwd_entries_table_col_last_write_time);
                ImGui::TableHeadersRow();

                ImGuiTableSortSpecs *sort_specs = ImGui::TableGetSortSpecs();
                if (sort_specs != nullptr && (expl.needs_sort || sort_specs->SpecsDirty)) {
                    sort_cwd_entries(expl, sort_specs);
                    sort_specs->SpecsDirty = false;
                    expl.needs_sort = false;
                }

                static explorer_window::dir_ent const *right_clicked_ent = nullptr;

                for (u64 i = 0; i < expl.cwd_entries.size(); ++i) {
                    auto &dir_ent = expl.cwd_entries[i];

                    if (dir_ent.is_filtered_out) {
                        ++num_filtered_dirents;
                        continue;
                    }

                    ImGui::TableNextRow();

                    if (ImGui::TableSetColumnIndex(cwd_entries_table_col_number)) {
                        ImGui::Text("%zu", i + 1);
                    }

                    if (ImGui::TableSetColumnIndex(cwd_entries_table_col_id)) {
                        ImGui::Text("%zu", dir_ent.basic.id);
                    }

                    if (ImGui::TableSetColumnIndex(cwd_entries_table_col_path)) {
                        ImGui::PushStyleColor(ImGuiCol_Text, dir_ent.basic.get_color());

                        if (ImGui::Selectable(dir_ent.basic.path.data(), dir_ent.is_selected, ImGuiSelectableFlags_SpanAllColumns)) {
                            if (!io.KeyCtrl && !io.KeyShift) {
                                // entry was selected but Ctrl was not held, so deselect everything
                                for (auto &dir_ent2 : expl.cwd_entries)
                                    dir_ent2.is_selected = false;
                            }

                            flip_bool(dir_ent.is_selected);

                            if (io.KeyShift) {
                                // shift click, select everything between the current item and the previously clicked item

                                u64 first_idx, last_idx;

                                if (explorer_window::NO_SELECTION == expl.cwd_prev_selected_dirent_idx) {
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
                                    expl.cwd_entries[j].is_selected = true;
                                }
                            }

                            static f64 last_click_time = 0;
                            f64 current_time = ImGui::GetTime();
                            f64 seconds_between_clicks = current_time - last_click_time;
                            f64 const double_click_window_sec = 0.3;

                            if (seconds_between_clicks <= double_click_window_sec) {
                                if (dir_ent.basic.is_directory()) {
                                    debug_log("[%s] double clicked directory [%s]", expl.name, dir_ent.basic.path.data());

                                    auto const &descend_target = dir_ent.basic.path;

                                    if (strcmp(descend_target.data(), "..") == 0) {
                                        try_ascend_directory(expl, opts);
                                    } else {
                                        try_descend_to_directory(expl, dir_ent.basic.path.data(), opts);
                                    }
                                }
                                else if (dir_ent.basic.is_symlink()) {
                                    path_t symlink_self_path_utf8 = expl.cwd;
                                    path_t symlink_target_path_utf8 = {};
                                    wchar_t symlink_self_path_utf16[MAX_PATH] = {};
                                    wchar_t symlink_target_path_utf16[MAX_PATH] = {};
                                    wchar_t working_dir_utf16[MAX_PATH] = {};
                                    wchar_t command_line_utf16[2048] = {};
                                    i32 show_command = SW_SHOWNORMAL;
                                    HRESULT com_handle = {};
                                    HINSTANCE result = {};
                                    LPITEMIDLIST item_id_list = nullptr;
                                    intptr_t err_code = {};
                                    i32 utf_written = {};

                                    if (!path_append(symlink_self_path_utf8, dir_ent.basic.path.data(), dir_sep_utf8, true)) {
                                        debug_log("[%s] path_append(symlink_self_path_utf8, dir_ent.basic.path.data() failed", expl.name);
                                        goto symlink_end;
                                    }

                                    debug_log("[%s] double clicked link [%s]", expl.name, symlink_self_path_utf8);

                                    utf_written = utf8_to_utf16(symlink_self_path_utf8.data(), symlink_self_path_utf16, lengthof(symlink_self_path_utf16));

                                    if (utf_written == 0) {
                                        debug_log("[%s] utf8_to_utf16 failed: symlink_self_path_utf8 -> symlink_self_path_utf16", expl.name);
                                        goto symlink_end;
                                    } else {
                                        debug_log("[%s] utf8_to_utf16 wrote %d characters", expl.name, utf_written);
                                        std::wcout << "symlink_self_path_utf16 = [" << symlink_self_path_utf16 << "]\n";
                                    }

                                    com_handle = s_persist_file_interface->Load(symlink_self_path_utf16, STGM_READ);

                                    if (com_handle != S_OK) {
                                        debug_log("[%s] s_persist_file_interface->Load [%s] failed: %s", expl.name, symlink_self_path_utf8, get_last_error_string().c_str());
                                        goto symlink_end;
                                    } else {
                                        std::wcout << "s_persist_file_interface->Load [" << symlink_self_path_utf16 << "]\n";
                                    }

                                    com_handle = s_shell_link->GetIDList(&item_id_list);

                                    if (com_handle != S_OK) {
                                        debug_log("[%s] s_shell_link->GetIDList failed: %s", expl.name, get_last_error_string().c_str());
                                        goto symlink_end;
                                    }

                                    if (!SHGetPathFromIDListW(item_id_list, symlink_target_path_utf16)) {
                                        debug_log("[%s] SHGetPathFromIDListW failed: %s", expl.name, get_last_error_string().c_str());
                                        goto symlink_end;
                                    } else {
                                        std::wcout << "symlink_target_path_utf16 = [" << symlink_target_path_utf16 << "]\n";
                                    }

                                    if (com_handle != S_OK) {
                                        debug_log("[%s] s_shell_link->GetPath failed: %s", expl.name, get_last_error_string().c_str());
                                        goto symlink_end;
                                    }

                                    utf_written = utf16_to_utf8(symlink_target_path_utf16, symlink_target_path_utf8.data(), symlink_target_path_utf8.size());

                                    if (utf_written == 0) {
                                        debug_log("[%s] utf16_to_utf8 failed", expl.name);
                                        goto symlink_end;
                                    } else {
                                        debug_log("[%s] utf16_to_utf8 wrote %d characters", expl.name, utf_written);
                                        debug_log("[%s] symlink_target_path_utf8 = [%s]", expl.name, symlink_target_path_utf8.data());
                                    }

                                    if (directory_exists(symlink_target_path_utf8.data())) {
                                        // shortcut to a directory, let's navigate there

                                        expl.cwd = symlink_target_path_utf8;
                                        (void) update_cwd_entries(full_refresh, &expl, expl.cwd.data(), opts);
                                        new_history_from(expl, expl.cwd);
                                    }
                                    else {
                                        // shortcut to a file, let's open it

                                        com_handle = s_shell_link->GetWorkingDirectory(working_dir_utf16, lengthof(working_dir_utf16));

                                        if (com_handle != S_OK) {
                                            debug_log("[%s] s_shell_link->GetWorkingDirectory failed: %s", expl.name, get_last_error_string().c_str());
                                            goto symlink_end;
                                        } else {
                                            std::wcout << "s_shell_link->GetWorkingDirectory [" << working_dir_utf16 << "]\n";
                                        }

                                        com_handle = s_shell_link->GetArguments(command_line_utf16, lengthof(command_line_utf16));

                                        if (com_handle != S_OK) {
                                            debug_log("[%s] s_shell_link->GetArguments failed: %s", expl.name, get_last_error_string().c_str());
                                            goto symlink_end;
                                        } else {
                                            std::wcout << "s_shell_link->GetArguments [" << command_line_utf16 << "]\n";
                                        }

                                        com_handle = s_shell_link->GetShowCmd(&show_command);

                                        if (com_handle != S_OK) {
                                            debug_log("[%s] s_shell_link->GetShowCmd failed: %s", expl.name, get_last_error_string().c_str());
                                            goto symlink_end;
                                        } else {
                                            std::wcout << "s_shell_link->GetShowCmd [" << show_command << "]\n";
                                        }

                                        result = ShellExecuteW(nullptr, L"open", symlink_target_path_utf16,
                                                               command_line_utf16, working_dir_utf16, show_command);

                                        err_code = (intptr_t)result;

                                        if (err_code > HINSTANCE_ERROR) {
                                            debug_log("[%s] ShellExecuteW success", expl.name);
                                        } else if (err_code == SE_ERR_NOASSOC) {
                                            debug_log("[%s] ShellExecuteW error: SE_ERR_NOASSOC", expl.name);
                                        } else if (err_code == SE_ERR_FNF) {
                                            debug_log("[%s] ShellExecuteW error: SE_ERR_FNF", expl.name);
                                        } else {
                                            debug_log("[%s] ShellExecuteW error: unexpected error", expl.name);
                                        }
                                    }

                                    symlink_end:;
                                }
                                else {
                                    debug_log("[%s] double clicked file [%s]", expl.name, dir_ent.basic.path.data());

                                    path_t target_full_path_utf8 = expl.cwd;

                                    bool app_success = path_append(target_full_path_utf8, dir_ent.basic.path.data(), dir_sep_utf8, true);

                                    if (!app_success) {
                                        debug_log("[%s] path_append failed, cwd = [%s], append data = [\\%s]", expl.name, expl.cwd.data(), dir_ent.basic.path.data());
                                    }
                                    else {
                                        wchar_t target_full_path_utf16[MAX_PATH] = {};
                                        i32 utf_written = {};
                                        HINSTANCE result = {};
                                        intptr_t ec = {};

                                        utf_written = utf8_to_utf16(target_full_path_utf8.data(), target_full_path_utf16, lengthof(target_full_path_utf16));

                                        if (utf_written == 0) {
                                            debug_log("[%s] utf8_to_utf16 failed: target_full_path_utf8 -> target_full_path_utf16", expl.name);
                                            goto dbl_click_file_end;
                                        }

                                        debug_log("[%s] utf8_to_utf16 wrote %d characters (target_full_path_utf8 -> target_full_path_utf16)", expl.name, utf_written);
                                        debug_log("[%s] target_full_path = [%s]", expl.name, target_full_path_utf8.data());

                                        result = ShellExecuteW(nullptr, L"open", target_full_path_utf16,
                                                               nullptr, nullptr, SW_SHOWNORMAL);

                                        ec = (intptr_t)result;

                                        if (ec > HINSTANCE_ERROR) {
                                            debug_log("[%s] ShellExecuteW success", expl.name);
                                        } else if (ec == SE_ERR_NOASSOC) {
                                            debug_log("[%s] ShellExecuteW: SE_ERR_NOASSOC", expl.name);
                                        } else if (ec == SE_ERR_FNF) {
                                            debug_log("[%s] ShellExecuteW: SE_ERR_FNF", expl.name);
                                        } else {
                                            debug_log("[%s] ShellExecuteW: some sort of error", expl.name);
                                        }
                                    }

                                    dbl_click_file_end:;
                                }
                            }
                            else {
                                debug_log("[%s] selected [%s]", expl.name, dir_ent.basic.path.data());
                            }

                            last_click_time = current_time;
                            expl.cwd_prev_selected_dirent_idx = i;

                        } // ImGui::Selectable

                        if (ImGui::IsItemClicked(ImGuiMouseButton_Right) && strcmp("..", dir_ent.basic.path.data()) != 0) {
                            debug_log("[%s] right clicked [%s]", expl.name, dir_ent.basic.path.data());
                            ImGui::OpenPopup("Context");
                            right_clicked_ent = &dir_ent;
                        }

                        ImGui::PopStyleColor();

                    } // path col

                    if (ImGui::TableSetColumnIndex(cwd_entries_table_col_type)) {
                        if (dir_ent.basic.is_directory()) {
                            ImGui::TextUnformatted("dir");
                        }
                        else if (dir_ent.basic.is_symlink()) {
                            ImGui::TextUnformatted("link");
                        }
                        else {
                            ImGui::TextUnformatted("file");
                        }
                    }

                    if (ImGui::TableSetColumnIndex(cwd_entries_table_col_size_pretty)) {
                        if (dir_ent.basic.is_directory()) {
                            ImGui::Text("");
                        }
                        else {
                            std::array<char, 32> pretty_size = {};
                            format_file_size(dir_ent.basic.size, pretty_size.data(), pretty_size.size(), opts.binary_size_system ? 1024 : 1000);
                            ImGui::TextUnformatted(pretty_size.data());
                        }
                    }

                    if (ImGui::TableSetColumnIndex(cwd_entries_table_col_size_bytes)) {
                        if (dir_ent.basic.is_directory()) {
                            ImGui::TextUnformatted("");
                        }
                        else {
                            ImGui::Text("%zu", dir_ent.basic.size);
                        }
                    }

                    // if (ImGui::TableSetColumnIndex(cwd_entries_table_col_creation_time)) {
                    //     auto [result, buffer] = filetime_to_string(&dir_ent.last_write_time_raw);
                    //     ImGui::TextUnformatted(buffer.data());
                    // }

                    if (ImGui::TableSetColumnIndex(cwd_entries_table_col_last_write_time)) {
                        auto [result, buffer] = filetime_to_string(&dir_ent.basic.last_write_time_raw);
                        ImGui::TextUnformatted(buffer.data());
                    }

                    expl.num_selected_cwd_entries += u64(dir_ent.is_selected);

                } // cwd_entries loop

                bool open_rename_popup = false;

                if (ImGui::BeginPopup("Context")) {
                    ImGui::PushStyleColor(ImGuiCol_Text, right_clicked_ent->basic.get_color());
                    ImGui::SeparatorText(right_clicked_ent->basic.path.data());
                    ImGui::PopStyleColor();

                    // bool is_directory = right_clicked_ent->basic.is_directory();

                    if (ImGui::Selectable("Copy name")) {
                        ImGui::SetClipboardText(right_clicked_ent->basic.path.data());
                    }
                    if (ImGui::Selectable("Copy path")) {
                        path_t full_path = path_create(expl.cwd.data());
                        if (!path_append(full_path, right_clicked_ent->basic.path.data(), dir_sep_utf8, true)) {
                            // TODO: handle error
                        } else {
                            ImGui::SetClipboardText(full_path.data());
                        }
                    }
                    if (ImGui::Selectable("Rename")) {
                        // calling OpenPopup here does not work, probably because we are already in a popup
                        open_rename_popup = true;
                    }
                    if (ImGui::Selectable("Reveal in File Explorer")) {
                        i32 utf_written = 0;
                        std::wstring select_command = {};
                        select_command.reserve(1024);

                        wchar_t select_path_cwd_buffer_utf16[MAX_PATH] = {};
                        wchar_t select_path_dirent_buffer_utf16[MAX_PATH] = {};

                        utf_written = utf8_to_utf16(expl.cwd.data(), select_path_cwd_buffer_utf16, lengthof(select_path_cwd_buffer_utf16));
                        if (utf_written == 0) {
                            debug_log("[%s] utf8_to_utf16 failed (expl.cwd -> select_path_cwd_buffer_utf16)", expl.name);
                            goto reveal_in_explorer_end;
                        }

                        select_command += L"/select,";
                        select_command += L'"';
                        select_command += select_path_cwd_buffer_utf16;
                        if (!select_command.ends_with(dir_sep_utf16)) {
                            select_command += dir_sep_utf16;
                        }

                        utf_written = utf8_to_utf16(right_clicked_ent->basic.path.data(), select_path_dirent_buffer_utf16, lengthof(select_path_dirent_buffer_utf16));
                        if (utf_written == 0) {
                            debug_log("[%s] utf8_to_utf16 failed (right_clicked_ent.basic.path -> select_path_dirent_buffer_utf16)", expl.name);
                            goto reveal_in_explorer_end;
                        }

                        select_command += select_path_dirent_buffer_utf16;
                        select_command += L'"';

                        std::wcout << "select_command: [" << select_command.c_str() << "]\n";

                        ShellExecuteW(nullptr, L"open", L"explorer.exe", select_command.c_str(), nullptr, SW_SHOWNORMAL);

                        reveal_in_explorer_end:;
                    }

                    ImGui::EndPopup();
                }

                if (open_rename_popup) {
                    ImGui::OpenPopup("Rename entry");
                }
                if (ImGui::BeginPopupModal("Rename entry", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                    static path_t new_name_utf8 = {};
                    static std::string err_msg = {};

                    auto cleanup_and_close_popup = [&]() {
                        new_name_utf8[0] = L'\0';
                        err_msg.clear();
                        ImGui::CloseCurrentPopup();
                    };

                    if (ImGui::SmallButton("Use##use_current_name")) {
                        new_name_utf8 = path_create(right_clicked_ent->basic.path.data());
                    }
                    ImGui::SameLine();
                    ImGui::TextUnformatted("Current name:");
                    ImGui::SameLine();
                    ImGui::TextColored(right_clicked_ent->basic.get_color(), right_clicked_ent->basic.path.data());

                    ImGui::Spacing();

                    // set initial focus on input text below
                    if (ImGui::IsWindowAppearing() && !ImGui::IsAnyItemActive() && !ImGui::IsMouseClicked(0)) {
                        ImGui::SetKeyboardFocusHere(0);
                    }
                    {
                        f32 avail_width = ImGui::GetContentRegionAvail().x;
                        ImGui::PushItemWidth(avail_width);
                        // TODO: apply callback to filter illegal characters
                        if (ImGui::InputTextWithHint("##New name", "New name...", new_name_utf8.data(), new_name_utf8.size(), 0, nullptr, nullptr)) {
                            err_msg.clear();
                        }
                        ImGui::PopItemWidth();
                    }

                    ImGui::Spacing();

                    if (ImGui::Button("Rename") && new_name_utf8[0] != '\0') {
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
                            goto rename_end;
                        }

                        utf_written = utf8_to_utf16(right_clicked_ent->basic.path.data(), buffer_old_name_utf16, lengthof(buffer_old_name_utf16));
                        if (utf_written == 0) {
                            debug_log("[%s] utf8_to_utf16 failed (right_clicked_ent.basic.path -> buffer_old_name_utf16)", expl.name);
                            cleanup_and_close_popup();
                            goto rename_end;
                        }

                        utf_written = utf8_to_utf16(new_name_utf8.data(), buffer_new_name_utf16, lengthof(buffer_new_name_utf16));
                        if (utf_written == 0) {
                            debug_log("[%s] utf8_to_utf16 failed (new_name_utf8 -> buffer_new_name_utf16)", expl.name);
                            cleanup_and_close_popup();
                            goto rename_end;
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
                            (void) update_cwd_entries(full_refresh, &expl, expl.cwd.data(), opts);
                            cleanup_and_close_popup();
                        }

                        rename_end:;
                    }

                    ImGui::SameLine();

                    if (ImGui::Button("Cancel")) {
                        cleanup_and_close_popup();
                    }

                    if (!err_msg.empty()) {
                        ImGui::Spacing();
                        ImGui::TextColored(red, "Error: %s", err_msg.c_str());
                    }

                    if (ImGui::IsWindowFocused() && ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                        cleanup_and_close_popup();
                    }

                    ImGui::EndPopup();
                }

                ImGui::EndTable();
            }

            if (ImGui::IsItemHovered() && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_A)) {
                // select all cwd entries when hovering over the table and pressing Ctrl-a
                for (auto &dir_ent2 : expl.cwd_entries)
                    dir_ent2.is_selected = true;

                expl.num_selected_cwd_entries = expl.cwd_entries.size();
            }
            if (window_focused && ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                for (auto &dir_ent2 : expl.cwd_entries)
                    dir_ent2.is_selected = false;

                expl.num_selected_cwd_entries = 0;
            }
        }

        ImGui::EndChild();
    }
    // cwd entries stats & table end

    ImGui::End();

    if (cwd_exists_before_edit && !path_loosely_same(expl.cwd, expl.prev_valid_cwd)) {
        expl.prev_valid_cwd = expl.cwd;
    }
}

#endif // SWAN_EXPLORER_WINDOW_CPP

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
