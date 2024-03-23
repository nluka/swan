#include "stdafx.hpp"
#include "data_types.hpp"
#include "common_functions.hpp"
#include "imgui_dependent_functions.hpp"
#include "path.hpp"

static circular_buffer<recent_file> g_recent_files = circular_buffer<recent_file>(global_constants::MAX_RECENT_FILES);
static std::mutex g_recent_files_mutex = {};

global_state::recent_files global_state::recent_files_get() noexcept { return { &g_recent_files, &g_recent_files_mutex }; }

u64 global_state::recent_files_find_idx(char const *search_path) noexcept
{
    std::scoped_lock lock(g_recent_files_mutex);

    for (u64 i = 0; i < g_recent_files.size(); ++i) {
        auto const &recent_file = g_recent_files[i];
        if (path_loosely_same(recent_file.path.data(), search_path)) {
            return i;
        }
    }

    return u64(-1);
}

void global_state::recent_files_move_to_front(u64 recent_file_idx, char const *new_action) noexcept
{
    std::scoped_lock lock(g_recent_files_mutex);

    auto temp = g_recent_files[recent_file_idx];
    temp.action_time = current_time_system();
    if (new_action) {
        temp.action.clear();
        temp.action = new_action;
    }

    g_recent_files.erase(g_recent_files.begin() + recent_file_idx);
    g_recent_files.push_front(temp);
}

void global_state::recent_files_add(char const *action, char const *full_file_path) noexcept
{
    swan_path path = path_create(full_file_path);

    std::scoped_lock lock(g_recent_files_mutex);
    g_recent_files.push_front({ action, current_time_system(), path });
}

void global_state::recent_files_remove(u64 recent_file_idx) noexcept
{
    std::scoped_lock lock(g_recent_files_mutex);
    g_recent_files.erase(g_recent_files.begin() + recent_file_idx);
}

bool global_state::recent_files_save_to_disk() noexcept
try {
    std::filesystem::path full_path = global_state::execution_path() / "data\\recent_files.txt";

    std::ofstream iss(full_path);

    if (!iss) {
        return false;
    }

    std::scoped_lock lock(g_recent_files_mutex);

    for (auto const &file : g_recent_files) {
        auto time_t = std::chrono::system_clock::to_time_t(file.action_time);
        std::tm tm = *std::localtime(&time_t);

        iss << file.action.size() << ' '
            << file.action.c_str() << ' '
            << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << ' '
            << path_length(file.path) << ' '
            << file.path.data() << '\n';
    }

    print_debug_msg("SUCCESS global_state::recent_files_save_to_disk");
    return true;
}
catch (...) {
    print_debug_msg("FAILED global_state::recent_files_save_to_disk");
    return false;
}

std::pair<bool, u64> global_state::recent_files_load_from_disk(char dir_separator) noexcept
try {
    std::filesystem::path full_path = global_state::execution_path() / "data\\recent_files.txt";

    std::ifstream in(full_path);

    if (!in) {
        return { false, 0 };
    }

    std::scoped_lock lock(g_recent_files_mutex);

    g_recent_files.clear();

    std::string line = {};
    line.reserve(global_state::page_size() - 1);

    u64 num_loaded_successfully = 0;

    while (std::getline(in, line)) {
        std::istringstream iss(line);

        u64 stored_action_len = 0;
        u64 stored_path_len = 0;
        swan_path stored_path = {};

        iss >> stored_action_len;
        iss.ignore(1);

        char buffer[recent_file::ACTION_MAX_LEN + 1] = {};
        iss.read(buffer, std::min(stored_action_len, lengthof(buffer) - 1));
        iss.ignore(1);

        std::tm tm = {};
        iss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
        system_time_point_t stored_time = std::chrono::system_clock::from_time_t(std::mktime(&tm));
        iss.ignore(1);

        iss >> (u64 &)stored_path_len;
        iss.ignore(1);

        iss.read(stored_path.data(), stored_path_len);

        path_force_separator(stored_path, dir_separator);

        g_recent_files.push_back();
        g_recent_files.back().action = buffer;
        g_recent_files.back().action_time = stored_time;
        g_recent_files.back().path = stored_path;

        ++num_loaded_successfully;

        line.clear();
    }

    print_debug_msg("SUCCESS global_state::recent_files_load_from_disk, loaded %zu files", num_loaded_successfully);
    return { true, num_loaded_successfully };
}
catch (...) {
    print_debug_msg("FAILED global_state::recent_files_load_from_disk");
    return { false, 0 };
}

u64 deselect_all(circular_buffer<recent_file> &recent_files) noexcept
{
    u64 num_deselected = 0;

    for (auto &rf : recent_files) {
        bool prev = rf.selected;
        bool &curr = rf.selected;
        curr = false;
        num_deselected += curr != prev;
    }

    return num_deselected;
}

void swan_windows::render_recent_files(bool &open) noexcept
{
    if (!imgui::Begin(swan_windows::get_name(swan_windows::id::recent_files), &open)) {
        imgui::End();
        return;
    }

    if (imgui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)) {
        global_state::focused_window_set(swan_windows::id::recent_files);
    }

    imgui::TextUnformatted("(?)");
    if (imgui::IsItemHovered()) {
        imgui::SetTooltip("Newly created and recently opened files are tracked here.\n");
    }

    imgui::SameLine();

    {
        imgui::ScopedDisable d(g_recent_files.empty());

        if (imgui::SmallButton("Clear##recent_files")) {
            char const *confirmation_msg = "Are you sure you want to clear your recent files? This action cannot be undone.";
            imgui::OpenConfirmationModal(swan_id_confirm_recent_files_clear, confirmation_msg, &(global_state::settings().confirm_recent_files_clear));
        }

        auto status = imgui::GetConfirmationStatus(swan_id_confirm_recent_files_clear);

        if (status.value_or(false)) {
            std::scoped_lock lock(g_recent_files_mutex);
            g_recent_files.clear();
            (void) global_state::recent_files_save_to_disk();
        }
    }

    static recent_file *s_context_menu_target = nullptr;
    static u64 s_context_menu_target_idx = u64(-1);
    static u64 s_latest_selected_row_idx = u64(-1);
    static u64 s_num_selected_when_context_menu_opened = 0;

    auto &io = imgui::GetIO();
    bool window_hovered = imgui::IsWindowHovered(ImGuiFocusedFlags_ChildWindows);
    bool any_popup_modals_open = global_state::popup_modals_are_any_open();
    u64 move_to_front_idx = u64(-1);
    u64 remove_idx = u64(-1);
    bool execute_forget_selection_immediately = false;
    system_time_point_t current_time = current_time_system();

    // handle keybind actions
    if (!any_popup_modals_open && window_hovered) {
        if (imgui::IsKeyPressed(ImGuiKey_Escape)) {
            std::scoped_lock recent_files_lock(g_recent_files_mutex);
            deselect_all(g_recent_files);
        }
        else if (io.KeyCtrl && imgui::IsKeyPressed(ImGuiKey_A)) {
            std::scoped_lock recent_files_lock(g_recent_files_mutex);
            for (auto &rf : g_recent_files) {
                rf.selected = true;
            }
        }
        else if (io.KeyCtrl && imgui::IsKeyPressed(ImGuiKey_I)) {
            std::scoped_lock recent_files_lock(g_recent_files_mutex);
            for (auto &rf : g_recent_files) {
                flip_bool(rf.selected);
            }
        }
    }

    enum recent_files_table_col : s32 {
        recent_files_table_col_number,
        recent_files_table_col_when,
        recent_files_table_col_file_name,
        recent_files_table_col_location,
        recent_files_table_col_count
    };

    s32 table_flags =
        ImGuiTableFlags_SizingStretchProp|
        ImGuiTableFlags_BordersV|
        ImGuiTableFlags_Hideable|
        ImGuiTableFlags_Reorderable|
        ImGuiTableFlags_Resizable|
        ImGuiTableFlags_ScrollY|
        (global_state::settings().explorer_cwd_entries_table_alt_row_bg ? ImGuiTableFlags_RowBg : 0)
    ;

    if (imgui::BeginTable("recent_files", recent_files_table_col_count, table_flags)) {
        imgui::TableSetupColumn("#", ImGuiTableColumnFlags_NoSort);
        imgui::TableSetupColumn("When");
        imgui::TableSetupColumn("File Name", ImGuiTableColumnFlags_NoHide);
        imgui::TableSetupColumn("Location");
        ImGui::TableSetupScrollFreeze(0, 1);
        imgui::TableHeadersRow();

        std::scoped_lock recent_files_lock(g_recent_files_mutex);

        ImGuiListClipper clipper;
        assert(g_recent_files.size() <= (u64)INT32_MAX);
        clipper.Begin((s32)g_recent_files.size());

        while (clipper.Step())
        for (u64 i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
            auto &file = g_recent_files[i];
            char *full_path = file.path.data();
            char *file_name = get_file_name(full_path);
            auto directory = get_everything_minus_file_name(full_path);
            swan_path file_directory = path_create(directory.data(), directory.size());

            bool right_clicked = false;
            bool double_clicked = false;

            imgui::TableNextRow();

            if (imgui::TableSetColumnIndex(recent_files_table_col_number)) {
                imgui::Text("%zu", i+1);
            }

            if (imgui::TableSetColumnIndex(recent_files_table_col_when)) {
                auto when = compute_when_str(file.action_time, current_time);
                imgui::Text("%s %s", file.action.c_str(), when.data());
            }

            if (imgui::TableSetColumnIndex(recent_files_table_col_file_name)) {
                {
                    file_name_extension_splitter splitter(file_name);
                    imgui::TextColored(get_color(basic_dirent::kind::file), get_icon_for_extension(splitter.ext));
                }

                imgui::SameLine();

                auto label = make_str_static<1200>("%s##recent_file_%zu", file_name, i);
                if (imgui::Selectable(label.data(), file.selected, ImGuiSelectableFlags_SpanAllColumns|ImGuiSelectableFlags_AllowDoubleClick)) {
                    bool selection_state_before_activate = file.selected;

                    u64 num_deselected = 0;
                    if (!io.KeyCtrl && !io.KeyShift) {
                        // entry was selected but Ctrl was not held, so deselect everything
                        num_deselected = deselect_all(g_recent_files);
                    }

                    if (num_deselected > 1) {
                        file.selected = true;
                    } else {
                        file.selected = !selection_state_before_activate;
                    }

                    if (io.KeyShift) {
                        auto [first_idx, last_idx] = imgui::SelectRange(s_latest_selected_row_idx, i);
                        s_latest_selected_row_idx = last_idx;
                        for (u64 j = first_idx; j <= last_idx; ++j) {
                            g_recent_files[j].selected = true;
                        }
                    } else {
                        s_latest_selected_row_idx = i;
                    }

                    double_clicked |= imgui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
                }
                right_clicked |= imgui::IsItemClicked(ImGuiMouseButton_Right);
            }

            if (imgui::TableSetColumnIndex(recent_files_table_col_location)) {
                std::string_view location = get_everything_minus_file_name(full_path);
                imgui::TextUnformatted(location.data(), location.data() + location.size());
            }

            if (double_clicked) {
                auto res = open_file(file_name, file_directory.data());

                if (res.success) {
                    move_to_front_idx = i;
                } else {
                    swan_popup_modals::open_error(make_str("Open file [%s].", full_path).c_str(), res.error_or_utf8_path.c_str());
                    remove_idx = i;
                }
            }
            if (right_clicked) {
                imgui::OpenPopup("## recent_files context_menu");
                s_context_menu_target = &g_recent_files[i];
                s_context_menu_target_idx = i;
                for (auto const &rf : g_recent_files) {
                    s_num_selected_when_context_menu_opened += u64(rf.selected);
                }
            }
        }

        if (imgui::BeginPopup("## recent_files context_menu")) {
            if (imgui::Selectable("Open file location")) {
                auto &expl = global_state::explorers()[0];
                char *full_path = s_context_menu_target->path.data();
                char *file_name = get_file_name(full_path);
                auto directory = get_everything_minus_file_name(full_path);
                swan_path file_directory = path_create(directory.data(), directory.size());

                wchar_t file_path_utf16[MAX_PATH];

                if (utf8_to_utf16(full_path, file_path_utf16, lengthof(file_path_utf16))) {
                    auto file_exists = PathFileExistsW(file_path_utf16);

                    if (!file_exists) {
                        std::string action = make_str("Open file location [%s].", full_path);
                        char const *failure = "File not found.";
                        swan_popup_modals::open_error(action.c_str(), failure);
                        remove_idx = s_context_menu_target_idx;
                    }
                    else {
                        expl.deselect_all_cwd_entries();
                        {
                            std::scoped_lock lock2(expl.select_cwd_entries_on_next_update_mutex);
                            expl.select_cwd_entries_on_next_update.clear();
                            expl.select_cwd_entries_on_next_update.push_back(path_create(file_name));
                        }

                        auto [file_directory_exists, _] = expl.update_cwd_entries(full_refresh, file_directory.data());

                        if (!file_directory_exists) {
                            std::string action = make_str("Open file location [%s].", full_path);
                            std::string failure = make_str("Directory [%s] not found.", file_directory.data());
                            swan_popup_modals::open_error(action.c_str(), failure.c_str());
                            remove_idx = s_context_menu_target_idx;
                        }
                        else {
                            expl.cwd = path_create(file_directory.data());

                            if (!path_loosely_same(expl.cwd, expl.latest_valid_cwd)) {
                                expl.push_history_item(expl.cwd);
                            }

                            expl.latest_valid_cwd = expl.cwd;
                            expl.scroll_to_nth_selected_entry_next_frame = 0;
                            (void) expl.save_to_disk();

                            global_state::settings().show.explorer_0 = true;
                            (void) global_state::settings().save_to_disk();

                            imgui::SetWindowFocus(expl.name);
                        }
                    }
                }
            }

            if (imgui::Selectable("Reveal in File Explorer")) {
                swan_path const &full_path = s_context_menu_target->path;
                auto res = reveal_in_windows_file_explorer(full_path);
                if (!res.success) {
                    std::string action = make_str("Reveal [%s] in File Explorer.", full_path.data());
                    char const *failed = res.error_or_utf8_path.c_str();
                    swan_popup_modals::open_error(action.c_str(), failed);
                }
            }

            {
                bool disabled = s_num_selected_when_context_menu_opened == 0;
                {
                    imgui::ScopedDisable d(disabled);

                    if (imgui::Selectable("Reveal selection in File Explorer")) {
                        auto reveal_selection = [&]() noexcept {
                            // std::scoped_lock lock(g_recent_files_mutex);

                            for (auto const &file : g_recent_files) {
                                if (file.selected) {
                                    swan_path const &full_path = file.path;
                                    auto res = reveal_in_windows_file_explorer(full_path);
                                    // TODO: report errors as notifications instead of modal
                                    if (!res.success) {
                                        std::string action = make_str("Reveal [%s] in File Explorer.", full_path.data());
                                        char const *failed = res.error_or_utf8_path.c_str();
                                        swan_popup_modals::open_error(action.c_str(), failed);
                                        break;
                                    }
                                }
                            }
                        };

                        if (s_num_selected_when_context_menu_opened > 5) {
                            std::string confirmation_msg = make_str(
                                "Are you sure you want to reveal the %zu selected files in Windows File Explorer? This will open %zu instances of the Explorer.",
                                s_num_selected_when_context_menu_opened, s_num_selected_when_context_menu_opened
                            );

                            imgui::OpenConfirmationModalWithCallback(
                                /* confirmation_id      = */ swan_id_confirm_recent_files_reveal_selected_in_win_file_expl,
                                /* confirmation_msg     = */ confirmation_msg.c_str(),
                                /* on_yes_callback      = */
                                [&reveal_selection]() noexcept {
                                    reveal_selection();
                                    (void) global_state::settings().save_to_disk();
                                },
                                /* confirmation_enabled = */ &(global_state::settings().confirm_recent_files_reveal_selected_in_win_file_expl)
                            );
                        }
                        else {
                            reveal_selection();
                        }
                    }
                }
                if (disabled) {
                    imgui::SameLine();
                    imgui::TextDisabled("(?)");
                    if (imgui::IsItemHovered()) {
                        imgui::SetTooltip("Select at least one record.");
                    }
                }
            }

            imgui::Separator();

            if (imgui::Selectable("Forget this one")) {
                remove_idx = s_context_menu_target_idx;
            }

            {
                bool disabled = s_num_selected_when_context_menu_opened == 0;
                {
                    imgui::ScopedDisable d(disabled);

                    if (imgui::Selectable("Forget selection")) {
                        execute_forget_selection_immediately = imgui::OpenConfirmationModal(
                            swan_id_confirm_recent_files_forget_selected,
                            make_str("Are you sure you want to forget the %zu selected files? This action cannot be undone.", s_num_selected_when_context_menu_opened).c_str(),
                            &(global_state::settings().confirm_recent_files_forget_selected)
                        );
                    }
                }
                if (disabled) {
                    imgui::SameLine();
                    imgui::TextDisabled("(?)");
                    if (imgui::IsItemHovered()) {
                        imgui::SetTooltip("Select at least one record.");
                    }
                }
            }

            imgui::EndPopup();
        }
        else {
            s_num_selected_when_context_menu_opened = 0;
        }

        {
            auto status = imgui::GetConfirmationStatus(swan_id_confirm_recent_files_forget_selected);

            if (execute_forget_selection_immediately || status.value_or(false)) {
                auto not_selected_end_iter = std::remove_if(g_recent_files.begin(), g_recent_files.end(),
                                                            [](recent_file const &rf) noexcept { return rf.selected; });

                g_recent_files.erase(not_selected_end_iter, g_recent_files.end());

                (void) global_state::completed_file_operations_save_to_disk(&recent_files_lock);
                (void) global_state::settings().save_to_disk(); // persist potential change to confirmation checkbox
            }
        }

        imgui::EndTable();
    }

    if (remove_idx != u64(-1)) {
        (void) global_state::recent_files_remove(remove_idx);
        (void) global_state::recent_files_save_to_disk();
    }
    if (move_to_front_idx != u64(-1)) {
        global_state::recent_files_move_to_front(move_to_front_idx, "Opened");
        (void) global_state::recent_files_save_to_disk();
    }

    imgui::End();
}
