#include "stdafx.hpp"
#include "data_types.hpp"
#include "common_fns.hpp"
#include "imgui_specific.hpp"
#include "path.hpp"

static circular_buffer<recent_file> s_recent_files = circular_buffer<recent_file>(100);
circular_buffer<recent_file> &global_state::recent_files() noexcept { return s_recent_files; }

bool global_state::save_recent_files_to_disk() noexcept
try {
    std::filesystem::path full_path = global_state::execution_path() / "data\\recent_files.txt";

    std::ofstream iss(full_path);

    if (!iss) {
        return false;
    }

    auto const &recent_files = global_state::recent_files();
    for (auto const &file : recent_files) {
        auto time_t = std::chrono::system_clock::to_time_t(file.open_time);
        std::tm tm = *std::localtime(&time_t);
        iss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << ' ' << path_length(file.path) << ' ' << file.path.data() << '\n';
    }

    print_debug_msg("SUCCESS global_state::save_recent_files_to_disk");
    return true;
}
catch (...) {
    print_debug_msg("FAILED global_state::save_recent_files_to_disk");
    return false;
}

std::pair<bool, u64> global_state::load_recent_files_from_disk() noexcept
try {
    std::filesystem::path full_path = global_state::execution_path() / "data\\recent_files.txt";

    std::ifstream in(full_path);

    if (!in) {
        return { false, 0 };
    }

    auto &recent_files = global_state::recent_files();

    recent_files.clear();

    std::string line = {};
    line.reserve(global_state::page_size() - 1);

    u64 num_loaded_successfully = 0;

    while (std::getline(in, line)) {
        std::istringstream iss(line);

        // std::time_t time = {};
        u64 path_len = 0;
        swan_path_t path = {};

        std::tm tm = {};
        iss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
        system_time_point_t stored_time = std::chrono::system_clock::from_time_t(std::mktime(&tm));

        iss.ignore(1);
        iss >> (u64 &)path_len;

        iss.ignore(1);
        iss.read(path.data(), path_len);

        recent_files.push_back({ stored_time, path });
        ++num_loaded_successfully;

        line.clear();
    }

    print_debug_msg("SUCCESS global_state::load_recent_files_from_disk, loaded %zu files", num_loaded_successfully);
    return { true, num_loaded_successfully };
}
catch (...) {
    print_debug_msg("FAILED global_state::load_recent_files_from_disk");
    return { false, 0 };
}

void swan_windows::render_recent_files(bool &open) noexcept
{
    if (!imgui::Begin(swan_windows::get_name(swan_windows::recent_files), &open)) {
        imgui::End();
        return;
    }

    if (imgui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)) {
        global_state::save_focused_window(swan_windows::recent_files);
    }

    auto &recent_files = global_state::recent_files();
    system_time_point_t current_time = current_time_system();
    u64 remove_idx = u64(-1);
    u64 move_to_front_idx = u64(-1);

    if (imgui::BeginTable("recent_files", 4, ImGuiTableFlags_SizingStretchProp|ImGuiTableFlags_BordersV|ImGuiTableFlags_Hideable|ImGuiTableFlags_Reorderable|ImGuiTableFlags_Resizable)) {
        imgui::TableSetupColumn("#");
        imgui::TableSetupColumn("Opened");
        imgui::TableSetupColumn("File Name");
        imgui::TableSetupColumn("Full Path");
        imgui::TableHeadersRow();

        u64 n = 0;
        for (auto const &file : recent_files) {
            char const *file_name = cget_file_name(file.path.data());
            char const *full_path = file.path.data();
            auto directory = get_everything_minus_file_name(full_path);
            swan_path_t file_directory = path_create(directory.data(), directory.size());

            bool left_clicked = false;
            bool right_clicked = false;

            imgui::TableNextColumn();
            imgui::Text("%zu", ++n);

            imgui::TableNextColumn();
            {
                auto when = compute_when_str(file.open_time, current_time);
                imgui::TextUnformatted(when.data());
            }

            imgui::TableNextColumn();
            {
                char buffer[2048];
                (void) snprintf(buffer, lengthof(buffer), "%s##recent_file_%zu", file_name, n);
                imgui::Selectable(buffer, &left_clicked, ImGuiSelectableFlags_SpanAllColumns);
                right_clicked = imgui::IsItemClicked(ImGuiMouseButton_Right);
            }

            imgui::TableNextColumn();
            {
                char buffer[2048];
                (void) snprintf(buffer, lengthof(buffer), "%s##recent_file_%zu", full_path, n);
                imgui::Selectable(buffer, &left_clicked, ImGuiSelectableFlags_SpanAllColumns);
                right_clicked = imgui::IsItemClicked(ImGuiMouseButton_Right);
            }

            if (left_clicked) {
                print_debug_msg("left clicked recent file #%zu [%s]", n, full_path);

                auto res = open_file(file_name, file_directory.data());

                if (res.success) {
                    move_to_front_idx = n-1;
                } else {
                    swan_popup_modals::open_error(make_str("Open file [%s].", full_path).c_str(), res.error_or_utf8_path.c_str());
                    remove_idx = n-1;
                }
            }
            if (right_clicked) {
                auto &expl = global_state::explorers()[0];

                wchar_t file_path_utf16[MAX_PATH];
                s32 written = utf8_to_utf16(full_path, file_path_utf16, lengthof(file_path_utf16));

                if (written == 0) {
                    // TODO: handle error
                }
                else {
                    auto file_exists = PathFileExistsW(file_path_utf16);

                    if (!file_exists) {
                        swan_popup_modals::open_error(make_str("Open file location [%s].", full_path).c_str(), "File not found.");
                        remove_idx = n-1;
                    }
                    else {
                        expl.deselect_all_cwd_entries();
                        swan_path_t to_select = path_create(file_name);
                        expl.entries_to_select.push_back(to_select);

                        bool file_directory_exists = expl.update_cwd_entries(full_refresh, file_directory.data());

                        if (!file_directory_exists) {
                            swan_popup_modals::open_error(make_str("Open file location [%s].", full_path).c_str(),
                                                          make_str("Directory [%s] not found.", file_directory.data()).c_str());
                            remove_idx = n-1;
                        }
                        else {
                            expl.cwd = path_create(file_directory.data());
                            expl.push_history_item(expl.cwd);
                            (void) expl.save_to_disk();
                            expl.scroll_to_first_selected_entry_next_frame = true;
                            global_state::settings().show.explorer_0 = true;
                            imgui::SetWindowFocus(expl.name);
                        }
                    }
                }
            }
        }
        imgui::EndTable();
    }

    if (!recent_files.empty()) {
        if (imgui::Button("Clear##recent_files")) {
            recent_files.clear();
            (void) global_state::save_recent_files_to_disk();
        }
    }

    if (remove_idx != u64(-1)) {
        recent_files.erase(recent_files.begin() + remove_idx);
        (void) global_state::save_recent_files_to_disk();
    }
    if (move_to_front_idx != u64(-1)) {
        move_recent_file_to_front_and_save(move_to_front_idx);
    }

    imgui::End();
}

void move_recent_file_to_front_and_save(u64 recent_file_idx) noexcept
{
    auto &recent_files = global_state::recent_files();
    auto temp = recent_files[recent_file_idx];
    temp.open_time = current_time_system();
    recent_files.erase(recent_files.begin() + recent_file_idx);
    recent_files.push_front(temp);
    (void) global_state::save_recent_files_to_disk();
}
