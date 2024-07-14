#include "stdafx.hpp"
#include "data_types.hpp"
#include "common_functions.hpp"
#include "imgui_dependent_functions.hpp"
#include "path.hpp"

static std::deque<recent_file> g_recent_files(global_constants::MAX_RECENT_FILES);
static std::mutex g_recent_files_mutex = {};

global_state::recent_files global_state::recent_files_get() noexcept { return { &g_recent_files, &g_recent_files_mutex }; }

void erase(global_state::recent_files &obj,
           std::deque<recent_file>::iterator first,
           std::deque<recent_file>::iterator last,
           bool perform_delete_icon_texture) noexcept
{
    if (perform_delete_icon_texture) {
        for (auto iter = first; iter != last; ++iter) {
            if (iter->icon_GLtexID > 0) delete_icon_texture(iter->icon_GLtexID, "recent_file");
        }
    }
    obj.container->erase(first, last);
}

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
    auto recent_files = global_state::recent_files_get();

    std::scoped_lock lock(*recent_files.mutex);

    auto temp = recent_files.container->operator[](recent_file_idx);
    temp.action_time = get_time_system();
    if (new_action) {
        temp.action.clear();
        temp.action = new_action;
    }
    auto delete_iter = g_recent_files.begin() + recent_file_idx;
    erase(recent_files, delete_iter, delete_iter + 1, false);
    recent_files.container->push_front(temp);
}

void global_state::recent_files_add(char const *action, char const *full_file_path) noexcept
{
    swan_path path = path_create(full_file_path);

    auto recent_files = global_state::recent_files_get();

    std::scoped_lock lock(*recent_files.mutex);

    if (recent_files.container->size() >= global_constants::MAX_RECENT_FILES) {
        erase(recent_files, recent_files.container->begin() + global_constants::MAX_RECENT_FILES, recent_files.container->end());
    }
    g_recent_files.emplace_front(action, get_time_system(), 0, ImVec2(), path, false);
}

void global_state::recent_files_remove(u64 recent_file_idx) noexcept
{
    auto recent_files = global_state::recent_files_get();

    std::scoped_lock lock(*recent_files.mutex);

    auto elem_iter = recent_files.container->begin() + recent_file_idx;
    erase(recent_files, elem_iter, elem_iter + 1);
}

bool global_state::recent_files_save_to_disk(std::scoped_lock<std::mutex> *supplied_lock) noexcept
try {
    std::filesystem::path full_path = global_state::execution_path() / "data\\recent_files.txt";

    std::ofstream iss(full_path);

    if (!iss) {
        return false;
    }

    auto lock = supplied_lock ? std::unique_lock<std::mutex>() : std::unique_lock<std::mutex>(g_recent_files_mutex);

    for (auto const &file : g_recent_files) {
        iss << file.action.size() << ' '
            << file.action.c_str() << ' '
            << std::chrono::system_clock::to_time_t(file.action_time) << ' '
            << path_length(file.path) << ' '
            << file.path.data() << '\n';
    }

    print_debug_msg("SUCCESS");
    return true;
}
catch (std::exception const &except) {
    print_debug_msg("FAILED catch(std::exception) %s", except.what());
    return false;
}
catch (...) {
    print_debug_msg("FAILED catch(...)");
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

        char stored_action[recent_file::ACTION_MAX_LEN + 1] = {};
        iss.read(stored_action, std::min(stored_action_len, lengthof(stored_action) - 1));
        iss.ignore(1);

        time_point_system_t stored_time = extract_system_time_from_istream(iss);
        iss.ignore(1);

        iss >> (u64 &)stored_path_len;
        iss.ignore(1);

        iss.read(stored_path.data(), stored_path_len);

        path_force_separator(stored_path, dir_separator);

        g_recent_files.emplace_back(stored_action, stored_time, 0, ImVec2(), stored_path, false);

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

u64 deselect_all(std::deque<recent_file> &recent_files) noexcept
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

bool swan_windows::render_recent_files(bool &open, bool any_popups_open) noexcept
{
    if (!imgui::Begin(swan_windows::get_name(swan_windows::id::recent_files), &open)) {
        return false;
    }

    std::string dummy_buf = {};
    bool search_text_edited;
    {
        imgui::ScopedDisable d(true);
        imgui::ScopedItemWidth w(imgui::CalcTextSize("123456789_123456789_123456789_").x);
        search_text_edited = imgui::InputTextWithHint("## recent_files search", ICON_CI_SEARCH " TODO", &dummy_buf);
    }

    imgui::SameLineSpaced(1);

    {
        auto help = render_help_indicator(true);

        if (help.hovered && imgui::BeginTooltip()) {
            imgui::AlignTextToFramePadding();
            imgui::TextUnformatted("[Recent Files] Help");
            imgui::Separator();

            imgui::TextUnformatted("- Double click a file to open");
            imgui::TextUnformatted("- Right click a file for context menu");
            imgui::TextUnformatted("- Hold Shift + Hover File Name to see full path");

            imgui::EndTooltip();
        }
    }

    imgui::SameLineSpaced(0);

    {
        auto recent_files = global_state::recent_files_get();

        imgui::ScopedDisable d(g_recent_files.empty());

        if (imgui::Button(ICON_CI_CLEAR_ALL "## recent_files")) {
            char const *confirmation_msg = "Are you sure you want to clear ALL recent files? This action cannot be undone.";
            imgui::OpenConfirmationModal(swan_id_confirm_recent_files_clear, confirmation_msg, &(global_state::settings().confirm_recent_files_clear));
        }
        if (imgui::IsItemHovered()) imgui::SetTooltip("Clear %zu records", recent_files.container->size());

        auto status = imgui::GetConfirmationStatus(swan_id_confirm_recent_files_clear);

        if (status.value_or(false)) {
            std::scoped_lock lock(*recent_files.mutex);
            erase(recent_files, recent_files.container->begin(), recent_files.container->end());
            (void) global_state::recent_files_save_to_disk(&lock);
        }
    }

    imgui::Separator();

    static recent_file *s_context_menu_target = nullptr;
    static u64 s_context_menu_target_idx = u64(-1);
    static std::optional<ImRect> s_context_menu_target_rect = std::nullopt;
    static u64 s_latest_selected_row_idx = u64(-1);
    static u64 s_num_selected_when_context_menu_opened = 0;

    auto &io = imgui::GetIO();
    bool window_hovered = imgui::IsWindowHovered(ImGuiFocusedFlags_ChildWindows);
    u64 move_to_front_idx = u64(-1);
    u64 remove_idx = u64(-1);
    bool execute_forget_selection_immediately = false;
    time_point_system_t current_time = get_time_system();

    // handle keybind actions
    if (!any_popups_open && window_hovered) {
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
        (global_state::settings().tables_alt_row_bg ? ImGuiTableFlags_RowBg : 0)|
        (global_state::settings().table_borders_in_body ? 0 : ImGuiTableFlags_NoBordersInBody)
    ;

    std::optional<ImRect> file_name_rect = std::nullopt;

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
            char *file_name = path_find_filename(full_path);
            auto directory = path_extract_location(full_path);
            swan_path file_directory = path_create(directory.data(), directory.size());

            bool right_clicked = false;
            bool double_clicked = false;

            imgui::TableNextRow();

            if (imgui::TableSetColumnIndex(recent_files_table_col_number)) {
                imgui::Text("%zu", i+1);
            }

            if (imgui::TableSetColumnIndex(recent_files_table_col_when)) {
                auto when = time_diff_str(file.action_time, current_time);
                imgui::Text("%s %s", file.action.c_str(), when.data());
            }

            if (imgui::TableSetColumnIndex(recent_files_table_col_file_name)) {
                static ImVec2 s_last_known_icon_size = {};

                if (global_state::settings().win32_file_icons) {
                    if (file.icon_GLtexID == 0) {
                        std::tie(file.icon_GLtexID, file.icon_size) = load_icon_texture(file.path.data(), 0, "recent_file");
                        if (file.icon_GLtexID > 0) {
                            s_last_known_icon_size = file.icon_size;
                        }
                    }
                    auto const &icon_size = file.icon_GLtexID < 1 ? s_last_known_icon_size : file.icon_size;
                    ImGui::Image((ImTextureID)std::max(file.icon_GLtexID, s64(0)), icon_size);
                }
                else { // fallback to generic icons
                    char const *icon = get_icon(basic_dirent::kind::file);
                    ImVec4 icon_color = get_color(basic_dirent::kind::file);
                    imgui::TextColored(icon_color, icon);
                }
                imgui::SameLine();

                auto label = make_str_static<1200>("%s ## recent_file_%zu", file_name, i);
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
            if (double_clicked) {
                auto res = open_file(file_name, file_directory.data()); // TODO async

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
                s_context_menu_target_rect = imgui::GetItemRect();

                bool keep_any_selected_state = s_context_menu_target->selected;

                for (auto &rf : g_recent_files) {
                    rf.selected = rf.selected && keep_any_selected_state;
                    s_num_selected_when_context_menu_opened += u64(rf.selected);
                }
            }
            if (imgui::TableGetHoveredColumn() == recent_files_table_col_file_name && imgui::IsItemHovered() && io.KeyShift) {
                if (imgui::BeginTooltip()) {
                    render_path_with_stylish_separators(full_path, appropriate_icon(file.icon_GLtexID, basic_dirent::kind::file));
                    imgui::EndTooltip();
                }
            }

            if (imgui::TableSetColumnIndex(recent_files_table_col_location)) {
                std::string_view location = path_extract_location(full_path);
                imgui::TextUnformatted(location.data(), location.data() + location.size());
            }
        }

        if (imgui::IsPopupOpen("## recent_files context_menu")) {
            if (s_num_selected_when_context_menu_opened <= 1) {
                assert(s_context_menu_target_rect.has_value());
                ImVec2 min = s_context_menu_target_rect.value().Min;
                min.x += 1; // to avoid overlap with table left V border
                ImVec2 const &max = s_context_menu_target_rect.value().Max;
                ImGui::GetWindowDrawList()->AddRect(min, max, imgui::ImVec4_to_ImU32(imgui::GetStyleColorVec4(ImGuiCol_NavHighlight), true));
            }
        } else {
            s_context_menu_target = nullptr;
            s_context_menu_target_idx = u64(-1);
            s_context_menu_target_rect = std::nullopt;
        }

        if (imgui::BeginPopup("## recent_files context_menu")) {
            char *full_path = s_context_menu_target->path.data();
            // char *file_name = path_find_filename(full_path);
            auto directory = path_extract_location(full_path);
            swan_path parent_directory = path_create(directory.data(), directory.size());

            {
                imgui::ScopedDisable d(s_num_selected_when_context_menu_opened > 1);

                if (imgui::Selectable("Find")) {
                    swan_path file_directory = path_create(parent_directory.data(), parent_directory.size() - 1);
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
                            (void) find_in_swan_explorer_0(full_path);
                        }
                    }
                }
            }

            if (imgui::Selectable("Reveal in WFE")) {
                if (s_num_selected_when_context_menu_opened == 0) {
                    swan_path const &full_path_ = s_context_menu_target->path;
                    auto res = reveal_in_windows_file_explorer(full_path_);
                    if (!res.success) {
                        std::string action = make_str("Reveal [%s] in Windows File Explorer.", full_path_.data());
                        char const *failed = res.error_or_utf8_path.c_str();
                        swan_popup_modals::open_error(action.c_str(), failed);
                    }
                }
                else {
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

                    if (s_num_selected_when_context_menu_opened < 5) {
                        // don't ask for confirmation for small numbers
                        reveal_selection();
                    }
                    else {
                        std::string confirmation_msg = make_str(
                            "Are you sure you want to reveal %zu files in Windows File Explorer? This will open %zu instances of the Explorer.",
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
                }
            }

            if (imgui::Selectable("Forget")) {
                if (s_num_selected_when_context_menu_opened <= 1) {
                    remove_idx = s_context_menu_target_idx;
                }
                else {
                    execute_forget_selection_immediately = imgui::OpenConfirmationModal(
                        swan_id_confirm_recent_files_forget_selected,
                        make_str("Are you sure you want to forget the %zu selected files? This action cannot be undone.", s_num_selected_when_context_menu_opened).c_str(),
                        &(global_state::settings().confirm_recent_files_forget_selected)
                    );
                }
            }

            if (imgui::BeginMenu("Copy ## recent_files context_menu")) {
                auto compute_clipboard = [&](std::function<std::string_view (recent_file const &)> extract) noexcept
                {
                    std::string clipboard = {};

                    for (u64 i = 0; i < g_recent_files.size(); ++i) {
                        auto const &cfo = g_recent_files[i];
                        if (cfo.selected) {
                            std::string_view copy_content = extract(cfo);
                            clipboard.append(copy_content);
                            clipboard += '\n';
                        }
                    }

                    if (clipboard.ends_with('\n')) clipboard.pop_back();

                    return clipboard;
                };

                if (imgui::Selectable("Name")) {
                    std::string clipboard = compute_clipboard([](recent_file const &rf) noexcept {
                        char const *file_name = path_cfind_filename(rf.path.data());
                        return std::string_view(file_name);
                    });
                    imgui::SetClipboardText(clipboard.c_str());
                }
                if (imgui::Selectable("Location")) {
                    std::string clipboard = compute_clipboard([](recent_file const &rf) noexcept {
                        std::string_view location = path_extract_location(rf.path.data());
                        return location;
                    });
                    imgui::SetClipboardText(clipboard.c_str());
                }
                if (imgui::Selectable("Full path")) {
                    std::string clipboard = compute_clipboard([](recent_file const &rf) noexcept {
                        return std::string_view(rf.path.data());
                    });
                    imgui::SetClipboardText(clipboard.c_str());
                }

                imgui::EndMenu();
            }

            ImVec2 popup_pos = imgui::GetWindowPos();
            ImVec2 popup_size = imgui::GetWindowSize();
            ImRect context_menu_rect = { popup_pos, popup_pos + popup_size };

            imgui::EndPopup();
        }
        else {
            s_num_selected_when_context_menu_opened = 0;
        }

        {
            auto status = imgui::GetConfirmationStatus(swan_id_confirm_recent_files_forget_selected);

            if (execute_forget_selection_immediately || status.value_or(false)) {
                auto recent_files = global_state::recent_files_get();

                auto delete_iter = std::stable_partition(g_recent_files.begin(), g_recent_files.end(),
                    [](recent_file const &rf) noexcept { return !rf.selected; });

                erase(recent_files, delete_iter, g_recent_files.end());

                (void) global_state::recent_files_save_to_disk(&recent_files_lock);
                (void) global_state::settings().save_to_disk(); // persist potential change to confirmation checkbox
            }
        }

        imgui::EndTable();
    }

    if (remove_idx != u64(-1)) {
        (void) global_state::recent_files_remove(remove_idx);
        (void) global_state::recent_files_save_to_disk(nullptr);
    }
    if (move_to_front_idx != u64(-1)) {
        global_state::recent_files_move_to_front(move_to_front_idx, "Opened");
        (void) global_state::recent_files_save_to_disk(nullptr);
    }

    return true;
}
