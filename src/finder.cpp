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

bool swan_windows::render_finder(finder_window &finder, bool &open, [[maybe_unused]] bool any_popups_open) noexcept
{
    if (!imgui::Begin(swan_windows::get_name(swan_windows::id::finder), &open)) {
        return false;
    }

    [[maybe_unused]] auto &style = imgui::GetStyle();
    [[maybe_unused]] auto const &io = imgui::GetIO();
    ImVec2 base_window_pos = imgui::GetCursorScreenPos();

    for (u64 i = 0; i < 1; ++i) {
        auto &search_directory = finder.search_directories[i];
        {
            auto label = make_str_static<64>("## finder search_dir %zu", i);
            auto hint = make_str_static<64>("Where to search...");

            imgui::ScopedAvailWidth w = {};

            bool path_changed = imgui::InputTextWithHint(label.data(), hint.data(), search_directory.path_utf8.data(), search_directory.path_utf8.max_size(),
                                                         ImGuiInputTextFlags_CallbackCharFilter, filter_chars_callback, (void *)windows_illegal_path_chars());

            if (imgui::BeginDragDropTarget()) {
                auto payload_wrapper = imgui::AcceptDragDropPayload(typeid(pin_drag_drop_payload).name());

                if (payload_wrapper != nullptr) {
                    assert(payload_wrapper->DataSize == sizeof(pin_drag_drop_payload));
                    auto payload_data = (pin_drag_drop_payload *)payload_wrapper->Data;
                    auto const &pin = global_state::pinned_get()[payload_data->pin_idx];
                    search_directory.path_utf8 = pin.path;
                    path_changed = true;
                }
                imgui::EndDragDropTarget();
            }

            if (imgui::IsItemFocused() && io.KeyCtrl && imgui::IsKeyPressed(ImGuiKey_O)) {
                imgui::OpenPopup("Bookmarks");
            }
            if (imgui::IsPopupOpen("Bookmarks")) {
                ImVec2 avail = imgui::GetContentRegionAvail();
                avail.y -= imgui::GetStyle().WindowPadding.y*10;
                imgui::SetNextWindowPos(base_window_pos, ImGuiCond_Always);
                imgui::SetNextWindowSize(imgui::GetWindowContentRegionMax(), ImGuiCond_Always);
            }
            if (imgui::BeginPopupModal("Bookmarks", nullptr, ImGuiWindowFlags_NoResize)) {
                static pinned_path *s_context_target = nullptr;
                auto [open_target, close_btn] = render_pinned(s_context_target, true);

                if (imgui::IsKeyPressed(ImGuiKey_Escape) || close_btn || open_target) {
                    imgui::CloseCurrentPopup();
                    imgui::ClearNavFocus();
                }

                if (open_target) {
                    if (!directory_exists(open_target->path.data())) {
                        std::string action = make_str("Open pin [%s].", open_target->label.c_str());
                        std::string failed = make_str("Pin path [%s] does not exit.", open_target->path.data());
                        swan_popup_modals::open_error(action.c_str(), failed.c_str());
                    }
                    else {
                        search_directory.path_utf8 = open_target->path;
                        finder.focus_search_value_input = true;
                    }
                }
                imgui::EndPopup();
            }

            if (path_changed) {
                wchar_t path_utf16[MAX_PATH];
                if (utf8_to_utf16(search_directory.path_utf8.data(), path_utf16, lengthof(path_utf16))) {
                    search_directory.found = PathIsDirectoryW(path_utf16);
                }
            }
        }
    }

    bool search_active = finder.search_task.active_token.load();
    [[maybe_unused]] bool search_cancelled = finder.search_task.cancellation_token.load();

    {
        if (search_active) {
            if (imgui::Button(ICON_LC_SEARCH_X "## finder")) {
                finder.search_task.cancellation_token.store(true);
            }
        } else {
            bool search_value_empty = cstr_empty(finder.search_value.data());
            bool any_search_dirs_not_found = std::any_of(finder.search_directories.begin(), finder.search_directories.end(),
                [](finder_window::search_directory const &sd) { return !sd.found; });

            imgui::ScopedDisable d(search_value_empty || any_search_dirs_not_found);

            if (imgui::Button(ICON_LC_SEARCH "## finder")) {
                finder.search_task.result.clear();
                finder.num_entries_checked.store(0);

                swan_finder::g_thread_pool.push_task([&finder]() {
                    search_proc(std::ref(finder.search_task), finder.search_directories, std::ref(finder.num_entries_checked), finder.search_value);
                });
            }
        }
    }

    imgui::SameLine();

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
            finder.search_task.result.clear();
            finder.num_entries_checked.store(0);

            swan_finder::g_thread_pool.push_task([&finder]() {
                search_proc(std::ref(finder.search_task), finder.search_directories, std::ref(finder.num_entries_checked), finder.search_value);
            });
        }
    }

    imgui::SameLine();

    {
        imgui::ScopedStyle<f32> s(imgui::GetStyle().Alpha, false ? 1 : imgui::GetStyle().DisabledAlpha);

        if (imgui::Button(ICON_CI_CASE_SENSITIVE)) {
            // TODO
            // flip_bool(expl.filter_case_sensitive);
            // (void) expl.update_cwd_entries(filter, expl.cwd.data());
            // (void) expl.save_to_disk();
        }
    }
    if (imgui::IsItemHovered()) {
        // imgui::SetTooltip("Case sensitive: %s\n", expl.filter_case_sensitive ? "ON" : "OFF");
    }

    {
        u64 num_entries_checked = finder.num_entries_checked.load();
        if (num_entries_checked > 0) {
            imgui::SameLineSpaced(1);
            u64 num_matches = 0;
            {
                std::scoped_lock lock(finder.search_task.result_mutex);
                num_matches = finder.search_task.result.size();
            }
            imgui::Text("%zu of %zu (%.2lf %%) entries matched", num_matches, num_entries_checked, (f64(num_matches) / f64(num_entries_checked) * 100.0));
        }
    }

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
            imgui::TableSetupColumn("Name", ImGuiTableColumnFlags_DefaultSort|ImGuiTableColumnFlags_NoHide, 0.0f, matches_table_col_name);
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
                    auto label = make_str_static<2048>("%s ## %zu", file_name, m.basic.id);

                    if (imgui::Selectable(label.data(), false, ImGuiSelectableFlags_SpanAllColumns|ImGuiSelectableFlags_AllowDoubleClick)) {
                        if (imgui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                            (void) find_in_swan_explorer_0(m.basic.path.data());
                        }
                    }

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

    return true;
}
