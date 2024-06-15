#include "stdafx.hpp"
#include "common_functions.hpp"
#include "imgui_dependent_functions.hpp"
#include "path.hpp"

static swan_settings g_settings = {};
swan_settings &global_state::settings() noexcept { return g_settings; }

bool swan_windows::render_settings(bool &open, [[maybe_unused]] bool any_popups_open, bool &out_changes_applied) noexcept
{
    static bool s_regular_change = false;
    static bool s_overridden = false;

    if (!imgui::Begin(swan_windows::get_name(swan_windows::id::settings), &open, ImGuiWindowFlags_AlwaysAutoResize)) {
        return false;
    }

    auto &settings = global_state::settings();

    s_regular_change |= imgui::Checkbox("Start with window maximized", &settings.startup_with_window_maximized);
    s_regular_change |= imgui::Checkbox("Start with previous window size & pos", &settings.startup_with_previous_window_pos_and_size);

    {
        imgui::Separator();
        imgui::AlignTextToFramePadding();
        imgui::TextUnformatted("Override window pos & size");
        if (s_overridden) {
            imgui::SameLine();
            if (imgui::Button("Apply")) {
                s_overridden = false;
                out_changes_applied = true;
            }
        }
    #if 1
        imgui::ScopedItemWidth w(200.f);
        s_overridden |= imgui::InputInt2("Window position (x, y)", &settings.window_x);
        s_overridden |= imgui::InputInt2("Window size (w, h)", &settings.window_w);
    #else
        imgui::ScopedItemWidth w(150.f);
        s_overridden |= imgui::InputInt("Window position x", &settings.window_x);
        s_overridden |= imgui::InputInt("Window position y", &settings.window_y);
        s_overridden |= imgui::InputInt("Window width", &settings.window_w);
        s_overridden |= imgui::InputInt("Window height", &settings.window_h);
    #endif
    }

    if (s_regular_change) {
        s_regular_change = false;
        (void) settings.save_to_disk();
    }

    return true;
}

bool swan_settings::save_to_disk() const noexcept
try {
    std::filesystem::path full_path = global_state::execution_path() / "data\\swan_settings.txt";
    std::ofstream ofs(full_path);
    if (!ofs) {
        return false;
    }

    static_assert(s8(1) == s8(true));
    static_assert(s8(0) == s8(false));

    assert(one_of(this->size_unit_multiplier, { 1000, 1024 }));

    auto write_bool = [&](char const *key, bool val) noexcept {
        ofs << key << ' ' << s32(val) << '\n';
    };

    auto write_ImVec4 = [&](char const *key, ImVec4 const &color) noexcept {
        ofs << key << ' '
            << color.x << ' '
            << color.y << ' '
            << color.z << ' '
            << color.w << '\n';
    };

    ofs << "window_x " << this->window_x << '\n';
    ofs << "window_x " << this->window_x << '\n';
    ofs << "window_y " << this->window_y << '\n';
    ofs << "window_w " << this->window_w << '\n';
    ofs << "window_h " << this->window_h << '\n';
    ofs << "size_unit_multiplier " << this->size_unit_multiplier << '\n';
    ofs << "explorer_refresh_mode " << (s32)this->explorer_refresh_mode << '\n';

    // wchar_t dir_separator_utf16;
    // char dir_separator_utf8;
    //? stored as a bool:
    write_bool("unix_directory_separator", this->dir_separator_utf8 == '/');

    write_bool("show_debug_info", this->show_debug_info);
    write_bool("file_extension_icons", this->file_extension_icons);
    write_bool("tables_alt_row_bg", this->tables_alt_row_bg);
    write_bool("table_borders_in_body", this->table_borders_in_body);

    write_bool("explorer_show_dotdot_dir", this->explorer_show_dotdot_dir);
    write_bool("explorer_clear_filter_on_cwd_change", this->explorer_clear_filter_on_cwd_change);

    write_bool("file_operations_src_path_full", this->file_operations_src_path_full);
    write_bool("file_operations_dst_path_full", this->file_operations_dst_path_full);

    write_bool("startup_with_window_maximized", this->startup_with_window_maximized);
    write_bool("startup_with_previous_window_pos_and_size", this->startup_with_previous_window_pos_and_size);

    write_bool("confirm.explorer_delete_via_keybind", this->confirm_explorer_delete_via_keybind);
    write_bool("confirm.explorer_delete_via_context_menu", this->confirm_explorer_delete_via_context_menu);
    write_bool("confirm.explorer_unpin_directory", this->confirm_explorer_unpin_directory);
    write_bool("confirm.recent_files_clear", this->confirm_recent_files_clear);
    write_bool("confirm.recent_files_reveal_selected_in_win_file_expl", this->confirm_recent_files_reveal_selected_in_win_file_expl);
    write_bool("confirm.recent_files_forget_selected", this->confirm_recent_files_forget_selected);
    write_bool("confirm.delete_pin", this->confirm_delete_pin);
    write_bool("confirm.completed_file_operations_forget", this->confirm_completed_file_operations_forget);
    write_bool("confirm.completed_file_operations_forget_group", this->confirm_completed_file_operations_forget_group);
    write_bool("confirm.completed_file_operations_forget_all", this->confirm_completed_file_operations_forget_all);
    write_bool("confirm.theme_editor_color_reset", this->confirm_theme_editor_color_reset);
    write_bool("confirm.theme_editor_style_reset", this->confirm_theme_editor_style_reset);

    write_bool("show.explorer_0", this->show.explorer_0);
    write_bool("show.explorer_1", this->show.explorer_1);
    write_bool("show.explorer_2", this->show.explorer_2);
    write_bool("show.explorer_3", this->show.explorer_3);
    write_bool("show.explorer_0_debug", this->show.explorer_0_debug);
    write_bool("show.explorer_1_debug", this->show.explorer_1_debug);
    write_bool("show.explorer_2_debug", this->show.explorer_2_debug);
    write_bool("show.explorer_3_debug", this->show.explorer_3_debug);
    write_bool("show.finder", this->show.finder);
    write_bool("show.pinned", this->show.pinned);
    write_bool("show.file_operations", this->show.file_operations);
    write_bool("show.recent_files", this->show.recent_files);
    write_bool("show.analytics", this->show.analytics);
    write_bool("show.settings", this->show.settings);
    write_bool("show.debug_log", this->show.debug_log);
    write_bool("show.imgui_demo", this->show.imgui_demo);
    write_bool("show.theme_editor", this->show.theme_editor);
    write_bool("show.icon_library", this->show.icon_library);
    write_bool("show.fa_icons", this->show.fa_icons);
    write_bool("show.ci_icons", this->show.ci_icons);
    write_bool("show.md_icons", this->show.md_icons);

    write_ImVec4("color.success", this->success_color);
    write_ImVec4("color.warning", this->warning_color);
    write_ImVec4("color.warning_lite", this->warning_lite_color);
    write_ImVec4("color.error", this->error_color);
    write_ImVec4("color.directory", this->directory_color);
    write_ImVec4("color.file", this->file_color);
    write_ImVec4("color.symlink", this->symlink_color);

    write_bool("check.success_color", this->check_success_color);
    write_bool("check.warning_color", this->check_warning_color);
    write_bool("check.warning_lite_color", this->check_warning_lite_color);
    write_bool("check.error_color", this->check_error_color);
    write_bool("check.directory_color", this->check_directory_color);
    write_bool("check.file_color", this->check_file_color);
    write_bool("check.symlink_color", this->check_symlink_color);

    write_bool("check.ImGuiCol_Text", this->checks_ImGuiCol[ImGuiCol_Text]);
    write_bool("check.ImGuiCol_TextDisabled", this->checks_ImGuiCol[ImGuiCol_TextDisabled]);
    write_bool("check.ImGuiCol_WindowBg", this->checks_ImGuiCol[ImGuiCol_WindowBg]);
    write_bool("check.ImGuiCol_ChildBg", this->checks_ImGuiCol[ImGuiCol_ChildBg]);
    write_bool("check.ImGuiCol_PopupBg", this->checks_ImGuiCol[ImGuiCol_PopupBg]);
    write_bool("check.ImGuiCol_Border", this->checks_ImGuiCol[ImGuiCol_Border]);
    write_bool("check.ImGuiCol_BorderShadow", this->checks_ImGuiCol[ImGuiCol_BorderShadow]);
    write_bool("check.ImGuiCol_FrameBg", this->checks_ImGuiCol[ImGuiCol_FrameBg]);
    write_bool("check.ImGuiCol_FrameBgHovered", this->checks_ImGuiCol[ImGuiCol_FrameBgHovered]);
    write_bool("check.ImGuiCol_FrameBgActive", this->checks_ImGuiCol[ImGuiCol_FrameBgActive]);
    write_bool("check.ImGuiCol_TitleBg", this->checks_ImGuiCol[ImGuiCol_TitleBg]);
    write_bool("check.ImGuiCol_TitleBgActive", this->checks_ImGuiCol[ImGuiCol_TitleBgActive]);
    write_bool("check.ImGuiCol_TitleBgCollapsed", this->checks_ImGuiCol[ImGuiCol_TitleBgCollapsed]);
    write_bool("check.ImGuiCol_MenuBarBg", this->checks_ImGuiCol[ImGuiCol_MenuBarBg]);
    write_bool("check.ImGuiCol_ScrollbarBg", this->checks_ImGuiCol[ImGuiCol_ScrollbarBg]);
    write_bool("check.ImGuiCol_ScrollbarGrab", this->checks_ImGuiCol[ImGuiCol_ScrollbarGrab]);
    write_bool("check.ImGuiCol_ScrollbarGrabHovered", this->checks_ImGuiCol[ImGuiCol_ScrollbarGrabHovered]);
    write_bool("check.ImGuiCol_ScrollbarGrabActive", this->checks_ImGuiCol[ImGuiCol_ScrollbarGrabActive]);
    write_bool("check.ImGuiCol_CheckMark", this->checks_ImGuiCol[ImGuiCol_CheckMark]);
    write_bool("check.ImGuiCol_SliderGrab", this->checks_ImGuiCol[ImGuiCol_SliderGrab]);
    write_bool("check.ImGuiCol_SliderGrabActive", this->checks_ImGuiCol[ImGuiCol_SliderGrabActive]);
    write_bool("check.ImGuiCol_Button", this->checks_ImGuiCol[ImGuiCol_Button]);
    write_bool("check.ImGuiCol_ButtonHovered", this->checks_ImGuiCol[ImGuiCol_ButtonHovered]);
    write_bool("check.ImGuiCol_ButtonActive", this->checks_ImGuiCol[ImGuiCol_ButtonActive]);
    write_bool("check.ImGuiCol_Header", this->checks_ImGuiCol[ImGuiCol_Header]);
    write_bool("check.ImGuiCol_HeaderHovered", this->checks_ImGuiCol[ImGuiCol_HeaderHovered]);
    write_bool("check.ImGuiCol_HeaderActive", this->checks_ImGuiCol[ImGuiCol_HeaderActive]);
    write_bool("check.ImGuiCol_Separator", this->checks_ImGuiCol[ImGuiCol_Separator]);
    write_bool("check.ImGuiCol_SeparatorHovered", this->checks_ImGuiCol[ImGuiCol_SeparatorHovered]);
    write_bool("check.ImGuiCol_SeparatorActive", this->checks_ImGuiCol[ImGuiCol_SeparatorActive]);
    write_bool("check.ImGuiCol_ResizeGrip", this->checks_ImGuiCol[ImGuiCol_ResizeGrip]);
    write_bool("check.ImGuiCol_ResizeGripHovered", this->checks_ImGuiCol[ImGuiCol_ResizeGripHovered]);
    write_bool("check.ImGuiCol_ResizeGripActive", this->checks_ImGuiCol[ImGuiCol_ResizeGripActive]);
    write_bool("check.ImGuiCol_Tab", this->checks_ImGuiCol[ImGuiCol_Tab]);
    write_bool("check.ImGuiCol_TabHovered", this->checks_ImGuiCol[ImGuiCol_TabHovered]);
    write_bool("check.ImGuiCol_TabActive", this->checks_ImGuiCol[ImGuiCol_TabActive]);
    write_bool("check.ImGuiCol_TabUnfocused", this->checks_ImGuiCol[ImGuiCol_TabUnfocused]);
    write_bool("check.ImGuiCol_TabUnfocusedActive", this->checks_ImGuiCol[ImGuiCol_TabUnfocusedActive]);
    write_bool("check.ImGuiCol_DockingPreview", this->checks_ImGuiCol[ImGuiCol_DockingPreview]);
    write_bool("check.ImGuiCol_DockingEmptyBg", this->checks_ImGuiCol[ImGuiCol_DockingEmptyBg]);
    write_bool("check.ImGuiCol_PlotLines", this->checks_ImGuiCol[ImGuiCol_PlotLines]);
    write_bool("check.ImGuiCol_PlotLinesHovered", this->checks_ImGuiCol[ImGuiCol_PlotLinesHovered]);
    write_bool("check.ImGuiCol_PlotHistogram", this->checks_ImGuiCol[ImGuiCol_PlotHistogram]);
    write_bool("check.ImGuiCol_PlotHistogramHovered", this->checks_ImGuiCol[ImGuiCol_PlotHistogramHovered]);
    write_bool("check.ImGuiCol_TableHeaderBg", this->checks_ImGuiCol[ImGuiCol_TableHeaderBg]);
    write_bool("check.ImGuiCol_TableBorderStrong", this->checks_ImGuiCol[ImGuiCol_TableBorderStrong]);
    write_bool("check.ImGuiCol_TableBorderLight", this->checks_ImGuiCol[ImGuiCol_TableBorderLight]);
    write_bool("check.ImGuiCol_TableRowBg", this->checks_ImGuiCol[ImGuiCol_TableRowBg]);
    write_bool("check.ImGuiCol_TableRowBgAlt", this->checks_ImGuiCol[ImGuiCol_TableRowBgAlt]);
    write_bool("check.ImGuiCol_TextSelectedBg", this->checks_ImGuiCol[ImGuiCol_TextSelectedBg]);
    write_bool("check.ImGuiCol_DragDropTarget", this->checks_ImGuiCol[ImGuiCol_DragDropTarget]);
    write_bool("check.ImGuiCol_NavHighlight", this->checks_ImGuiCol[ImGuiCol_NavHighlight]);
    write_bool("check.ImGuiCol_NavWindowingHighlight", this->checks_ImGuiCol[ImGuiCol_NavWindowingHighlight]);
    write_bool("check.ImGuiCol_NavWindowingDimBg", this->checks_ImGuiCol[ImGuiCol_NavWindowingDimBg]);
    write_bool("check.ImGuiCol_ModalWindowDimBg", this->checks_ImGuiCol[ImGuiCol_ModalWindowDimBg]);

    ofs << serialize_ImGuiStyle(imgui::GetStyle(), 8192, serialize_ImGuiStyle_mode::plain_text);

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

bool swan_settings::load_from_disk() noexcept
try {
    std::filesystem::path full_path = global_state::execution_path() / "data\\swan_settings.txt";
    std::ifstream ifs(full_path);
    if (!ifs) {
        print_debug_msg("FAILED global_state::settings::load_from_disk, !file");
        return false;
    }

    auto content = std::string(std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>());
    while (content.back() == '\n') {
        content.pop_back();
    }

    auto lines = std::string_view(content) | std::ranges::views::split('\n');

    char const *key_pattern = "[a-z0-9_.]{1,}";
    char const *int_or_float = "[0-9]{1,}.?[0-9]{0,}";

    auto key_numerical_pattern = make_str_static<128>("^%s %s$", key_pattern, int_or_float);
    std::regex valid_line_key_numerical(key_numerical_pattern.data(), std::regex_constants::icase);

    auto key_ImVec4_pattern = make_str_static<128>("^%s %s %s %s %s$", key_pattern, int_or_float, int_or_float, int_or_float, int_or_float);
    std::regex valid_line_key_color(key_ImVec4_pattern.data(), std::regex_constants::icase);

    auto &style = imgui::GetStyle();
    std::stringstream ss;
    std::string line_str = {};
    std::string property = {};
    u64 line_num = 0;
    u64 num_lines_parsed = 0;
    u64 num_lines_skipped = 0;

    auto extract_bool = [&]() noexcept -> bool {
        char bool_ch = {};
        ss >> bool_ch;
        return bool_ch == '1';
    };
    auto extract_f32 = [&]() noexcept -> f32 {
        f32 val = {};
        ss >> val;
        return val;
    };
    auto extract_ImGuiDir = [&]() noexcept -> ImGuiDir {
        s32 val = {};
        ss >> val;
        return static_cast<ImGuiDir>(val);
    };
    auto extract_ImVec4 = [&]() noexcept -> ImVec4 {
        f32 x, y, z, w;
        ss >> x >> y >> z >> w;
        return ImVec4(x, y, z, w);
    };

    for (auto const &line : lines) {
        ++line_num;
        line_str = std::string(line.data(), line.size());

        if (line_str.empty()) {
            continue;
        }

        print_debug_msg("Parsing line [%s]", line_str.c_str());

        if (!std::regex_match(line_str, valid_line_key_numerical) &&
            !std::regex_match(line_str, valid_line_key_color))
        {
            print_debug_msg("FAILED line %zu malformed, skipping...", line_num);
            ++num_lines_skipped;
            continue;
        }

        ss.str(""); ss.clear();
        ss << line_str;
        ss >> property;

        if (property.starts_with("confirm.")) {
            std::string_view remainder(property.c_str() + lengthof("confirm"));

            if (remainder == "explorer_delete_via_keybind") {
                this->confirm_explorer_delete_via_keybind = extract_bool();
            }
            else if (remainder == "explorer_delete_via_context_menu") {
                this->confirm_explorer_delete_via_context_menu = extract_bool();
            }
            else if (remainder == "explorer_unpin_directory") {
                this->confirm_explorer_unpin_directory = extract_bool();
            }
            else if (remainder == "recent_files_clear") {
                this->confirm_recent_files_clear = extract_bool();
            }
            else if (remainder == "recent_files_reveal_selected_in_win_file_expl") {
                this->confirm_recent_files_reveal_selected_in_win_file_expl = extract_bool();
            }
            else if (remainder == "recent_files_forget_selected") {
                this->confirm_recent_files_forget_selected = extract_bool();
            }
            else if (remainder == "delete_pin") {
                this->confirm_delete_pin = extract_bool();
            }
            else if (remainder == "completed_file_operations_forget") {
                this->confirm_completed_file_operations_forget = extract_bool();
            }
            else if (remainder == "completed_file_operations_forget_group") {
                this->confirm_completed_file_operations_forget_group = extract_bool();
            }
            else if (remainder == "completed_file_operations_forget_all") {
                this->confirm_completed_file_operations_forget_all = extract_bool();
            }
            else if (remainder == "theme_editor_color_reset") {
                this->confirm_theme_editor_color_reset = extract_bool();
            }
            else if (remainder == "theme_editor_style_reset") {
                this->confirm_theme_editor_style_reset = extract_bool();
            }
            else {
                print_debug_msg("Unknown property [%s] at line %zu, skipping...", property.c_str(), line_num);
                ++num_lines_skipped;
                continue;
            }
        }
        else if (property.starts_with("show.")) {
            std::string_view remainder(property.c_str() + lengthof("show"));

            if (remainder == "pinned") {
                this->show.pinned = extract_bool();
            }
            else if (remainder == "file_operations") {
                this->show.file_operations = extract_bool();
            }
            else if (remainder == "recent_files") {
                this->show.recent_files = extract_bool();
            }
            else if (remainder == "explorer_0") {
                this->show.explorer_0 = extract_bool();
            }
            else if (remainder == "explorer_1") {
                this->show.explorer_1 = extract_bool();
            }
            else if (remainder == "explorer_2") {
                this->show.explorer_2 = extract_bool();
            }
            else if (remainder == "explorer_3") {
                this->show.explorer_3 = extract_bool();
            }
            else if (remainder == "explorer_0_debug") {
                this->show.explorer_0_debug = extract_bool();
            }
            else if (remainder == "explorer_1_debug") {
                this->show.explorer_1_debug = extract_bool();
            }
            else if (remainder == "explorer_2_debug") {
                this->show.explorer_2_debug = extract_bool();
            }
            else if (remainder == "explorer_3_debug") {
                this->show.explorer_3_debug = extract_bool();
            }
            else if (remainder == "finder") {
                this->show.finder = extract_bool();
            }
            else if (remainder == "analytics") {
                this->show.analytics = extract_bool();
            }
            else if (remainder == "debug_log") {
                this->show.debug_log = extract_bool();
            }
            else if (remainder == "settings") {
                this->show.settings = extract_bool();
            }
            else if (remainder == "imgui_demo") {
                this->show.imgui_demo = extract_bool();
            }
            else if (remainder == "theme_editor") {
                this->show.theme_editor = extract_bool();
            }
            else if (remainder == "icon_library") {
                this->show.icon_library = extract_bool();
            }
            else if (remainder == "fa_icons") {
                this->show.fa_icons = extract_bool();
            }
            else if (remainder == "ci_icons") {
                this->show.ci_icons = extract_bool();
            }
            else if (remainder == "md_icons") {
                this->show.md_icons = extract_bool();
            }
            else {
                print_debug_msg("Unknown property [%s] at line %zu, skipping...", property.c_str(), line_num);
                ++num_lines_skipped;
                continue;
            }
        }
        else if (property.starts_with("color.")) {
            std::string_view remainder(property.c_str() + lengthof("color"));

            if (remainder == "success") {
                this->success_color = extract_ImVec4();
            }
            else if (remainder == "warning") {
                this->warning_color = extract_ImVec4();
            }
            else if (remainder == "warning_lite") {
                this->warning_lite_color = extract_ImVec4();
            }
            else if (remainder == "error") {
                this->error_color = extract_ImVec4();
            }
            else if (remainder == "directory") {
                this->directory_color = extract_ImVec4();
            }
            else if (remainder == "file") {
                this->file_color = extract_ImVec4();
            }
            else if (remainder == "symlink") {
                this->symlink_color = extract_ImVec4();
            }
            else {
                print_debug_msg("Unknown property [%s] at line %zu, skipping...", property.c_str(), line_num);
                ++num_lines_skipped;
                continue;
            }
        }
        else if (property.starts_with("style.")) {
            std::string_view remainder(property.c_str() + lengthof("style"));

            if (remainder == "Alpha") {
                style.Alpha = extract_f32();
            }
            else if (remainder == "DisabledAlpha") {
                style.DisabledAlpha = extract_f32();
            }
            else if (remainder == "WindowRounding") {
                style.WindowRounding = extract_f32();
            }
            else if (remainder == "WindowBorderSize") {
                style.WindowBorderSize = extract_f32();
            }
            else if (remainder == "ChildRounding") {
                style.ChildRounding = extract_f32();
            }
            else if (remainder == "ChildBorderSize") {
                style.ChildBorderSize = extract_f32();
            }
            else if (remainder == "PopupRounding") {
                style.PopupRounding = extract_f32();
            }
            else if (remainder == "PopupBorderSize") {
                style.PopupBorderSize = extract_f32();
            }
            else if (remainder == "FrameRounding") {
                style.FrameRounding = extract_f32();
            }
            else if (remainder == "FrameBorderSize") {
                style.FrameBorderSize = extract_f32();
            }
            else if (remainder == "IndentSpacing") {
                style.IndentSpacing = extract_f32();
            }
            else if (remainder == "ColumnsMinSpacing") {
                style.ColumnsMinSpacing = extract_f32();
            }
            else if (remainder == "ScrollbarSize") {
                style.ScrollbarSize = extract_f32();
            }
            else if (remainder == "ScrollbarRounding") {
                style.ScrollbarRounding = extract_f32();
            }
            else if (remainder == "GrabMinSize") {
                style.GrabMinSize = extract_f32();
            }
            else if (remainder == "GrabRounding") {
                style.GrabRounding = extract_f32();
            }
            else if (remainder == "LogSliderDeadzone") {
                style.LogSliderDeadzone = extract_f32();
            }
            else if (remainder == "TabRounding") {
                style.TabRounding = extract_f32();
            }
            else if (remainder == "TabBorderSize") {
                style.TabBorderSize = extract_f32();
            }
            else if (remainder == "TabMinWidthForCloseButton") {
                style.TabMinWidthForCloseButton = extract_f32();
            }
            else if (remainder == "SeparatorTextBorderSize") {
                style.SeparatorTextBorderSize = extract_f32();
            }
            else if (remainder == "MouseCursorScale") {
                style.MouseCursorScale = extract_f32();
            }
            else if (remainder == "CurveTessellationTol") {
                style.CurveTessellationTol = extract_f32();
            }
            else if (remainder == "CircleTessellationMaxError") {
                style.CircleTessellationMaxError = extract_f32();
            }

            else if (remainder == "AntiAliasedLines") {
                style.AntiAliasedLines = extract_bool();
            }
            else if (remainder == "AntiAliasedLinesUseTex") {
                style.AntiAliasedLinesUseTex = extract_bool();
            }
            else if (remainder == "AntiAliasedFill") {
                style.AntiAliasedFill = extract_bool();
            }

            else if (remainder == "WindowPadding.x") {
                style.WindowPadding.x = extract_f32();
            }
            else if (remainder == "WindowPadding.y") {
                style.WindowPadding.y = extract_f32();
            }
            else if (remainder == "WindowMinSize.x") {
                style.WindowMinSize.x = extract_f32();
            }
            else if (remainder == "WindowMinSize.y") {
                style.WindowMinSize.y = extract_f32();
            }
            else if (remainder == "WindowTitleAlign.x") {
                style.WindowTitleAlign.x = extract_f32();
            }
            else if (remainder == "WindowTitleAlign.y") {
                style.WindowTitleAlign.y = extract_f32();
            }
            else if (remainder == "FramePadding.x") {
                style.FramePadding.x = extract_f32();
            }
            else if (remainder == "FramePadding.y") {
                style.FramePadding.y = extract_f32();
            }
            else if (remainder == "ItemSpacing.x") {
                style.ItemSpacing.x = extract_f32();
            }
            else if (remainder == "ItemSpacing.y") {
                style.ItemSpacing.y = extract_f32();
            }
            else if (remainder == "ItemInnerSpacing.x") {
                style.ItemInnerSpacing.x = extract_f32();
            }
            else if (remainder == "ItemInnerSpacing.y") {
                style.ItemInnerSpacing.y = extract_f32();
            }
            else if (remainder == "CellPadding.x") {
                style.CellPadding.x = extract_f32();
            }
            else if (remainder == "CellPadding.y") {
                style.CellPadding.y = extract_f32();
            }
            else if (remainder == "TouchExtraPadding.x") {
                style.TouchExtraPadding.x = extract_f32();
            }
            else if (remainder == "TouchExtraPadding.y") {
                style.TouchExtraPadding.y = extract_f32();
            }
            else if (remainder == "ButtonTextAlign.x") {
                style.ButtonTextAlign.x = extract_f32();
            }
            else if (remainder == "ButtonTextAlign.y") {
                style.ButtonTextAlign.y = extract_f32();
            }
            else if (remainder == "SelectableTextAlign.x") {
                style.SelectableTextAlign.x = extract_f32();
            }
            else if (remainder == "SelectableTextAlign.y") {
                style.SelectableTextAlign.y = extract_f32();
            }
            else if (remainder == "SeparatorTextAlign.x") {
                style.SeparatorTextAlign.x = extract_f32();
            }
            else if (remainder == "SeparatorTextAlign.y") {
                style.SeparatorTextAlign.y = extract_f32();
            }
            else if (remainder == "SeparatorTextPadding.x") {
                style.SeparatorTextPadding.x = extract_f32();
            }
            else if (remainder == "SeparatorTextPadding.y") {
                style.SeparatorTextPadding.y = extract_f32();
            }
            else if (remainder == "DisplayWindowPadding.x") {
                style.DisplayWindowPadding.x = extract_f32();
            }
            else if (remainder == "DisplayWindowPadding.y") {
                style.DisplayWindowPadding.y = extract_f32();
            }
            else if (remainder == "DisplaySafeAreaPadding.x") {
                style.DisplaySafeAreaPadding.x = extract_f32();
            }
            else if (remainder == "DisplaySafeAreaPadding.y") {
                style.DisplaySafeAreaPadding.y = extract_f32();
            }

            else if (remainder == "WindowMenuButtonPosition") {
                style.WindowMenuButtonPosition = extract_ImGuiDir();
            }
            else if (remainder == "ColorButtonPosition") {
                style.ColorButtonPosition = extract_ImGuiDir();
            }

            else {
                print_debug_msg("Unknown property [%s] at line %zu, skipping...", property.c_str(), line_num);
                ++num_lines_skipped;
                continue;
            }
        }
        else if (property.starts_with("ImGuiCol.")) {
            std::string_view remainder(property.c_str() + lengthof("ImGuiCol"));

            if (remainder == "Text") {
                style.Colors[ImGuiCol_Text] = extract_ImVec4();
            }
            else if (remainder == "TextDisabled") {
                style.Colors[ImGuiCol_TextDisabled] = extract_ImVec4();
            }
            else if (remainder == "WindowBg") {
                style.Colors[ImGuiCol_WindowBg] = extract_ImVec4();
            }
            else if (remainder == "ChildBg") {
                style.Colors[ImGuiCol_ChildBg] = extract_ImVec4();
            }
            else if (remainder == "PopupBg") {
                style.Colors[ImGuiCol_PopupBg] = extract_ImVec4();
            }
            else if (remainder == "Border") {
                style.Colors[ImGuiCol_Border] = extract_ImVec4();
            }
            else if (remainder == "BorderShadow") {
                style.Colors[ImGuiCol_BorderShadow] = extract_ImVec4();
            }
            else if (remainder == "FrameBg") {
                style.Colors[ImGuiCol_FrameBg] = extract_ImVec4();
            }
            else if (remainder == "FrameBgHovered") {
                style.Colors[ImGuiCol_FrameBgHovered] = extract_ImVec4();
            }
            else if (remainder == "FrameBgActive") {
                style.Colors[ImGuiCol_FrameBgActive] = extract_ImVec4();
            }
            else if (remainder == "TitleBg") {
                style.Colors[ImGuiCol_TitleBg] = extract_ImVec4();
            }
            else if (remainder == "TitleBgActive") {
                style.Colors[ImGuiCol_TitleBgActive] = extract_ImVec4();
            }
            else if (remainder == "TitleBgCollapsed") {
                style.Colors[ImGuiCol_TitleBgCollapsed] = extract_ImVec4();
            }
            else if (remainder == "MenuBarBg") {
                style.Colors[ImGuiCol_MenuBarBg] = extract_ImVec4();
            }
            else if (remainder == "ScrollbarBg") {
                style.Colors[ImGuiCol_ScrollbarBg] = extract_ImVec4();
            }
            else if (remainder == "ScrollbarGrab") {
                style.Colors[ImGuiCol_ScrollbarGrab] = extract_ImVec4();
            }
            else if (remainder == "ScrollbarGrabHovered") {
                style.Colors[ImGuiCol_ScrollbarGrabHovered] = extract_ImVec4();
            }
            else if (remainder == "ScrollbarGrabActive") {
                style.Colors[ImGuiCol_ScrollbarGrabActive] = extract_ImVec4();
            }
            else if (remainder == "CheckMark") {
                style.Colors[ImGuiCol_CheckMark] = extract_ImVec4();
            }
            else if (remainder == "SliderGrab") {
                style.Colors[ImGuiCol_SliderGrab] = extract_ImVec4();
            }
            else if (remainder == "SliderGrabActive") {
                style.Colors[ImGuiCol_SliderGrabActive] = extract_ImVec4();
            }
            else if (remainder == "Button") {
                style.Colors[ImGuiCol_Button] = extract_ImVec4();
            }
            else if (remainder == "ButtonHovered") {
                style.Colors[ImGuiCol_ButtonHovered] = extract_ImVec4();
            }
            else if (remainder == "ButtonActive") {
                style.Colors[ImGuiCol_ButtonActive] = extract_ImVec4();
            }
            else if (remainder == "Header") {
                style.Colors[ImGuiCol_Header] = extract_ImVec4();
            }
            else if (remainder == "HeaderHovered") {
                style.Colors[ImGuiCol_HeaderHovered] = extract_ImVec4();
            }
            else if (remainder == "HeaderActive") {
                style.Colors[ImGuiCol_HeaderActive] = extract_ImVec4();
            }
            else if (remainder == "Separator") {
                style.Colors[ImGuiCol_Separator] = extract_ImVec4();
            }
            else if (remainder == "SeparatorHovered") {
                style.Colors[ImGuiCol_SeparatorHovered] = extract_ImVec4();
            }
            else if (remainder == "SeparatorActive") {
                style.Colors[ImGuiCol_SeparatorActive] = extract_ImVec4();
            }
            else if (remainder == "ResizeGrip") {
                style.Colors[ImGuiCol_ResizeGrip] = extract_ImVec4();
            }
            else if (remainder == "ResizeGripHovered") {
                style.Colors[ImGuiCol_ResizeGripHovered] = extract_ImVec4();
            }
            else if (remainder == "ResizeGripActive") {
                style.Colors[ImGuiCol_ResizeGripActive] = extract_ImVec4();
            }
            else if (remainder == "Tab") {
                style.Colors[ImGuiCol_Tab] = extract_ImVec4();
            }
            else if (remainder == "TabHovered") {
                style.Colors[ImGuiCol_TabHovered] = extract_ImVec4();
            }
            else if (remainder == "TabActive") {
                style.Colors[ImGuiCol_TabActive] = extract_ImVec4();
            }
            else if (remainder == "TabUnfocused") {
                style.Colors[ImGuiCol_TabUnfocused] = extract_ImVec4();
            }
            else if (remainder == "TabUnfocusedActive") {
                style.Colors[ImGuiCol_TabUnfocusedActive] = extract_ImVec4();
            }
            else if (remainder == "DockingPreview") {
                style.Colors[ImGuiCol_DockingPreview] = extract_ImVec4();
            }
            else if (remainder == "DockingEmptyBg") {
                style.Colors[ImGuiCol_DockingEmptyBg] = extract_ImVec4();
            }
            else if (remainder == "PlotLines") {
                style.Colors[ImGuiCol_PlotLines] = extract_ImVec4();
            }
            else if (remainder == "PlotLinesHovered") {
                style.Colors[ImGuiCol_PlotLinesHovered] = extract_ImVec4();
            }
            else if (remainder == "PlotHistogram") {
                style.Colors[ImGuiCol_PlotHistogram] = extract_ImVec4();
            }
            else if (remainder == "PlotHistogramHovered") {
                style.Colors[ImGuiCol_PlotHistogramHovered] = extract_ImVec4();
            }
            else if (remainder == "TableHeaderBg") {
                style.Colors[ImGuiCol_TableHeaderBg] = extract_ImVec4();
            }
            else if (remainder == "TableBorderStrong") {
                style.Colors[ImGuiCol_TableBorderStrong] = extract_ImVec4();
            }
            else if (remainder == "TableBorderLight") {
                style.Colors[ImGuiCol_TableBorderLight] = extract_ImVec4();
            }
            else if (remainder == "TableRowBg") {
                style.Colors[ImGuiCol_TableRowBg] = extract_ImVec4();
            }
            else if (remainder == "TableRowBgAlt") {
                style.Colors[ImGuiCol_TableRowBgAlt] = extract_ImVec4();
            }
            else if (remainder == "TextSelectedBg") {
                style.Colors[ImGuiCol_TextSelectedBg] = extract_ImVec4();
            }
            else if (remainder == "DragDropTarget") {
                style.Colors[ImGuiCol_DragDropTarget] = extract_ImVec4();
            }
            else if (remainder == "NavHighlight") {
                style.Colors[ImGuiCol_NavHighlight] = extract_ImVec4();
            }
            else if (remainder == "NavWindowingHighlight") {
                style.Colors[ImGuiCol_NavWindowingHighlight] = extract_ImVec4();
            }
            else if (remainder == "NavWindowingDimBg") {
                style.Colors[ImGuiCol_NavWindowingDimBg] = extract_ImVec4();
            }
            else if (remainder == "ModalWindowDimBg") {
                style.Colors[ImGuiCol_ModalWindowDimBg] = extract_ImVec4();
            }
            else {
                print_debug_msg("Unknown property [%s] at line %zu, skipping...", property.c_str(), line_num);
                ++num_lines_skipped;
                continue;
            }
        }
        else if (property.starts_with("check.")) {
            std::string_view remainder(property.c_str() + lengthof("check"));

            if (remainder == "ImGuiCol_Text") {
                this->checks_ImGuiCol[ImGuiCol_Text] = extract_bool();
            }
            else if (remainder == "ImGuiCol_TextDisabled") {
                this->checks_ImGuiCol[ImGuiCol_TextDisabled] = extract_bool();
            }
            else if (remainder == "ImGuiCol_WindowBg") {
                this->checks_ImGuiCol[ImGuiCol_WindowBg] = extract_bool();
            }
            else if (remainder == "ImGuiCol_ChildBg") {
                this->checks_ImGuiCol[ImGuiCol_ChildBg] = extract_bool();
            }
            else if (remainder == "ImGuiCol_PopupBg") {
                this->checks_ImGuiCol[ImGuiCol_PopupBg] = extract_bool();
            }
            else if (remainder == "ImGuiCol_Border") {
                this->checks_ImGuiCol[ImGuiCol_Border] = extract_bool();
            }
            else if (remainder == "ImGuiCol_BorderShadow") {
                this->checks_ImGuiCol[ImGuiCol_BorderShadow] = extract_bool();
            }
            else if (remainder == "ImGuiCol_FrameBg") {
                this->checks_ImGuiCol[ImGuiCol_FrameBg] = extract_bool();
            }
            else if (remainder == "ImGuiCol_FrameBgHovered") {
                this->checks_ImGuiCol[ImGuiCol_FrameBgHovered] = extract_bool();
            }
            else if (remainder == "ImGuiCol_FrameBgActive") {
                this->checks_ImGuiCol[ImGuiCol_FrameBgActive] = extract_bool();
            }
            else if (remainder == "ImGuiCol_TitleBg") {
                this->checks_ImGuiCol[ImGuiCol_TitleBg] = extract_bool();
            }
            else if (remainder == "ImGuiCol_TitleBgActive") {
                this->checks_ImGuiCol[ImGuiCol_TitleBgActive] = extract_bool();
            }
            else if (remainder == "ImGuiCol_TitleBgCollapsed") {
                this->checks_ImGuiCol[ImGuiCol_TitleBgCollapsed] = extract_bool();
            }
            else if (remainder == "ImGuiCol_MenuBarBg") {
                this->checks_ImGuiCol[ImGuiCol_MenuBarBg] = extract_bool();
            }
            else if (remainder == "ImGuiCol_ScrollbarBg") {
                this->checks_ImGuiCol[ImGuiCol_ScrollbarBg] = extract_bool();
            }
            else if (remainder == "ImGuiCol_ScrollbarGrab") {
                this->checks_ImGuiCol[ImGuiCol_ScrollbarGrab] = extract_bool();
            }
            else if (remainder == "ImGuiCol_ScrollbarGrabHovered") {
                this->checks_ImGuiCol[ImGuiCol_ScrollbarGrabHovered] = extract_bool();
            }
            else if (remainder == "ImGuiCol_ScrollbarGrabActive") {
                this->checks_ImGuiCol[ImGuiCol_ScrollbarGrabActive] = extract_bool();
            }
            else if (remainder == "ImGuiCol_CheckMark") {
                this->checks_ImGuiCol[ImGuiCol_CheckMark] = extract_bool();
            }
            else if (remainder == "ImGuiCol_SliderGrab") {
                this->checks_ImGuiCol[ImGuiCol_SliderGrab] = extract_bool();
            }
            else if (remainder == "ImGuiCol_SliderGrabActive") {
                this->checks_ImGuiCol[ImGuiCol_SliderGrabActive] = extract_bool();
            }
            else if (remainder == "ImGuiCol_Button") {
                this->checks_ImGuiCol[ImGuiCol_Button] = extract_bool();
            }
            else if (remainder == "ImGuiCol_ButtonHovered") {
                this->checks_ImGuiCol[ImGuiCol_ButtonHovered] = extract_bool();
            }
            else if (remainder == "ImGuiCol_ButtonActive") {
                this->checks_ImGuiCol[ImGuiCol_ButtonActive] = extract_bool();
            }
            else if (remainder == "ImGuiCol_Header") {
                this->checks_ImGuiCol[ImGuiCol_Header] = extract_bool();
            }
            else if (remainder == "ImGuiCol_HeaderHovered") {
                this->checks_ImGuiCol[ImGuiCol_HeaderHovered] = extract_bool();
            }
            else if (remainder == "ImGuiCol_HeaderActive") {
                this->checks_ImGuiCol[ImGuiCol_HeaderActive] = extract_bool();
            }
            else if (remainder == "ImGuiCol_Separator") {
                this->checks_ImGuiCol[ImGuiCol_Separator] = extract_bool();
            }
            else if (remainder == "ImGuiCol_SeparatorHovered") {
                this->checks_ImGuiCol[ImGuiCol_SeparatorHovered] = extract_bool();
            }
            else if (remainder == "ImGuiCol_SeparatorActive") {
                this->checks_ImGuiCol[ImGuiCol_SeparatorActive] = extract_bool();
            }
            else if (remainder == "ImGuiCol_ResizeGrip") {
                this->checks_ImGuiCol[ImGuiCol_ResizeGrip] = extract_bool();
            }
            else if (remainder == "ImGuiCol_ResizeGripHovered") {
                this->checks_ImGuiCol[ImGuiCol_ResizeGripHovered] = extract_bool();
            }
            else if (remainder == "ImGuiCol_ResizeGripActive") {
                this->checks_ImGuiCol[ImGuiCol_ResizeGripActive] = extract_bool();
            }
            else if (remainder == "ImGuiCol_Tab") {
                this->checks_ImGuiCol[ImGuiCol_Tab] = extract_bool();
            }
            else if (remainder == "ImGuiCol_TabHovered") {
                this->checks_ImGuiCol[ImGuiCol_TabHovered] = extract_bool();
            }
            else if (remainder == "ImGuiCol_TabActive") {
                this->checks_ImGuiCol[ImGuiCol_TabActive] = extract_bool();
            }
            else if (remainder == "ImGuiCol_TabUnfocused") {
                this->checks_ImGuiCol[ImGuiCol_TabUnfocused] = extract_bool();
            }
            else if (remainder == "ImGuiCol_TabUnfocusedActive") {
                this->checks_ImGuiCol[ImGuiCol_TabUnfocusedActive] = extract_bool();
            }
            else if (remainder == "ImGuiCol_DockingPreview") {
                this->checks_ImGuiCol[ImGuiCol_DockingPreview] = extract_bool();
            }
            else if (remainder == "ImGuiCol_DockingEmptyBg") {
                this->checks_ImGuiCol[ImGuiCol_DockingEmptyBg] = extract_bool();
            }
            else if (remainder == "ImGuiCol_PlotLines") {
                this->checks_ImGuiCol[ImGuiCol_PlotLines] = extract_bool();
            }
            else if (remainder == "ImGuiCol_PlotLinesHovered") {
                this->checks_ImGuiCol[ImGuiCol_PlotLinesHovered] = extract_bool();
            }
            else if (remainder == "ImGuiCol_PlotHistogram") {
                this->checks_ImGuiCol[ImGuiCol_PlotHistogram] = extract_bool();
            }
            else if (remainder == "ImGuiCol_PlotHistogramHovered") {
                this->checks_ImGuiCol[ImGuiCol_PlotHistogramHovered] = extract_bool();
            }
            else if (remainder == "ImGuiCol_TableHeaderBg") {
                this->checks_ImGuiCol[ImGuiCol_TableHeaderBg] = extract_bool();
            }
            else if (remainder == "ImGuiCol_TableBorderStrong") {
                this->checks_ImGuiCol[ImGuiCol_TableBorderStrong] = extract_bool();
            }
            else if (remainder == "ImGuiCol_TableBorderLight") {
                this->checks_ImGuiCol[ImGuiCol_TableBorderLight] = extract_bool();
            }
            else if (remainder == "ImGuiCol_TableRowBg") {
                this->checks_ImGuiCol[ImGuiCol_TableRowBg] = extract_bool();
            }
            else if (remainder == "ImGuiCol_TableRowBgAlt") {
                this->checks_ImGuiCol[ImGuiCol_TableRowBgAlt] = extract_bool();
            }
            else if (remainder == "ImGuiCol_TextSelectedBg") {
                this->checks_ImGuiCol[ImGuiCol_TextSelectedBg] = extract_bool();
            }
            else if (remainder == "ImGuiCol_DragDropTarget") {
                this->checks_ImGuiCol[ImGuiCol_DragDropTarget] = extract_bool();
            }
            else if (remainder == "ImGuiCol_NavHighlight") {
                this->checks_ImGuiCol[ImGuiCol_NavHighlight] = extract_bool();
            }
            else if (remainder == "ImGuiCol_NavWindowingHighlight") {
                this->checks_ImGuiCol[ImGuiCol_NavWindowingHighlight] = extract_bool();
            }
            else if (remainder == "ImGuiCol_NavWindowingDimBg") {
                this->checks_ImGuiCol[ImGuiCol_NavWindowingDimBg] = extract_bool();
            }
            else if (remainder == "ImGuiCol_ModalWindowDimBg") {
                this->checks_ImGuiCol[ImGuiCol_ModalWindowDimBg] = extract_bool();
            }

            else if (remainder == "success_color") {
                this->check_success_color = extract_bool();
            }
            else if (remainder == "warning_color") {
                this->check_warning_color = extract_bool();
            }
            else if (remainder == "warning_lite_color") {
                this->check_warning_lite_color = extract_bool();
            }
            else if (remainder == "error_color") {
                this->check_error_color = extract_bool();
            }
            else if (remainder == "directory_color") {
                this->check_directory_color = extract_bool();
            }
            else if (remainder == "file_color") {
                this->check_file_color = extract_bool();
            }
            else if (remainder == "symlink_color") {
                this->check_symlink_color = extract_bool();
            }

            else {
                print_debug_msg("Unknown property [%s] at line %zu, skipping...", property.c_str(), line_num);
                ++num_lines_skipped;
                continue;
            }
        }
        else {
            if (property == "window_x") {
                ss >> this->window_x;
            }
            else if (property == "window_y") {
                ss >> this->window_y;
            }
            else if (property == "window_w") {
                ss >> this->window_w;
            }
            else if (property == "window_h") {
                ss >> this->window_h;
            }
            else if (property == "size_unit_multiplier") {
                ss >> this->size_unit_multiplier;
            }
            else if (property == "explorer_refresh_mode") {
                ss >> (s32 &)this->explorer_refresh_mode;
            }
            else if (property == "unix_directory_separator") {
                bool unix_directory_separator = extract_bool();
                if (unix_directory_separator) {
                    this->dir_separator_utf8 = '/';
                    this->dir_separator_utf16 = L'/';
                } else {
                    this->dir_separator_utf8 = '\\';
                    this->dir_separator_utf16 = L'\\';
                }
            }
            else if (property == "show_debug_info") {
                this->show_debug_info = extract_bool();
            }
            else if (property == "explorer_show_dotdot_dir") {
                this->explorer_show_dotdot_dir = extract_bool();
            }
            else if (property == "tables_alt_row_bg") {
                this->tables_alt_row_bg = extract_bool();
            }
            else if (property == "table_borders_in_body") {
                this->table_borders_in_body = extract_bool();
            }
            else if (property == "explorer_clear_filter_on_cwd_change") {
                this->explorer_clear_filter_on_cwd_change = extract_bool();
            }
            else if (property == "file_extension_icons") {
                this->file_extension_icons = extract_bool();
            }

            else if (property == "file_operations_src_path_full") {
                this->file_operations_src_path_full = extract_bool();
            }
            else if (property == "file_operations_dst_path_full") {
                this->file_operations_dst_path_full = extract_bool();
            }

            else if (property == "startup_with_window_maximized") {
                this->startup_with_window_maximized = extract_bool();
            }
            else if (property == "startup_with_previous_window_pos_and_size") {
                this->startup_with_previous_window_pos_and_size = extract_bool();
            }

            else {
                print_debug_msg("Unknown property [%s] at line %zu, skipping...", property.c_str(), line_num);
                ++num_lines_skipped;
                continue;
            }
        }

        ++num_lines_parsed;
    }

    print_debug_msg("SUCCESS parsed %zu lines, skipped %zu", num_lines_parsed, num_lines_skipped);
    return true;
}
catch (std::exception const &except) {
    print_debug_msg("FAILED catch(std::exception) %s", except.what());
    return false;
}
catch (...) {
    print_debug_msg("FAILED, catch(...)");
    return false;
}
