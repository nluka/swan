#include "stdafx.hpp"
#include "common_fns.hpp"
#include "imgui_specific.hpp"
#include "path.hpp"

static swan_settings g_settings = {};
swan_settings &global_state::settings() noexcept { return g_settings; }

void swan_windows::render_settings(GLFWwindow *window) noexcept
{
    static bool regular_change = false;
    static bool overridden = false;

    if (imgui::Begin(swan_windows::get_name(swan_windows::settings), &global_state::settings().show.settings)) {
        if (imgui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)) {
            global_state::save_focused_window(swan_windows::settings);
        }

        auto &settings = global_state::settings();

        regular_change |= imgui::Checkbox("Start with window maximized", &settings.startup_with_window_maximized);
        regular_change |= imgui::Checkbox("Start with previous window size & pos", &settings.startup_with_previous_window_pos_and_size);

        {
            imgui::Separator();
            imgui::AlignTextToFramePadding();
            imgui::TextUnformatted("Override window pos & size");
            if (overridden) {
                imgui::SameLine();
                if (imgui::Button("Apply")) {
                    overridden = false;
                    glfwSetWindowPos(window, global_state::settings().window_x, global_state::settings().window_y);
                    glfwSetWindowSize(window, global_state::settings().window_w, global_state::settings().window_h);
                    (void) settings.save_to_disk();
                }
            }
        #if 1
            imgui::ScopedItemWidth w(200.f);
            overridden |= imgui::InputInt2("Window position (x, y)", &settings.window_x);
            overridden |= imgui::InputInt2("Window size (w, h)", &settings.window_w);
        #else
            imgui::ScopedItemWidth w(150.f);
            overridden |= imgui::InputInt("Window position x", &settings.window_x);
            overridden |= imgui::InputInt("Window position y", &settings.window_y);
            overridden |= imgui::InputInt("Window width", &settings.window_w);
            overridden |= imgui::InputInt("Window height", &settings.window_h);
        #endif
        }

        if (regular_change) {
            regular_change = false;
            (void) settings.save_to_disk();
        }
    }

    imgui::End();
}

bool swan_settings::save_to_disk() const noexcept
try {
    std::filesystem::path full_path = global_state::execution_path() / "data\\swan_settings.xml";
    std::ofstream ofs(full_path);
    if (!ofs) {
        return false;
    }
#if SWAN_SETTINGS_USE_BOOST_SERIALIZATION
    {
        boost::archive::xml_oarchive oa(ofs);
        oa << boost::serialization::make_nvp("swan_settings", *this);
    }
#else
    static_assert(s8(1) == s8(true));
    static_assert(s8(0) == s8(false));

    assert(one_of(this->size_unit_multiplier, { 1000, 1024 }));

    auto write_bool = [&](char const *key, bool val) noexcept {
        ofs << key << ' ' << s32(val) << '\n';
    };

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
    write_bool("explorer_show_dotdot_dir", this->explorer_show_dotdot_dir);
    write_bool("explorer_cwd_entries_table_alt_row_bg", this->explorer_cwd_entries_table_alt_row_bg);
    write_bool("explorer_cwd_entries_table_borders_in_body", this->explorer_cwd_entries_table_borders_in_body);
    write_bool("explorer_clear_filter_on_cwd_change", this->explorer_clear_filter_on_cwd_change);

    write_bool("startup_with_window_maximized", this->startup_with_window_maximized);
    write_bool("startup_with_previous_window_pos_and_size", this->startup_with_previous_window_pos_and_size);

    write_bool("confirm_explorer_delete_via_keybind", this->confirm_explorer_delete_via_keybind);
    write_bool("confirm_explorer_delete_via_context_menu", this->confirm_explorer_delete_via_context_menu);
    write_bool("confirm_explorer_unpin_directory", this->confirm_explorer_unpin_directory);
    write_bool("confirm_recent_files_clear", this->confirm_recent_files_clear);
    write_bool("confirm_delete_pin", this->confirm_delete_pin);
    write_bool("confirm_completed_file_operations_forget_single", this->confirm_completed_file_operations_forget_single);
    write_bool("confirm_completed_file_operations_forget_group", this->confirm_completed_file_operations_forget_group);
    write_bool("confirm_completed_file_operations_forget_selected", this->confirm_completed_file_operations_forget_selected);
    write_bool("confirm_completed_file_operations_forget_all", this->confirm_completed_file_operations_forget_all);

    write_bool("show.explorer_0", this->show.explorer_0);
    write_bool("show.explorer_1", this->show.explorer_1);
    write_bool("show.explorer_2", this->show.explorer_2);
    write_bool("show.explorer_3", this->show.explorer_3);
    write_bool("show.pinned", this->show.pinned);
    write_bool("show.file_operations", this->show.file_operations);
    write_bool("show.recent_files", this->show.recent_files);
    write_bool("show.analytics", this->show.analytics);
    write_bool("show.settings", this->show.settings);
    write_bool("show.debug_log", this->show.debug_log);
    write_bool("show.imgui_demo", this->show.imgui_demo);
    write_bool("show.icon_library", this->show.icon_library);
    write_bool("show.fa_icons", this->show.fa_icons);
    write_bool("show.ci_icons", this->show.ci_icons);
    write_bool("show.md_icons", this->show.md_icons);
#endif
    print_debug_msg("SUCCESS swan_settings::save_to_disk");
    return true;
}
catch (std::exception const &except) {
    print_debug_msg("FAILED swan_settings::save_to_disk, %s", except.what());
    return false;
}
catch (...) {
    print_debug_msg("FAILED swan_settings::save_to_disk");
    return false;
}

bool swan_settings::load_from_disk() noexcept
try {
    std::filesystem::path full_path = global_state::execution_path() / "data\\swan_settings.xml";
    std::ifstream ifs(full_path);
    if (!ifs) {
        print_debug_msg("FAILED global_state::settings::load_from_disk, !file");
        return false;
    }
#if SWAN_SETTINGS_USE_BOOST_SERIALIZATION
    {
        boost::archive::xml_iarchive ia(ifs);
        ia >> boost::serialization::make_nvp("swan_settings", *this);
    }
#else
    auto content = std::string(std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>());
    while (content.back() == '\n') {
        content.pop_back();
    }

    auto lines = std::string_view(content) | std::ranges::views::split('\n');

    std::regex valid_line("^[a-z0-9_.]{1,} [0-9]{1,}$");
    std::stringstream ss;
    std::string line_str = {};
    std::string property = {};
    u64 line_num = 0;

    auto extract_bool = [&]() -> bool {
        char bool_ch = {};
        ss >> bool_ch;
        return bool_ch == '1';
    };

    for (auto const &line : lines) {
        ++line_num;
        line_str = std::string(line.data(), line.size());

        if (!std::regex_match(line_str, valid_line)) {
            print_debug_msg("FAILED global_state::settings::load_from_disk, malformed content at line %zu", line_num);
            return false;
        }

        ss.str(""); ss.clear();
        ss << line_str;
        ss >> property;

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
        else if (property == "explorer_cwd_entries_table_alt_row_bg") {
            this->explorer_cwd_entries_table_alt_row_bg = extract_bool();
        }
        else if (property == "explorer_cwd_entries_table_borders_in_body") {
            this->explorer_cwd_entries_table_borders_in_body = extract_bool();
        }
        else if (property == "explorer_clear_filter_on_cwd_change") {
            this->explorer_clear_filter_on_cwd_change = extract_bool();
        }

        else if (property == "startup_with_window_maximized") {
            this->startup_with_window_maximized = extract_bool();
        }
        else if (property == "startup_with_previous_window_pos_and_size") {
            this->startup_with_previous_window_pos_and_size = extract_bool();
        }

        else if (property == "confirm_explorer_delete_via_keybind") {
            this->confirm_explorer_delete_via_keybind = extract_bool();
        }
        else if (property == "confirm_explorer_delete_via_context_menu") {
            this->confirm_explorer_delete_via_context_menu = extract_bool();
        }
        else if (property == "confirm_explorer_unpin_directory") {
            this->confirm_explorer_unpin_directory = extract_bool();
        }
        else if (property == "confirm_recent_files_clear") {
            this->confirm_recent_files_clear = extract_bool();
        }
        else if (property == "confirm_delete_pin") {
            this->confirm_delete_pin = extract_bool();
        }
        else if (property == "confirm_completed_file_operations_forget_single") {
            this->confirm_completed_file_operations_forget_single = extract_bool();
        }
        else if (property == "confirm_completed_file_operations_forget_group") {
            this->confirm_completed_file_operations_forget_group = extract_bool();
        }
        else if (property == "confirm_completed_file_operations_forget_selected") {
            this->confirm_completed_file_operations_forget_selected = extract_bool();
        }
        else if (property == "confirm_completed_file_operations_forget_all") {
            this->confirm_completed_file_operations_forget_all = extract_bool();
        }

        else if (property == "show.pinned") {
            this->show.pinned = extract_bool();
        }
        else if (property == "show.file_operations") {
            this->show.file_operations = extract_bool();
        }
        else if (property == "show.recent_files") {
            this->show.recent_files = extract_bool();
        }
        else if (property == "show.explorer_0") {
            this->show.explorer_0 = extract_bool();
        }
        else if (property == "show.explorer_1") {
            this->show.explorer_1 = extract_bool();
        }
        else if (property == "show.explorer_2") {
            this->show.explorer_2 = extract_bool();
        }
        else if (property == "show.explorer_3") {
            this->show.explorer_3 = extract_bool();
        }
        else if (property == "show.analytics") {
            this->show.analytics = extract_bool();
        }
        else if (property == "show.debug_log") {
            this->show.debug_log = extract_bool();
        }
        else if (property == "show.settings") {
            this->show.settings = extract_bool();
        }
        else if (property == "show.imgui_demo") {
            this->show.imgui_demo = extract_bool();
        }
        else if (property == "show.icon_library") {
            this->show.icon_library = extract_bool();
        }
        else if (property == "show.fa_icons") {
            this->show.fa_icons = extract_bool();
        }
        else if (property == "show.ci_icons") {
            this->show.ci_icons = extract_bool();
        }
        else if (property == "show.md_icons") {
            this->show.md_icons = extract_bool();
        }
        else {
            print_debug_msg("Unknown property [%s] in [swan_settings.txt]", property.c_str());
        }
    }
#endif
    print_debug_msg("SUCCESS global_state::settings::load_from_disk");
    return true;
}
catch (std::exception const &except) {
    print_debug_msg("FAILED global_state::settings::load_from_disk, %s", except.what());
    return false;
}
catch (...) {
    print_debug_msg("FAILED global_state::settings::load_from_disk, catch(...)");
    return false;
}
