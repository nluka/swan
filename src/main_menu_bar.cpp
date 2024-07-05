#include "common_functions.hpp"
#include "imgui_extension.hpp"

void render_main_menu_bar(std::array<explorer_window, global_constants::num_explorers> &explorers) noexcept
{
    if (imgui::BeginMainMenuBar()) {
        bool setting_change = false;
        static_assert((false | false) == false);
        static_assert((false | true) == true);
        static_assert((true | true) == true);

        if (imgui::BeginMenu("[Windows]")) {
            setting_change |= imgui::MenuItem(explorers[0].name, nullptr, &global_state::settings().show.explorer_0);
            setting_change |= imgui::MenuItem(explorers[1].name, nullptr, &global_state::settings().show.explorer_1);
            setting_change |= imgui::MenuItem(explorers[2].name, nullptr, &global_state::settings().show.explorer_2);
            setting_change |= imgui::MenuItem(explorers[3].name, nullptr, &global_state::settings().show.explorer_3);

            setting_change |= imgui::MenuItem(swan_windows::get_name(swan_windows::id::finder), nullptr, &global_state::settings().show.finder);
            setting_change |= imgui::MenuItem(swan_windows::get_name(swan_windows::id::pinned), nullptr, &global_state::settings().show.pinned);
            setting_change |= imgui::MenuItem(swan_windows::get_name(swan_windows::id::file_operations), nullptr, &global_state::settings().show.file_operations);
            setting_change |= imgui::MenuItem(swan_windows::get_name(swan_windows::id::recent_files), nullptr, &global_state::settings().show.recent_files);
            setting_change |= imgui::MenuItem(swan_windows::get_name(swan_windows::id::analytics), nullptr, &global_state::settings().show.analytics);
            setting_change |= imgui::MenuItem(swan_windows::get_name(swan_windows::id::settings), nullptr, &global_state::settings().show.settings);

            imgui::Separator();

            setting_change |= imgui::MenuItem(swan_windows::get_name(swan_windows::id::theme_editor), nullptr, &global_state::settings().show.theme_editor);
            setting_change |= imgui::MenuItem(swan_windows::get_name(swan_windows::id::icon_library), nullptr, &global_state::settings().show.icon_library);

            imgui::Separator();

            setting_change |= imgui::MenuItem(swan_windows::get_name(swan_windows::id::debug_log), nullptr, &global_state::settings().show.debug_log);
            setting_change |= imgui::MenuItem(swan_windows::get_name(swan_windows::id::imgui_demo), nullptr, &global_state::settings().show.imgui_demo);

            imgui::EndMenu();
        }
        if (imgui::BeginMenu("[Settings]")) {
            {
                static bool binary_size_system = {};
                binary_size_system = global_state::settings().size_unit_multiplier == 1024;

                if (imgui::MenuItem("Base2 size system, 1024 > 1000", nullptr, &binary_size_system)) {
                    setting_change = true;

                    global_state::settings().size_unit_multiplier = binary_size_system ? 1024 : 1000;

                    for (auto &expl : explorers) {
                        expl.update_request_from_outside = full_refresh;
                    }
                }
            }
            {
                static bool unix_directory_separator = {};
                unix_directory_separator = global_state::settings().dir_separator_utf8 == '/';

                if (imgui::MenuItem("Unix directory separators", nullptr, &unix_directory_separator)) {
                    setting_change = true;

                    char new_utf8_separator = unix_directory_separator ? '/' : '\\';

                    global_state::settings().dir_separator_utf8 = new_utf8_separator;
                    global_state::settings().dir_separator_utf16 = static_cast<wchar_t>(new_utf8_separator);
                    global_state::pinned_update_directory_separators(new_utf8_separator);

                    for (auto &expl : explorers) {
                        path_force_separator(expl.cwd, new_utf8_separator);
                    }

                    {
                        auto recent_files = global_state::recent_files_get();

                        std::scoped_lock lock(*recent_files.mutex);

                        for (auto &recent_file : *recent_files.container) {
                            path_force_separator(recent_file.path, new_utf8_separator);
                        }
                    }

                    {
                        auto completed_file_operations = global_state::completed_file_operations_get();

                        std::scoped_lock lock(*completed_file_operations.mutex);

                        for (auto &file_op : *completed_file_operations.container) {
                            path_force_separator(file_op.src_path, global_state::settings().dir_separator_utf8);
                            path_force_separator(file_op.dst_path, global_state::settings().dir_separator_utf8);
                        }
                    }
                }
            }

            if (imgui::MenuItem("Windows file icons", nullptr, &global_state::settings().win32_file_icons)) {
                setting_change = true;

                for (auto &expl : global_state::explorers()) {
                    for (auto &dirent : expl.cwd_entries) {
                        if (dirent.icon_GLtexID > 0) delete_icon_texture(dirent.icon_GLtexID, "explorer_window::dirent");
                    }
                }
                {
                    auto recent_files = global_state::recent_files_get();
                    std::scoped_lock lock(*recent_files.mutex);
                    for (auto &rf : *recent_files.container) {
                        if (rf.icon_GLtexID > 0) delete_icon_texture(rf.icon_GLtexID, "recent_file");
                    }
                }
                {
                    auto completed_file_operations = global_state::completed_file_operations_get();
                    std::scoped_lock lock(*completed_file_operations.mutex);
                    for (auto &cfo : *completed_file_operations.container) {
                        if (cfo.src_icon_GLtexID > 0) delete_icon_texture(cfo.src_icon_GLtexID, "completed_file_operation");
                        if (cfo.dst_icon_GLtexID > 0) delete_icon_texture(cfo.dst_icon_GLtexID, "completed_file_operation");
                    }
                }
            }

            setting_change |= imgui::MenuItem("Alternating table rows background", nullptr, &global_state::settings().tables_alt_row_bg);
            setting_change |= imgui::MenuItem("Borders in table body", nullptr, &global_state::settings().table_borders_in_body);
            setting_change |= imgui::MenuItem("Show debug info", nullptr, &global_state::settings().show_debug_info);

            imgui::Separator();

            if (imgui::BeginMenu("Explorer")) {
                if (imgui::BeginMenu("Refresh mode")) {
                    char const *labels[] = {
                        "Automatic",
                        "Notify   ",
                        "Manual   ",
                    };
                    {
                        imgui::ScopedItemWidth w(imgui::CalcTextSize(labels[0]).x + 50);
                        imgui::ScopedStyle<ImVec2> p(imgui::GetStyle().FramePadding, { 6, 4 });
                        setting_change |= imgui::Combo("## explorer_refresh_mode", (s32 *)&global_state::settings().explorer_refresh_mode, labels, (s32)lengthof(labels));
                    }
                    imgui::EndMenu();
                }

                if (imgui::MenuItem("Show '..' directory", nullptr, &global_state::settings().explorer_show_dotdot_dir)) {
                    setting_change = true;
                    for (auto &expl : explorers) {
                        expl.update_request_from_outside = full_refresh;
                    }
                }

                setting_change |= imgui::MenuItem("Clear filter on navigation", nullptr, &global_state::settings().explorer_clear_filter_on_cwd_change);

                imgui::EndMenu();
            }

            if (imgui::BeginMenu("Confirmations")) {
                setting_change |= imgui::MenuItem("[Recent Files]     Clear", nullptr, &global_state::settings().confirm_recent_files_clear);
                setting_change |= imgui::MenuItem("[Recent Files]     Reveal selection in File Explorer", nullptr, &global_state::settings().confirm_recent_files_reveal_selected_in_win_file_expl);
                setting_change |= imgui::MenuItem("[Recent Files]     Forget selection", nullptr, &global_state::settings().confirm_recent_files_forget_selected);
                setting_change |= imgui::MenuItem("[Pinned]           Delete pin", nullptr, &global_state::settings().confirm_delete_pin);
                setting_change |= imgui::MenuItem("[Explorer]         Delete via context menu", nullptr, &global_state::settings().confirm_explorer_delete_via_context_menu);
                setting_change |= imgui::MenuItem("[Explorer]         Delete via Del key", nullptr, &global_state::settings().confirm_explorer_delete_via_keybind);
                setting_change |= imgui::MenuItem("[Explorer]         Unpin working directory", nullptr, &global_state::settings().confirm_explorer_unpin_directory);
                setting_change |= imgui::MenuItem("[File Operations]  Forget", nullptr, &global_state::settings().confirm_completed_file_operations_forget);
                setting_change |= imgui::MenuItem("[File Operations]  Forget group", nullptr, &global_state::settings().confirm_completed_file_operations_forget_group);
                setting_change |= imgui::MenuItem("[File Operations]  Forget all", nullptr, &global_state::settings().confirm_completed_file_operations_forget_all);
                setting_change |= imgui::MenuItem("[Theme Editor]     Reset colors", nullptr, &global_state::settings().confirm_theme_editor_color_reset);
                setting_change |= imgui::MenuItem("[Theme Editor]     Reset style", nullptr, &global_state::settings().confirm_theme_editor_style_reset);

                imgui::EndMenu();
            }

            imgui::EndMenu();
        }

        if (setting_change) {
            (void) global_state::settings().save_to_disk();
        }

        imgui::EndMainMenuBar();
    }
}