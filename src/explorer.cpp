#ifndef SWAN_EXPLORER_CPP
#define SWAN_EXPLORER_CPP

#include <array>
#include <string_view>
#include <algorithm>
#include <vector>
#include <cstdint>
#include <regex>
#include <deque>
#include <sstream>

#include <Windows.h>
#include "Shlwapi.h"

#include "imgui/imgui.h"

#include "primitives.hpp"
#include "on_scope_exit.hpp"
#include "options.hpp"
#include "scoped_timer.hpp"

#include "util.cpp"

typedef std::array<char, MAX_PATH> path_t;

struct directory_entry
{
    bool is_filtered_out;
    bool is_directory;
    bool is_symbolic_link;
    bool is_selected;
    path_t path;
    u64 size;
};

enum filter_mode : i32
{
    contains,
    regex,
    // glob,
    count,
};

struct explorer_window
{
    char const *name;
    bool show;

    path_t cwd; // current working directory
    std::vector<directory_entry> cwd_entries; // all direct children of the cwd
    u64 cwd_prev_selected_dirent_idx; // idx of most recently clicked cwd entry, NO_SELECTION means there isn't one
    static u64 const NO_SELECTION = UINT64_MAX;

    std::deque<path_t> wd_history; // history for cwd (back/forward arrows)
    u64 wd_history_pos; // where in the cwd history we are (back/forward arrows)

    std::string filter;
    std::string filter_error;
    filter_mode filter_mode;
    bool filter_case_sensitive;

    // [DEBUG]
    u64 num_file_searches;
    LARGE_INTEGER last_refresh_timestamp;
    f64 filter_time_us;
};

u64 path_length(path_t const &path) noexcept
{
    return strlen(path.data());
}

enum class path_append_result : i32
{
    nil = -1,
    success = 0,
    exceeds_max_path,
};

path_append_result path_append(path_t &path, char const *str)
{
    u64 str_len = strlen(str);
    u64 final_len_without_nul = path_length(path) + str_len;

    if (final_len_without_nul > MAX_PATH) {
        return path_append_result::exceeds_max_path;
    }

    (void) strncat(path.data(), str, str_len);

    return path_append_result::success;
}

bool path_ends_with(path_t const &path, char const *chars)
{
    u64 len = path_length(path);
    char last_ch = path[len - 1];
    return strchr(chars, last_ch);
}

void path_clear(path_t &path)
{
    path[0] = '\0';
}

// TODO: consider adding an options struct argument to control whether the filesystem is queried
//       or filtration is performed, etc. as it may be relevant for efficiency
static
void update_cwd_entries(explorer_window *expl_ptr, std::string_view parent_dir, explorer_options const &opts)
{
    IM_ASSERT(expl_ptr != nullptr);

    scoped_timer<scoped_timer_unit::MICROSECONDS> timer("update_cwd_entries", nullptr, &std::cout);

    explorer_window &expl = *expl_ptr;

    expl.cwd_entries.clear();
    expl.filter_error.clear();

    WIN32_FIND_DATAA find_data;

    // TODO: make more efficient
    while (parent_dir.ends_with(' ')) {
        parent_dir = std::string_view(parent_dir.data(), parent_dir.size() - 1);
    }

    if (!directory_exists(parent_dir.data())) {
        debug_log("%s: update_cwd_entries(): directory [%s] doesn't exist", expl.name, parent_dir.data());
        return;
    }

    static std::string search_path{};
    search_path.reserve(parent_dir.size() + strlen("\\*"));
    search_path = parent_dir;
    search_path += "\\*";

    debug_log("%s: update_cwd_entries(): querying filesystem, search_path = [%s]", expl.name, search_path.c_str());

    HANDLE find_handle = FindFirstFileA(search_path.data(), &find_data);
    auto find_handle_cleanup_routine = make_on_scope_exit([&find_handle] { FindClose(find_handle); });

    if (find_handle == INVALID_HANDLE_VALUE) {
        debug_log("%s: update_cwd_entries(): find_handle == INVALID_HANDLE_VALUE", expl.name);
    }

    do {
        ++expl.num_file_searches;

        directory_entry entry = {};

        std::memcpy(entry.path.data(), find_data.cFileName, entry.path.size());

        entry.is_directory = find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;
        entry.is_symbolic_link = path_ends_with(entry.path, ".lnk");

        entry.size = static_cast<u64>(find_data.nFileSizeHigh) << 32;
        entry.size |= static_cast<u64>(find_data.nFileSizeLow);

        if (strcmp(entry.path.data(), ".") == 0) {
            continue;
        }

        if (strcmp(entry.path.data(), "..") == 0) {
            if (opts.show_dotdot_dir) {
                expl.cwd_entries.emplace_back(entry);
                std::swap(expl.cwd_entries.back(), expl.cwd_entries.front());
            }
        } else {
            expl.cwd_entries.emplace_back(entry);
        }
    }
    while (FindNextFileA(find_handle, &find_data));

    std::sort(expl.cwd_entries.begin(), expl.cwd_entries.end(), [](directory_entry const &lhs, directory_entry const &rhs) {
        if (lhs.is_directory && rhs.is_directory) {
            return strcmp(lhs.path.data(), "..") == 0;
        }
        else {
            return lhs.is_directory;
        }
    });

    if (!expl.filter.empty()) {
        switch (expl.filter_mode) {
            default:
            case filter_mode::contains: {
                {
                    char const *(*strstr_scase)(char const *, char const *) = strstr;
                    auto matcher = expl.filter_case_sensitive ? strstr_scase : strstr_icase;

                    scoped_timer<scoped_timer_unit::MICROSECONDS> filter_timer(nullptr, &expl.filter_time_us);

                    for (auto &dir_ent : expl.cwd_entries) {
                        dir_ent.is_filtered_out = !matcher(dir_ent.path.data(), expl.filter.c_str());
                    }
                }
                break;
            }

            case filter_mode::regex: {
                static std::regex filter_regex;
                try {
                    scoped_timer<scoped_timer_unit::MICROSECONDS> regex_ctor_timer("filter_regex ctor", nullptr, &std::cout);
                    filter_regex = expl.filter;
                }
                catch (std::exception const &except) {
                    debug_log("%s: update_cwd_entries(): error constructing std::regex, %s", expl.name, except.what());
                    expl.filter_error = except.what();
                    break;
                }
                {
                    auto match_flags = std::regex_constants::match_default | (std::regex_constants::icase * (expl.filter_case_sensitive == 0));

                    scoped_timer<scoped_timer_unit::MICROSECONDS> filter_timer(nullptr, &expl.filter_time_us);

                    for (auto &dir_ent : expl.cwd_entries) {
                        dir_ent.is_filtered_out = !std::regex_match(
                            dir_ent.path.data(),
                            filter_regex,
                            (std::regex_constants::match_flag_type)match_flags
                        );
                    }
                }
                break;
            }

            // case filter_mode::glob: {
            //     throw std::runtime_error("not implemented");
            //     break;
            // }
        }
    }

    (void) QueryPerformanceCounter(&expl.last_refresh_timestamp);
}

explorer_window create_default_explorer_windows(
    char const *name,
    bool show,
    path_t starting_dir,
    explorer_options const &opts
) {
    explorer_window w;

    w.show = show;
    w.name = name;

    w.cwd = starting_dir;
    w.cwd_entries = {};
    w.cwd_entries.reserve(1024);
    w.cwd_prev_selected_dirent_idx = explorer_window::NO_SELECTION;

    w.wd_history = {};
    w.wd_history.push_back(starting_dir);
    w.wd_history_pos = 0;

    w.filter = std::string(1024, '\0');
    w.filter_error = "";
    w.filter_mode = filter_mode::contains;
    w.filter_case_sensitive = false;

    w.num_file_searches = 0;
    w.last_refresh_timestamp = {};
    w.filter_time_us = 0.f;

    update_cwd_entries(&w, starting_dir.data(), opts);

    return w;
}

void new_history_from(explorer_window &expl, path_t const &new_latest_entry)
{
    u64 num_trailing_history_items_to_del = expl.wd_history.size() - expl.wd_history_pos - 1;

    for (u64 i = 0; i < num_trailing_history_items_to_del; ++i) {
        expl.wd_history.pop_back();
    }

    if (MAX_EXPLORER_WD_HISTORY == expl.wd_history.size()) {
        expl.wd_history.pop_front();
    } else {
        ++expl.wd_history_pos;
    }
    expl.wd_history.push_back(new_latest_entry);

#if !defined(NDEBUG)
    std::stringstream hist_ss = {};

    for (u64 i = 0; i < expl.wd_history.size(); ++i) {
        hist_ss << i << ":[" << expl.wd_history[i].data() << "], ";
    }

    std::string hist_str = hist_ss.str();
    if (!hist_str.empty()) {
        // remove trailing ", "
        hist_str.pop_back();
        hist_str.pop_back();
    }

    debug_log("%s: history (len=%zu, pos=%zu): %s", expl.name, expl.wd_history.size(), expl.wd_history_pos, hist_str.c_str());
#endif
}

void try_descend_to_directory(explorer_window &expl, char const *child_dir, explorer_options const &opts)
{
    path_t new_working_dir = expl.cwd;

    auto app_res = path_append_result::nil;
    if (!path_ends_with(expl.cwd, "\\/")) {
        app_res = path_append(expl.cwd, "\\");
    }
    app_res = path_append(expl.cwd, child_dir);

    if (app_res != path_append_result::success) {
        debug_log("%s: path_append failed, cwd = [%s], append data = [\\%s]", expl.name, expl.cwd.data(), child_dir);
        expl.cwd = new_working_dir;
    }
    else {
        if (PathCanonicalizeA(new_working_dir.data(), expl.cwd.data())) {
            debug_log("%s: PathCanonicalizeA success: new_working_dir = [%s]", expl.name, new_working_dir.data());

            update_cwd_entries(&expl, new_working_dir.data(), opts);

            new_history_from(expl, new_working_dir);
            expl.cwd = new_working_dir;
            expl.cwd_prev_selected_dirent_idx = UINT64_MAX;
        }
        else {
            debug_log("%s: PathCanonicalizeA failed", expl.name);
        }
    }
}

struct cwd_text_input_callback_user_data
{
    explorer_window *expl_ptr;
    explorer_options *opts_ptr;
};

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
        auto user_data = (cwd_text_input_callback_user_data *)(data->UserData);
        auto expl_ptr = user_data->expl_ptr;
        auto opts_pts = user_data->opts_ptr;

        debug_log("UserData: %p expl: %p opts: %p", data->UserData, expl_ptr, opts_pts);
        debug_log("%s: ImGuiInputTextFlags_CallbackEdit, data->Buf = [%s]", expl_ptr->name, data->Buf);
        update_cwd_entries(user_data->expl_ptr, data->Buf, *user_data->opts_ptr);
    }

    return 0;
}

// static
// f64 compute_diff_ms(LARGE_INTEGER start, LARGE_INTEGER end)
// {
//     LARGE_INTEGER frequency;
//     (void) QueryPerformanceFrequency(&frequency);

//     LONGLONG ticks = end.QuadPart - start.QuadPart;
//     f64 milliseconds = f64(ticks) * f64(1000) / f64(frequency.QuadPart);
//     return milliseconds;
// }

static
void render_file_explorer(explorer_window &expl, explorer_options &opts)
{
    if (!expl.show) {
        return;
    }

    if (!ImGui::Begin(expl.name)) {
        ImGui::End();
        return;
    }

    static ImVec4 const white(1, 1, 1, 1);
    static ImVec4 const yellow(1, 1, 0, 1);
    static ImVec4 const orange(1, 0.5f, 0, 1);
    static ImVec4 const red(1, 0.2f, 0, 1);

    auto &io = ImGui::GetIO();
    bool window_focused = ImGui::IsWindowFocused();

    // handle enter key pressed on cwd entry
    if (window_focused && ImGui::IsKeyPressed(ImGuiKey_Enter)) {
        if (UINT64_MAX == expl.cwd_prev_selected_dirent_idx) {
            debug_log("%s: pressed enter but cwd_prev_selected_dirent_idx was NO_SELECTION", expl.name);
        } else {
            auto dirent_which_enter_pressed_on = expl.cwd_entries[expl.cwd_prev_selected_dirent_idx];
            debug_log("%s: pressed enter on [%s]", expl.name, dirent_which_enter_pressed_on.path.data());
            if (dirent_which_enter_pressed_on.is_directory) {
                try_descend_to_directory(expl, dirent_which_enter_pressed_on.path.data(), opts);
            }
        }
    }

    // refresh button, ctrl-r refresh logic, automatic refreshing
    {
        bool refreshed = false; // to avoid refreshing twice in one frame

        auto refresh = [&]() {
            if (!refreshed) {
                update_cwd_entries(&expl, expl.cwd.data(), opts);
                refreshed = true;
            }
        };

        if (window_focused && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_R)) {
            debug_log("%s: ctrl-r, refresh", expl.name);
            refresh();
        }

        if (ImGui::Button("Refresh") && !refreshed) {
            debug_log("%s: refresh triggered", expl.name);
            refresh();
        }

        // if (!refreshed) {
        //     // see if it's time for an automatic refresh
        //     LARGE_INTEGER current_timestamp = {};
        //     (void) QueryPerformanceCounter(&current_timestamp);
        //     f64 diff_ms = compute_diff_ms(expl.last_refresh_timestamp, current_timestamp);
        //     if (diff_ms >= f64(1500)) {
        //         debug_log("%s: automatic refresh triggered (%lld)", expl.name, current_timestamp.QuadPart);
        //         update_cwd_entries(&expl, expl.cwd.data());
        //     }
        // }
    }

    ImGui::SameLine();

    // history back arrow start
    {
        ImGui::BeginDisabled(expl.wd_history_pos == 0);

        if (ImGui::ArrowButton("Back", ImGuiDir_Left)) {
            debug_log("%s: back arrow button triggered", expl.name);

            if (expl.wd_history_pos == 0) {
                debug_log("%s: already at front of history deque", expl.name);
            } else {
                --expl.wd_history_pos;
                expl.cwd = expl.wd_history[expl.wd_history_pos];
                debug_log("%s: new wd_history_pos (--) = %zu, new cwd = [%s]", expl.name, expl.wd_history_pos, expl.cwd.data());
                update_cwd_entries(&expl, expl.cwd.data(), opts);
            }
        }

        ImGui::EndDisabled();
    }
    // history back arrow end

    ImGui::SameLine();

    // history forward arrow
    {
        ImGui::BeginDisabled(expl.wd_history_pos == expl.wd_history.size() - 1);

        if (ImGui::ArrowButton("Forward", ImGuiDir_Right)) {
            debug_log("%s: forward arrow button triggered", expl.name);

            if (expl.wd_history_pos == expl.wd_history.size() - 1) {
                debug_log("%s: already at back of history deque", expl.name);
            } else {
                ++expl.wd_history_pos;
                expl.cwd = expl.wd_history[expl.wd_history_pos];
                debug_log("%s: new wd_history_pos (++) = %zu, new cwd = [%s]", expl.name, expl.wd_history_pos, expl.cwd.data());
                update_cwd_entries(&expl, expl.cwd.data(), opts);
            }
        }

        ImGui::EndDisabled();
    }
    // history forward end

    ImGui::SameLine();
    ImGui::Spacing();
    ImGui::SameLine();

    // cwd filter start
    {
        ImGui::Text("filter:");
        ImGui::SameLine();

    #if 0
        ImGui::Text("Filter Mode:");
        ImGui::SameLine();
        ImGui::RadioButton("Contains", (i32 *)(&expl.filter_mode), filter_mode::contains);
        ImGui::SameLine();
        ImGui::RadioButton("RegExp", (i32 *)(&expl.filter_mode), filter_mode::regex);
        ImGui::SameLine();
        ImGui::RadioButton("Glob", (i32 *)(&expl.filter_mode), filter_mode::glob);
    #else
        // important that they are all the same length,
        // this assumption is leveraged for calculation of combo box width
        static char const *filter_modes[] = {
            "Contains",
            "RegExp  ",
        };
        ImVec2 max_dropdown_elem_size = ImGui::CalcTextSize(filter_modes[0]);

        ImGui::PushItemWidth(max_dropdown_elem_size.x + 30.f); // some extra for the dropdown button
        ImGui::Combo("##filter_mode", (i32 *)(&expl.filter_mode), filter_modes, lengthof(filter_modes));
        ImGui::PopItemWidth();
    #endif
    }
    // cwd filter end

    ImGui::SameLine();

    // case sensitivity button start
    {
        if (ImGui::Button(expl.filter_case_sensitive ? "aa" : "aA")) {
            flip_bool(expl.filter_case_sensitive);
            update_cwd_entries(&expl, expl.cwd.data(), opts);
        }
    }
    // case sensitivity button start

    ImGui::SameLine();

    // filter text input start
    {
        ImGui::PushItemWidth(max(
            ImGui::CalcTextSize(expl.filter.c_str()).x + (ImGui::GetStyle().FramePadding.x * 2) + 10.f,
            ImGui::CalcTextSize("12345678").x
        ));
        if (ImGui::InputText("##filter", expl.filter.data(), expl.filter.size())) {
            debug_log("%s: filter InputText active", expl.name);
            update_cwd_entries(&expl, expl.cwd.data(), opts);
        }
        ImGui::PopItemWidth();
    }
    // filter text input start

    ImGui::Spacing();
    ImGui::Spacing();

    static char const *clicknav_label     = "clicknav:";
    static char const *cwd_label_with_len = "cwd(%3d):";
    static char const *cwd_label_no_len   = "     cwd:";

    // clicknav start
    {
        if (!expl.cwd_entries.empty()) {
            static std::vector<char const *> slices(50);
            slices.clear();

            path_t sliced_path = expl.cwd;
            char const *slice = strtok(sliced_path.data(), "\\/");
            while (slice != nullptr) {
                slices.push_back(slice);
                slice = strtok(nullptr, "\\/");
            }

            auto cd_to_slice = [&expl, &sliced_path](char const *slice) {
                char const *slice_end = slice;
                while (*slice_end != '\0') {
                    ++slice_end;
                }

                u64 len = slice_end - sliced_path.data();

                if (len == path_length(expl.cwd)) {
                    debug_log("%s: cd_to_slice: slice == cwd, not updating cwd|history", expl.name);
                }
                else {
                    expl.cwd[len] = '\0';
                    new_history_from(expl, expl.cwd);
                }
            };

            f32 original_spacing = ImGui::GetStyle().ItemSpacing.x;
            if (slices.size() > 1) {
                ImGui::GetStyle().ItemSpacing.x = 2;
            }

            ImGui::Text(clicknav_label);
            ImGui::SameLine();
            for (auto slice_it = slices.begin(); slice_it != slices.end() - 1; ++slice_it) {
                if (ImGui::Button(*slice_it)) {
                    debug_log("%s: clicked slice [%s]", expl.name, *slice_it);
                    cd_to_slice(*slice_it);
                    update_cwd_entries(&expl, expl.cwd.data(), opts);
                }
                ImGui::SameLine();
                ImGui::Text("\\");
                ImGui::SameLine();
            }
            ImGui::SameLine();
            if (ImGui::Button(slices.back())) {
                debug_log("%s: clicked slice [%s]", expl.name, slices.back());
                cd_to_slice(slices.back());
                update_cwd_entries(&expl, expl.cwd.data(), opts);
            }

            // TODO: ImGui::Combo() for all directories in expl.cwd

            if (slices.size() > 1) {
                ImGui::GetStyle().ItemSpacing.x = original_spacing;
            }
        }
    }
    // clicknav end

    ImGui::Spacing();
    ImGui::Spacing();

    // cwd text input start
    {
        cwd_text_input_callback_user_data user_data;
        user_data.expl_ptr = &expl;
        user_data.opts_ptr = &opts;

        // label
        if (opts.show_cwd_len) {
            ImGui::Text(cwd_label_with_len, path_length(expl.cwd));
        } else {
            ImGui::Text(cwd_label_no_len);
        }

        ImGui::SameLine();

        // input
        ImGui::PushItemWidth(ImGui::CalcTextSize(expl.cwd.data()).x + (ImGui::GetStyle().FramePadding.x * 2) + 10.f);
        ImGui::InputText("##cwd", expl.cwd.data(), expl.cwd.size(),
            ImGuiInputTextFlags_CallbackCharFilter|ImGuiInputTextFlags_CallbackEdit, cwd_text_input_callback, (void *)&user_data);
        ImGui::PopItemWidth();
    }
    // cwd text input

    ImGui::Spacing();
    ImGui::Spacing();

    // cwd entries stats & table start

    if (!directory_exists(expl.cwd.data())) {
        ImGui::TextColored(orange, "Invalid directory.");
    }
    else if (expl.cwd_entries.empty()) {
        // cwd exists but is empty
        ImGui::TextColored(orange, "Empty directory.");
    }
    else {
        u64 num_selected_directories = 0;
        u64 num_selected_symlinks = 0;
        u64 num_selected_files = 0;

        u64 num_filtered_directories = 0;
        u64 num_filtered_symlinks = 0;
        u64 num_filtered_files = 0;

        u64 num_child_directories = 0;
        u64 num_child_symlinks = 0;

        for (auto const &dir_ent : expl.cwd_entries) {
            static_assert(false == 0);
            static_assert(true == 1);

            num_selected_directories += u64(dir_ent.is_selected && dir_ent.is_directory);
            num_selected_symlinks += u64(dir_ent.is_selected && dir_ent.is_symbolic_link);
            num_selected_files += u64(dir_ent.is_selected && !dir_ent.is_directory && !dir_ent.is_symbolic_link);

            num_filtered_directories += u64(dir_ent.is_filtered_out && dir_ent.is_directory);
            num_filtered_symlinks += u64(dir_ent.is_filtered_out && dir_ent.is_symbolic_link);
            num_filtered_files += u64(dir_ent.is_filtered_out && !dir_ent.is_directory && !dir_ent.is_symbolic_link);

            num_child_directories += u64(dir_ent.is_directory);
            num_child_symlinks += u64(dir_ent.is_symbolic_link);
        }

        u64 num_filtered_dirents = num_filtered_directories + num_filtered_symlinks + num_filtered_files;
        u64 num_selected_dirents = num_selected_directories + num_selected_symlinks + num_selected_files;
        u64 num_child_dirents = expl.cwd_entries.size();
        u64 num_child_files = num_child_dirents - num_child_directories - num_child_symlinks;

    #if 1
        (void) num_selected_dirents;
        (void) num_child_files;
    #else
        ImGui::Text("%zu children", num_child_dirents);
        if (num_child_dirents > 0) {
            ImGui::SameLine();
            ImGui::Text(":");
        }
        if (num_child_files > 0) {
            ImGui::SameLine();
            ImGui::Text("%zu file%s", num_child_files, num_child_files == 1 ? "" : "s");
        }
        if (num_child_directories > 0) {
            ImGui::SameLine();
            ImGui::Text("%zu director%s", num_child_directories, num_child_directories == 1 ? "y" : "ies");
        }
        if (num_child_symlinks > 0) {
            ImGui::SameLine();
            ImGui::Text("%zu shortcut%s", num_child_symlinks, num_child_symlinks == 1 ? "" : "s");
        }

        if (expl.filter_error != "") {
            ImGui::PushTextWrapPos(ImGui::GetColumnWidth());
            ImGui::TextColored(red, "%s", expl.filter_error.c_str());
            ImGui::PopTextWrapPos();
        }
        else {
            ImGui::Text("%zu filtered", num_filtered_dirents);
            if (num_filtered_dirents > 0) {
                ImGui::SameLine();
                ImGui::Text(":");
            }
            if (num_filtered_files > 0) {
                ImGui::SameLine();
                ImGui::Text("%zu file%s", num_filtered_files, num_filtered_files == 1 ? "" : "s");
            }
            if (num_filtered_directories > 0) {
                ImGui::SameLine();
                ImGui::Text("%zu director%s", num_filtered_directories, num_filtered_directories == 1 ? "y" : "ies");
            }
            if (num_filtered_symlinks > 0) {
                ImGui::SameLine();
                ImGui::Text("%zu shortcut%s", num_filtered_symlinks, num_filtered_symlinks == 1 ? "" : "s");
            }
        }

        ImGui::Text("%zu selected", num_selected_dirents);
        if (num_selected_dirents > 0) {
            ImGui::SameLine();
            ImGui::Text(":");
        }
        if (num_selected_files > 0) {
            ImGui::SameLine();
            ImGui::Text("%zu file%s", num_selected_files, num_selected_files == 1 ? "" : "s");
        }
        if (num_selected_directories > 0) {
            ImGui::SameLine();
            ImGui::Text("%zu director%s", num_selected_directories, num_selected_directories == 1 ? "y" : "ies");
        }
        if (num_selected_symlinks > 0) {
            ImGui::SameLine();
            ImGui::Text("%zu shortcut%s", num_selected_symlinks, num_selected_symlinks == 1 ? "" : "s");
        }

        ImGui::Spacing();
        ImGui::Spacing();
    #endif

        enum class column_id : i32 { number, path, size_pretty, size_bytes, count };

        if (num_filtered_dirents == expl.cwd_entries.size()) {
            ImGui::TextColored(orange, "No entries passed filter.");
        }
        else if (ImGui::BeginTable("Entries", (i32)column_id::count, ImGuiTableFlags_Resizable|ImGuiTableFlags_Reorderable|ImGuiTableFlags_Sortable)) {
            ImGui::TableSetupColumn("Number", ImGuiTableColumnFlags_DefaultSort, 0.0f, (u32)column_id::number);
            ImGui::TableSetupColumn("Path", ImGuiTableColumnFlags_NoSort, 0.0f, (u32)column_id::path);
            ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_NoSort, 0.0f, (u32)column_id::size_pretty);
            ImGui::TableSetupColumn("Bytes", ImGuiTableColumnFlags_NoSort, 0.0f, (u32)column_id::size_bytes);
            ImGui::TableHeadersRow();

            if (window_focused && ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                for (auto &dir_ent2 : expl.cwd_entries)
                    dir_ent2.is_selected = false;
            }
            else if (window_focused && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_A)) {
                for (auto &dir_ent2 : expl.cwd_entries)
                    dir_ent2.is_selected = true;
            }

            for (u64 i = 0; i < expl.cwd_entries.size(); ++i) {
                auto &dir_ent = expl.cwd_entries[i];

                if (dir_ent.is_filtered_out) {
                    ++num_filtered_dirents;
                    continue;
                }

                ImGui::TableNextRow();

                // Number (column_id::number)
                {
                    ImGui::TableSetColumnIndex((i32)column_id::number);
                    ImGui::Text("%zu", i + 1);
                }

                // Path (column_id::path)
                {
                    ImGui::TableSetColumnIndex((i32)column_id::path);
                    ImGui::PushStyleColor(ImGuiCol_Text, dir_ent.is_directory ? yellow : white);

                    if (ImGui::Selectable(dir_ent.path.data(), dir_ent.is_selected, ImGuiSelectableFlags_SpanAllColumns)) {
                        if (!io.KeyCtrl && !io.KeyShift) {
                            // entry was selected but Ctrl was not held, so deselect everything
                            for (auto &dir_ent2 : expl.cwd_entries)
                                dir_ent2.is_selected = false;
                        }

                        flip_bool(dir_ent.is_selected);

                        if (io.KeyShift) {
                            // shift click, select everything between the current item and the previously clicked item

                            u64 first_idx, last_idx;

                            if (UINT64_MAX == expl.cwd_prev_selected_dirent_idx) {
                                // nothing in cwd has been selected, so start selection from very first entry
                                expl.cwd_prev_selected_dirent_idx = 0;
                            }

                            if (i <= expl.cwd_prev_selected_dirent_idx) {
                                // prev selected item below current one
                                first_idx = i;
                                last_idx = expl.cwd_prev_selected_dirent_idx;
                            }
                            else {
                                first_idx = expl.cwd_prev_selected_dirent_idx;
                                last_idx = i;
                            }

                            debug_log("%s: shift click, [%zu, %zu]", expl.name, first_idx, last_idx);

                            for (u64 j = first_idx; j <= last_idx; ++j) {
                                expl.cwd_entries[j].is_selected = true;
                            }
                        }

                        static f64 last_click_time = 0;
                        f64 current_time = ImGui::GetTime();

                        if (current_time - last_click_time <= 0.2) {
                            if (dir_ent.is_directory) {
                                debug_log("%s: double clicked directory [%s]", expl.name, dir_ent.path.data());
                                try_descend_to_directory(expl, dir_ent.path.data(), opts);
                            }
                            else if (dir_ent.is_symbolic_link) {
                                debug_log("%s: double clicked link [%s]", expl.name, dir_ent.path.data());
                                // TODO: implement
                            }
                            else {
                                debug_log("%s: double clicked file [%s]", expl.name, dir_ent.path.data());
                                path_t target_full_path = expl.cwd;
                                if (!path_ends_with(target_full_path, "\\/")) {
                                    (void) path_append(target_full_path, "\\");
                                }
                                if (path_append_result::success == path_append(target_full_path, dir_ent.path.data())) {
                                    debug_log("%s: target_full_path = [%s]", expl.name, target_full_path.data());
                                    [[maybe_unused]] HINSTANCE result = ShellExecuteA(nullptr, "open", target_full_path.data(), nullptr, nullptr, SW_SHOWNORMAL);
                                }
                                else {
                                    debug_log("%s: path_append failed, cwd = [%s], append data = [\\%s]", expl.name, expl.cwd.data(), dir_ent.path.data());
                                }
                            }
                        }
                        else {
                            debug_log("%s: selected [%s]", expl.name, dir_ent.path.data());
                        }

                        last_click_time = current_time;
                        expl.cwd_prev_selected_dirent_idx = i;
                    }

                    ImGui::PopStyleColor();
                }

                // Size (column_id::size_pretty)
                {
                    ImGui::TableSetColumnIndex((i32)column_id::size_pretty);
                    if (dir_ent.is_directory) {
                        ImGui::Text("");
                    }
                    else {
                        std::array<char, 21> pretty_size = {};
                        format_file_size(dir_ent.size, pretty_size.data(), pretty_size.size(), opts.binary_size_system ? 1024 : 1000);
                        ImGui::Text("%s", pretty_size.data());
                    }
                }

                // Bytes (column_id::size_bytes)
                {
                    ImGui::TableSetColumnIndex((i32)column_id::size_bytes);
                    if (dir_ent.is_directory) {
                        ImGui::Text("");
                    }
                    else {
                        ImGui::Text("%zu", dir_ent.size);
                    }
                }
            }

            ImGui::EndTable();
        }
    }
    // cwd entries stats & table end

    // debug info start
    if (opts.show_debug_info) {
        for (u64 i = 0; i < 10; ++i) {
            ImGui::Spacing();
        }

        ImGui::Text("[DEBUG]");
        ImGui::Text("cwd = [%s]", expl.cwd.data());
        ImGui::Text("cwd_exists = %d", directory_exists(expl.cwd.data()));
        ImGui::Text("num_file_searches = %zu", expl.num_file_searches);
        ImGui::Text("dir_entries.size() = %zu", expl.cwd_entries.size());
        ImGui::Text("cwd_prev_selected_dirent_idx = %lld", expl.cwd_prev_selected_dirent_idx);
        ImGui::Text("filter_time_us = %.1lf", expl.filter_time_us);
    }
    // debug info end

    ImGui::End();
}

#endif // SWAN_EXPLORER_CPP
