#include "stdafx.hpp"
#include "data_types.hpp"
#include "common_fns.hpp"
#include "imgui_specific.hpp"
#include "path.hpp"

// #undef min
// #undef max

static swan_settings s_settings = {};
swan_settings &global_state::settings() noexcept { return s_settings; }

void swan_windows::render_settings(GLFWwindow *window) noexcept
{
    static bool regular_change = false;
    static bool overridden = false;

    if (imgui::Begin(swan_windows::get_name(swan_windows::settings), &global_state::settings().show.settings)) {
        if (imgui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)) {
            global_state::save_focused_window(swan_windows::settings);
        }

        auto &settings = global_state::settings();

        regular_change |= imgui::Checkbox("Start with window maximized", &settings.start_with_window_maximized);
        regular_change |= imgui::Checkbox("Start with previous window size & pos", &settings.start_with_previous_window_pos_and_size);

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
    std::filesystem::path full_path = global_state::execution_path() / "data\\swan_settings.txt";

    std::ofstream out(full_path);

    if (!out) {
        return false;
    }

    static_assert(s8(1) == s8(true));
    static_assert(s8(0) == s8(false));

    assert(one_of(this->size_unit_multiplier, { 1000, 1024 }));

    auto write_bool = [&](char const *key, bool val) {
        out << key << ' ' << s32(val) << '\n';
    };

    out << "window_x " << this->window_x << '\n';
    out << "window_y " << this->window_y << '\n';
    out << "window_w " << this->window_w << '\n';
    out << "window_h " << this->window_h << '\n';
    out << "size_unit_multiplier " << this->size_unit_multiplier << '\n';
    out << "expl_refresh_mode " << (s32)this->expl_refresh_mode << '\n';

    // wchar_t dir_separator_utf16;
    // char dir_separator_utf8;
    //? stored as a bool:
    write_bool("unix_directory_separator", this->dir_separator_utf8 == '/');

    write_bool("show_debug_info", this->show_debug_info);
    write_bool("show_dotdot_dir", this->show_dotdot_dir);
    write_bool("cwd_entries_table_alt_row_bg", this->cwd_entries_table_alt_row_bg);
    write_bool("cwd_entries_table_borders_in_body", this->cwd_entries_table_borders_in_body);
    write_bool("clear_filter_on_cwd_change", this->clear_filter_on_cwd_change);

    write_bool("start_with_window_maximized", this->start_with_window_maximized);
    write_bool("start_with_previous_window_pos_and_size", this->start_with_previous_window_pos_and_size);

    write_bool("show.explorer_0", this->show.explorer_0);
    write_bool("show.explorer_1", this->show.explorer_1);
    write_bool("show.explorer_2", this->show.explorer_2);
    write_bool("show.explorer_3", this->show.explorer_3);
    write_bool("show.pin_manager", this->show.pin_manager);
    write_bool("show.file_operations", this->show.file_operations);
    write_bool("show.recent_files", this->show.recent_files);
    write_bool("show.analytics", this->show.analytics);
    write_bool("show.settings", this->show.settings);
    write_bool("show.debug_log", this->show.debug_log);
#if DEBUG_MODE
    write_bool("show.imgui_demo", this->show.imgui_demo);
    write_bool("show.fa_icons", this->show.fa_icons);
    write_bool("show.ci_icons", this->show.ci_icons);
    write_bool("show.md_icons", this->show.md_icons);
#endif

    print_debug_msg("SUCCESS swan_settings::save_to_disk");
    return true;
}
catch (...) {
    print_debug_msg("FAILED swan_settings::save_to_disk");
    return false;
}

bool swan_settings::load_from_disk() noexcept
try {
    std::filesystem::path full_path = global_state::execution_path() / "data\\swan_settings.txt";

    std::ifstream file(full_path);

    if (!file) {
        print_debug_msg("FAILED global_state::settings::load_from_disk, !file");
        return false;
    }

    auto content = std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
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
        else if (property == "expl_refresh_mode") {
            ss >> (s32 &)this->expl_refresh_mode;
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
        else if (property == "show_dotdot_dir") {
            this->show_dotdot_dir = extract_bool();
        }
        else if (property == "cwd_entries_table_alt_row_bg") {
            this->cwd_entries_table_alt_row_bg = extract_bool();
        }
        else if (property == "cwd_entries_table_borders_in_body") {
            this->cwd_entries_table_borders_in_body = extract_bool();
        }
        else if (property == "clear_filter_on_cwd_change") {
            this->clear_filter_on_cwd_change = extract_bool();
        }
        else if (property == "start_with_window_maximized") {
            this->start_with_window_maximized = extract_bool();
        }
        else if (property == "start_with_previous_window_pos_and_size") {
            this->start_with_previous_window_pos_and_size = extract_bool();
        }
        else if (property == "show.pin_manager") {
            this->show.pin_manager = extract_bool();
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
    #if DEBUG_MODE
        else if (property == "show.imgui_demo") {
            this->show.imgui_demo = extract_bool();
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
    #endif
        else {
            print_debug_msg("Unknown property [%s] in [swan_settings.txt]", property.c_str());
        }
    }

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
