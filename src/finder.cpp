#include "stdafx.hpp"
#include "data_types.hpp"
#include "common_functions.hpp"
#include "imgui_dependent_functions.hpp"

namespace swan_finder
{
    static swan_thread_pool_t g_thread_pool(1);
}

void traverse_directory_recursively(swan_path const &directory_path_utf8,
                                    std::atomic<u64> &num_entries_checked,
                                    progressive_task<std::vector<finder_window::match>> &search_task,
                                    char const *search_value,
                                    u64 search_value_len) noexcept
{
    wchar_t search_path_utf16[MAX_PATH];

    if (!utf8_to_utf16(directory_path_utf8.data(), search_path_utf16, lengthof(search_path_utf16))) {
        return;
    }

    if (search_path_utf16[wcslen(search_path_utf16) - 1] != L'\\') {
        (void) StrCatW(search_path_utf16, L"\\");
    }
    (void) StrCatW(search_path_utf16, L"*");

    WIN32_FIND_DATAW find_data;
    HANDLE find_handle = FindFirstFileW(search_path_utf16, &find_data);
    SCOPE_EXIT { FindClose(find_handle); };

    if (find_handle == INVALID_HANDLE_VALUE) {
        print_debug_msg("find_handle == INVALID_HANDLE_VALUE [%s]", directory_path_utf8.data());
        return;
    }

    do {
        if (search_task.cancellation_token.load() == true) {
            return;
        }

        swan_path found_file_name = path_create("");

        if (!utf16_to_utf8(find_data.cFileName, found_file_name.data(), found_file_name.size())) {
            continue;
        }

        if (path_equals_exactly(found_file_name, ".") || path_equals_exactly(found_file_name, "..")) {
            continue;
        }

        u64 num_entries_checked_ = num_entries_checked++;

        bool is_directory = find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;

        char const *found_substr = strstr(found_file_name.data(), search_value);

        finder_window::match match = {};

        if (found_substr) {
            match.highlight_start_idx = found_substr - found_file_name.data();
            match.highlight_len = search_value_len;

            match.basic.id = (u32)num_entries_checked_;
            match.basic.size = two_u32_to_one_u64(find_data.nFileSizeLow, find_data.nFileSizeHigh);
            match.basic.creation_time_raw = find_data.ftCreationTime;
            match.basic.last_write_time_raw = find_data.ftLastWriteTime;

            match.basic.path = directory_path_utf8;
            if (!path_append(match.basic.path, found_file_name.data(), L'\\', true)) {
                continue;
            }

            if (is_directory) {
                match.basic.type = basic_dirent::kind::directory;
            }
            else if (path_ends_with(match.basic.path, ".lnk")) {
                match.basic.type = basic_dirent::kind::symlink_ambiguous;

                // TODO:
            #if 0
                if (finder.detailed_symlinks) {
                    match.basic.type = basic_dirent::kind::invalid_symlink; // default value, if something fails below

                    static std::wstring full_path_utf16 = {};
                    full_path_utf16.clear();
                    full_path_utf16.append(search_path_utf16);
                    full_path_utf16.pop_back(); // remove '*'
                    full_path_utf16.append(find_data.cFileName);

                    // Load the shortcut
                    HRESULT com_handle = s_persist_file_interface->Load(full_path_utf16.c_str(), STGM_READ);
                    if (FAILED(com_handle)) {
                        WCOUT_IF_DEBUG("FAILED IPersistFile::Load [" << full_path_utf16.c_str() << "]\n");
                    }
                    else {
                        // Get the target path
                        wchar_t target_path_utf16[MAX_PATH];
                        com_handle = s_shell_link->GetPath(target_path_utf16, lengthof(target_path_utf16), NULL, SLGP_RAWPATH);
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
            #endif
            }
            else {
                match.basic.type = basic_dirent::kind::file;
            }

            std::scoped_lock lock(search_task.result_mutex);
            search_task.result.push_back(match);
        }

        if (is_directory) {
            swan_path sub_directory_utf8 = directory_path_utf8;
            if (!path_append(sub_directory_utf8, found_file_name.data(), '\\', true, true)) {
                return;
            }
            traverse_directory_recursively(sub_directory_utf8, num_entries_checked, search_task, search_value, search_value_len);
        }
    }
    while (FindNextFileW(find_handle, &find_data));
}

void search_proc(progressive_task<std::vector<finder_window::match>> &search_task,
                 std::vector<finder_window::search_directory> search_directories,
                 std::atomic<u64> &num_entries_checked,
                 std::array<char, 1024> search_value) noexcept
{
    search_task.active_token.store(true);
    SCOPE_EXIT { search_task.active_token.store(false); };

    u64 search_value_len = strlen(search_value.data());

    for (auto const &search_dir : search_directories) {
        swan_path search_dir_path_ut8_normalized = search_dir.path_utf8;
        path_force_separator(search_dir_path_ut8_normalized, L'\\');

        traverse_directory_recursively(search_dir_path_ut8_normalized, num_entries_checked, search_task, search_value.data(), search_value_len);
    }
}

void swan_windows::render_finder(finder_window &finder, bool &open, [[maybe_unused]] bool any_popups_open) noexcept
{
    if (!imgui::Begin(swan_windows::get_name(swan_windows::id::finder), &open)) {
        imgui::End();
        return;
    }

    // auto &io = imgui::GetIO();

    if (imgui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)) {
        global_state::focused_window_set(swan_windows::id::finder);
    }

    std::optional<u64> remove_search_dir_idx = std::nullopt;

    for (u64 i = 0; i < finder.search_directories.size(); ++i) {
        auto &search_directory = finder.search_directories[i];

        {
            imgui::ScopedDisable d(finder.search_directories.size() == 1);

            auto label = make_str_static<64>(ICON_FA_MINUS "## finder search_dir %zu", i);

            if (imgui::Button(label.data())) {
                remove_search_dir_idx = i;
            }
            imgui::SameLine();
        }

        if (path_is_empty(search_directory.path_utf8)) {
            imgui::TextColored(warning_color(), ICON_CI_ALERT);
            if (imgui::IsItemHovered()) {
                imgui::SetTooltip("Empty input");
            }
        }
        else if (search_directory.found) {
            imgui::TextColored(success_color(), ICON_CI_CHECK);
            if (imgui::IsItemHovered()) {
                imgui::SetTooltip("Directory found");
            }
        }
        else {
            imgui::TextColored(error_color(), ICON_CI_X);
            if (imgui::IsItemHovered()) {
                imgui::SetTooltip("Directory not found");
            }
        }

        imgui::SameLine();

        {
            auto label = make_str_static<64>("## finder search_dir %zu", i);
            auto hint = make_str_static<64>("Search directory %zu", i);

            imgui::ScopedAvailWidth w = {};

            bool path_changed = imgui::InputTextWithHint(label.data(), hint.data(), search_directory.path_utf8.data(), search_directory.path_utf8.max_size(),
                                                         ImGuiInputTextFlags_CallbackCharFilter, filter_chars_callback, (void *)windows_illegal_path_chars());

            if (path_changed) {
                wchar_t path_utf16[MAX_PATH];
                if (utf8_to_utf16(search_directory.path_utf8.data(), path_utf16, lengthof(path_utf16))) {
                    search_directory.found = PathIsDirectoryW(path_utf16);
                }
            }
        }
    }

    if (remove_search_dir_idx.has_value()) {
        finder.search_directories.erase(finder.search_directories.begin() + remove_search_dir_idx.value());
    }

    if (imgui::Button(ICON_FA_PLUS "## finder search_dir")) {
        finder.search_directories.push_back({ false, path_create("") });
    }

    imgui::SameLine();

    imgui::Text(ICON_CI_BLANK);

    imgui::SameLine();

    bool search_active = finder.search_task.active_token.load();
    [[maybe_unused]] bool search_cancelled = finder.search_task.cancellation_token.load();

    {
        auto width = std::max(
            imgui::CalcTextSize(finder.search_value.data()).x + (imgui::GetStyle().FramePadding.x * 2) + 10.f,
            imgui::CalcTextSize("1").x * 20.75f
        );

        imgui::ScopedDisable d(search_active);
        imgui::ScopedItemWidth iw(width);

        if (finder.focus_search_value_input) {
            finder.focus_search_value_input = false;
            imgui::ActivateItemByID(imgui::GetID("## finder search_value"));
        }

        imgui::InputTextWithHint("## finder search_value", "Search for...", finder.search_value.data(), finder.search_value.max_size(),
                                 ImGuiInputTextFlags_CallbackCharFilter, filter_chars_callback, (void *)windows_illegal_path_chars());

        if (imgui::IsItemFocused() && imgui::IsKeyPressed(ImGuiKey_Enter)) {
            swan_finder::g_thread_pool.push_task([&finder]() {
                search_proc(std::ref(finder.search_task), finder.search_directories, std::ref(finder.num_entries_checked), finder.search_value);
            });
        }
    }

    imgui::SameLine();

    if (search_active) {
        if (imgui::Button(ICON_FA_STOP "## finder")) {
            finder.search_task.cancellation_token.store(true);
        }
    } else {
        bool search_value_empty = cstr_empty(finder.search_value.data());
        bool any_search_dirs_not_found = std::any_of(finder.search_directories.begin(), finder.search_directories.end(),
                                                     [](finder_window::search_directory const &sd) { return !sd.found; });

        imgui::ScopedDisable d(search_value_empty || any_search_dirs_not_found);

        if (imgui::Button(ICON_FA_SEARCH "## finder")) {
            finder.search_task.result.clear();
            finder.num_entries_checked.store(0);

            swan_finder::g_thread_pool.push_task([&finder]() {
                search_proc(std::ref(finder.search_task), finder.search_directories, std::ref(finder.num_entries_checked), finder.search_value);
            });
        }
    }

    imgui::SameLine();

    {
        u64 num_entries_checked = finder.num_entries_checked.load();
        if (num_entries_checked > 0) {
            u64 num_matches = 0;
            {
                std::scoped_lock lock(finder.search_task.result_mutex);
                num_matches = finder.search_task.result.size();
            }
            imgui::Text("%zu of %zu (%.2lf %%) entries matched", num_matches, num_entries_checked, (f64(num_matches) / f64(num_entries_checked) * 100.0));
        }
    }

    imgui::Spacing(1);

    // imgui::Text("Status: %s", progressive_task_status_cstr(finder.search_task.status()));

    enum matches_table_col : s32 {
        matches_table_col_number,
        matches_table_col_id,
        matches_table_col_name,
        matches_table_col_parent,
        matches_table_col_count
    };

    s32 table_flags =
        ImGuiTableFlags_SizingStretchProp|
        ImGuiTableFlags_Hideable|
        ImGuiTableFlags_Resizable|
        ImGuiTableFlags_Reorderable|
        ImGuiTableFlags_BordersV|
        ImGuiTableFlags_ScrollY|
        (global_state::settings().tables_alt_row_bg ? ImGuiTableFlags_RowBg : 0)|
        (global_state::settings().table_borders_in_body ? 0 : ImGuiTableFlags_NoBordersInBody)
    ;

    if (imgui::BeginChild("## finder matches child")) {
        if (imgui::BeginTable("## finder matches table", matches_table_col_count, table_flags)) {
            imgui::TableSetupColumn("#", ImGuiTableColumnFlags_NoSort, 0.0f, matches_table_col_number);
            imgui::TableSetupColumn("ID", ImGuiTableColumnFlags_DefaultSort, 0.0f, matches_table_col_id);
            imgui::TableSetupColumn("Name", ImGuiTableColumnFlags_DefaultSort, 0.0f, matches_table_col_name);
            imgui::TableSetupColumn("Location", ImGuiTableColumnFlags_DefaultSort, 0.0f, matches_table_col_parent);
            ImGui::TableSetupScrollFreeze(0, 1);
            imgui::TableHeadersRow();

            auto &matches = finder.search_task.result;
            auto &mutex = finder.search_task.result_mutex;

            std::scoped_lock lock(mutex);

            ImGuiListClipper clipper;
            assert(matches.size() <= (u64)INT32_MAX);
            clipper.Begin((s32)matches.size());

            while (clipper.Step())
            for (u64 i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                finder_window::match const &m = matches[i];

                imgui::TableNextRow();

                if (imgui::TableSetColumnIndex(matches_table_col_number)) {
                    imgui::Text("%zu", i+1);
                }

                if (imgui::TableSetColumnIndex(matches_table_col_id)) {
                    imgui::Text("%zu", m.basic.id);
                }

                if (imgui::TableSetColumnIndex(matches_table_col_name)) {
                    char const *icon = m.basic.kind_icon();
                    ImVec4 icon_color = get_color(m.basic.type);

                    imgui::TextColored(icon_color, icon);
                    imgui::SameLine();

                    ImVec2 path_text_rect_min = imgui::GetCursorScreenPos();
                    char const *file_name = path_cfind_filename(m.basic.path.data());
                    imgui::TextUnformatted(file_name);
                    imgui::HighlightTextRegion(path_text_rect_min, file_name, m.highlight_start_idx, m.highlight_len,
                                               imgui::ReduceAlphaTo(imgui::Denormalize(warning_lite_color()), 75));

                    f32 offset_for_icon = imgui::CalcTextSize(icon).x + imgui::GetStyle().ItemSpacing.x;

                    if (imgui::RenderTooltipWhenColumnTextTruncated(matches_table_col_name, file_name, offset_for_icon)) {
                        // TODO: highlight inside tooltip
                        // ImVec2 tooltip_text_rect_min = imgui::GetCursorScreenPos();
                        // imgui::HighlightTextRegion(tooltip_text_rect_min, file_name, m.highlight_start_idx, m.highlight_len);
                    }
                }

                if (imgui::TableSetColumnIndex(matches_table_col_parent)) {
                    std::string_view parent = path_extract_location(m.basic.path.data());
                    imgui::TextUnformatted(parent.data(), parent.data() + parent.length() - 1);
                }
            }

            imgui::EndTable();
        }
    }
    imgui::EndChild();

    imgui::End();
}
