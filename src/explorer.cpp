#ifndef SWAN_EXPLORER_CPP
#define SWAN_EXPLORER_CPP

#include <array>
#include <string_view>
#include <algorithm>
#include <vector>

#include <Windows.h>
#include "Shlwapi.h"

#include "imgui/imgui.h"

#include "primitives.hpp"
#include "on_scope_exit.hpp"
#include "options.hpp"

#include "util.cpp"

typedef std::array<char, MAX_PATH> path_t;

struct directory_entry
{
    bool is_directory = 0;
    bool is_selected = 0;
    path_t path = {};
    u64 size = 0;
};

struct explorer_window
{
    bool show = false;
    std::array<char, 1024> filter = {};
    path_t working_dir = {};
    std::vector<directory_entry> dir_entries = {};
    char const *name = "";
    u64 num_file_searches = 0;
    u64 last_selected_dirent_idx = 0;
};

enum class path_append_result : i32
{
    success = 0,
    nil,
    exceeds_max_path,
};

path_append_result path_append(path_t &path, char const *str)
{
    u64 path_len = strlen(path.data());
    u64 str_len = strlen(str);
    u64 final_len_without_nul = path_len + str_len;

    if (final_len_without_nul > MAX_PATH) {
        return path_append_result::exceeds_max_path;
    }

    (void) strncat(path.data(), str, str_len);

    return path_append_result::success;
}

bool path_ends_with(path_t const &path, char const *chars)
{
    u64 len = strlen(path.data());
    char last_ch = path[len - 1];
    return strchr(chars, last_ch);
}

static
void update_dir_entries(explorer_window *expl_ptr, std::string_view parent_dir)
{
    IM_ASSERT(expl_ptr != nullptr);
    explorer_window &expl = *expl_ptr;

    expl.dir_entries.clear();

    WIN32_FIND_DATAA find_data;

    // TODO: make more efficient
    while (parent_dir.ends_with(' ')) {
        parent_dir = std::string_view(parent_dir.data(), parent_dir.size() - 1);
    }

    if (!directory_exists(parent_dir.data())) {
        debug_log("%s: directory [%s] doesn't exist", expl.name, parent_dir.data());
        return;
    }

    static std::string search_path{};
    search_path.reserve(parent_dir.size() + strlen("\\*"));
    search_path = parent_dir;
    search_path += "\\*";

    debug_log("%s: search_path = [%s]", expl.name, search_path.c_str());

    HANDLE find_handle = FindFirstFileA(search_path.data(), &find_data);
    auto find_handle_cleanup_routine = make_on_scope_exit([&find_handle] { FindClose(find_handle); });

    if (find_handle == INVALID_HANDLE_VALUE) {
        debug_log("%s: find_handle == INVALID_HANDLE_VALUE", expl.name);
    }

    do {
        ++expl.num_file_searches;

        directory_entry entry;

        entry.is_directory = find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;

        entry.size = static_cast<u64>(find_data.nFileSizeHigh) << 32;
        entry.size |= static_cast<u64>(find_data.nFileSizeLow);

        std::memcpy(entry.path.data(), find_data.cFileName, entry.path.size());

        if (strcmp(entry.path.data(), ".") == 0) {
            continue;
        }

        expl.dir_entries.emplace_back(entry);

        if (strcmp(entry.path.data(), "..") == 0) {
            std::swap(expl.dir_entries.back(), expl.dir_entries.front());
        }
    }
    while (FindNextFileA(find_handle, &find_data));

    std::sort(expl.dir_entries.begin(), expl.dir_entries.end(), [](directory_entry const &lhs, directory_entry const &rhs) {
        if (lhs.is_directory && rhs.is_directory) {
            return strcmp(lhs.path.data(), "..") == 0;
        }
        else {
            return lhs.is_directory;
        }
    });
}

void try_descend_to_directory(explorer_window &expl, char const *child_dir)
{
    path_t new_working_dir = expl.working_dir;

    auto app_res = path_append_result::nil;
    if (!path_ends_with(expl.working_dir, "\\/")) {
        app_res = path_append(expl.working_dir, "\\");
    }
    app_res = path_append(expl.working_dir, child_dir);

    if (app_res != path_append_result::success) {
        debug_log("%s: path_append failed, working_dir = [%s], append data = [\\%s]", expl.name, expl.working_dir.data(), child_dir);
        expl.working_dir = new_working_dir;
    } else {
        if (PathCanonicalizeA(new_working_dir.data(), expl.working_dir.data())) {
            debug_log("%s: PathCanonicalizeA success: new_working_dir = [%s]", expl.name, new_working_dir.data());
            expl.working_dir = new_working_dir;
            update_dir_entries(&expl, expl.working_dir.data());
        } else {
            debug_log("%s: PathCanonicalizeA failed", expl.name);
        }
    }
}

static
i32 cwd_text_input_callback(ImGuiInputTextCallbackData *data)
{
    if (data->EventFlag == ImGuiInputTextFlags_CallbackCharFilter) {
        static std::wstring const forbidden_chars = L"<>\"|?*";
        bool is_forbidden = forbidden_chars.find(data->EventChar) != std::string::npos;
        if (is_forbidden) {
            data->EventChar = L'\0';
        }
    }
    else if (data->EventFlag == ImGuiInputTextFlags_CallbackEdit) {
        auto expl = reinterpret_cast<explorer_window *>(data->UserData);
        debug_log("%s: ImGuiInputTextFlags_CallbackEdit, data->Buf = [%s]", expl->name, data->Buf);
        update_dir_entries(expl, data->Buf);
    }

    return 0;
}

static
void render_file_explorer(explorer_window &expl, explorer_options const &expl_opts)
{
    if (!expl.show) {
        return;
    }

    auto &io = ImGui::GetIO();

    if (!ImGui::Begin(expl.name)) {
        ImGui::End();
        return;
    }

    {
        static char const *cwd_label_with_len = "cwd(%3d):";
        static char const *cwd_label_no_len   = "cwd:     ";
        static char const *filter_label       = "filter:  ";

        if (expl_opts.show_cwd_len) {
            ImGui::Text(cwd_label_with_len, strlen(expl.working_dir.data()));
        }
        else {
            ImGui::Text(cwd_label_no_len);
        }
        ImGui::SameLine();
        ImGui::PushItemWidth(ImGui::GetColumnWidth());
        ImGui::InputText("##cwd", expl.working_dir.data(), expl.working_dir.size(),
            ImGuiInputTextFlags_CallbackCharFilter|ImGuiInputTextFlags_CallbackEdit, cwd_text_input_callback, &expl);

        ImGui::Text(filter_label);
        ImGui::SameLine();
        ImGui::PushItemWidth(ImGui::GetColumnWidth());
        ImGui::InputText("##filter", expl.filter.data(), expl.filter.size());
    }

    ImGui::Spacing();

    if (expl.dir_entries.empty()) {
        ImGui::Text("Not a directory.");
        ImGui::End();
        return;
    }

    static ImVec4 const white(255, 255, 255, 255);
    static ImVec4 const yellow(255, 255, 0, 255);

    if (ImGui::BeginTable("Entries", 3, ImGuiTableFlags_Resizable|ImGuiTableFlags_Reorderable|ImGuiTableFlags_Sortable)) {
        enum class column_id : u32 { number, path, size, };

        ImGui::TableSetupColumn("Number", 0, 0.0f, (u32)column_id::number);
        ImGui::TableSetupColumn("Path", ImGuiTableColumnFlags_DefaultSort, 0.0f, (u32)column_id::path);
        ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_DefaultSort, 0.0f, (u32)column_id::size);
        ImGui::TableHeadersRow();

        if (ImGui::IsWindowFocused() && ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            for (auto &dir_ent2 : expl.dir_entries)
                dir_ent2.is_selected = false;
        }
        else if (ImGui::IsWindowFocused() && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_A)) {
            for (auto &dir_ent2 : expl.dir_entries)
                dir_ent2.is_selected = true;
        }

        for (u64 i = 0; i < expl.dir_entries.size(); ++i) {
            auto &dir_ent = expl.dir_entries[i];

            ImGui::TableNextRow();

            {
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%zu", i + 1);
            }

            {
                ImGui::TableSetColumnIndex(1);
                ImGui::PushStyleColor(ImGuiCol_Text, dir_ent.is_directory ? yellow : white);

                if (ImGui::Selectable(dir_ent.path.data(), dir_ent.is_selected, ImGuiSelectableFlags_SpanAllColumns)) {
                    if (!io.KeyCtrl && !io.KeyShift) {
                        // entry was selected but Ctrl was not held, so deselect everything
                        for (auto &dir_ent2 : expl.dir_entries)
                            dir_ent2.is_selected = false;
                    }

                    flip_bool(dir_ent.is_selected);

                    if (io.KeyShift) {
                        // shift click, select everything between the current item and the previously clicked item

                        u64 first_idx, last_idx;

                        if (i <= expl.last_selected_dirent_idx) {
                            // prev selected item below current one
                            first_idx = i;
                            last_idx = expl.last_selected_dirent_idx;
                        }
                        else {
                            first_idx = expl.last_selected_dirent_idx;
                            last_idx = i;
                        }

                        debug_log("%s: shift click, [%zu, %zu]", expl.name, first_idx, last_idx);

                        for (u64 j = first_idx; j <= last_idx; ++j)
                            expl.dir_entries[j].is_selected = true;
                    }

                    static f64 last_click_time = 0;
                    f64 current_time = ImGui::GetTime();

                    if (current_time - last_click_time <= 0.2) {
                        if (dir_ent.is_directory) {
                            debug_log("%s: double clicked directory [%s]", expl.name, dir_ent.path.data());
                            try_descend_to_directory(expl, dir_ent.path.data());
                        }
                        else {
                            debug_log("%s: double clicked file [%s]", expl.name, dir_ent.path.data());
                            path_t target_full_path = expl.working_dir;
                            if (!path_ends_with(target_full_path, "\\/")) {
                                (void) path_append(target_full_path, "\\");
                            }
                            if (path_append_result::success == path_append(target_full_path, dir_ent.path.data())) {
                                debug_log("%s: target_full_path = [%s]", expl.name, target_full_path.data());
                                [[maybe_unused]] HINSTANCE result = ShellExecuteA(nullptr, "open", target_full_path.data(), nullptr, nullptr, SW_SHOWNORMAL);
                            }
                            else {
                                debug_log("%s: path_append failed, working_dir = [%s], append data = [\\%s]", expl.name, expl.working_dir.data(), dir_ent.path.data());
                            }
                        }
                    }
                    else {
                        debug_log("%s: selected [%s]", expl.name, dir_ent.path.data());
                    }

                    last_click_time = current_time;
                    expl.last_selected_dirent_idx = i;
                }

                ImGui::PopStyleColor();
            }

            {
                ImGui::TableSetColumnIndex(2);
                if (dir_ent.is_directory) {
                    ImGui::Text("");
                }
                else {
                    ImGui::Text("%zu", dir_ent.size);
                }
            }
        }

        if (ImGui::IsKeyPressed(ImGuiKey_Enter)) {
            auto dirent_which_enter_pressed_on = expl.dir_entries[expl.last_selected_dirent_idx];
            debug_log("%s: pressed enter on [%s]", expl.name, dirent_which_enter_pressed_on.path.data());
            if (dirent_which_enter_pressed_on.is_directory) {
                try_descend_to_directory(expl, dirent_which_enter_pressed_on.path.data());
            }
        }

        ImGui::EndTable();
    }

    ImGui::End();
}

#endif // SWAN_EXPLORER_CPP
