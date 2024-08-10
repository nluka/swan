#include "stdafx.hpp"
#include "data_types.hpp"
#include "imgui_dependent_functions.hpp"

struct directory_completion_suggestions
{
    struct match
    {
        enum class type : u8 {
            starts_with,
            substr,
        };

        type type;
        swan_path directory_name;
    };

    swan_path parent_dir = {};
    bool waiting_for_cancel_ack = false;
    bool final_sort_done = false;
    progressive_task<std::vector<match>> search_task = {};
};

static
void find_cwd_completion_suggestions(directory_completion_suggestions &completion_suggestions, swan_path search_value) noexcept
{
    auto &search_task = completion_suggestions.search_task;
    auto &parent_path_utf8 = completion_suggestions.parent_dir;

    completion_suggestions.search_task.active_token.store(true);
    SCOPE_EXIT { completion_suggestions.search_task.active_token.store(false); };

    wchar_t search_path_utf16[MAX_PATH];

    if (!utf8_to_utf16(parent_path_utf8.data(), search_path_utf16, lengthof(search_path_utf16))) {
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
        print_debug_msg("find_handle == INVALID_HANDLE_VALUE [%s]", parent_path_utf8.data());
        return;
    }

    do {
        if (search_task.cancellation_token.load() == true) {
            return;
        }

        if (!(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            continue;
        }

        swan_path found_file_name = path_create("");

        if (!utf16_to_utf8(find_data.cFileName, found_file_name.data(), found_file_name.size())) {
            continue;
        }
        if (path_equals_exactly(found_file_name, ".") || path_equals_exactly(found_file_name, "..")) {
            continue;
        }
        if (cstr_eq(found_file_name.data(), search_value.data())) {
            continue;
        }

        if (cstr_starts_with(found_file_name.data(), search_value.data())) {
            std::scoped_lock lock(search_task.result_mutex);
            search_task.result.emplace_back(directory_completion_suggestions::match::type::starts_with, found_file_name);
        }
        else if (strstr(found_file_name.data(), search_value.data())) {
            std::scoped_lock lock(search_task.result_mutex);
            search_task.result.emplace_back(directory_completion_suggestions::match::type::substr, found_file_name);
        }
    }
    while (FindNextFileW(find_handle, &find_data));
}

#if 0
    if (!cstr_empty(path_cfind_filename(s_cwd_input.data()))) {
        // cancel existing task, do this as early as possible to maximize chance of ack being received by the time we are
        // ready to launch the new search task
        expl.cwd_completion_suggestions.search_task.cancellation_token.store(true);
        expl.cwd_completion_suggestions.waiting_for_cancel_ack = true;
        expl.cwd_completion_suggestions.final_sort_done = false;
        std::string_view parent = path_extract_location(expl.cwd.data());
        expl.cwd_completion_suggestions.parent_dir = path_create(parent.data(), parent.size());
    }
#endif

#if 0
    if (expl.cwd_completion_suggestions.waiting_for_cancel_ack) {
        if (expl.cwd_completion_suggestions.search_task.active_token.load() == false) {
            // existing task acked our cancellation and has stopped; result variable will no longer be modified and we may proceed
            expl.cwd_completion_suggestions.waiting_for_cancel_ack = false;
            expl.cwd_completion_suggestions.search_task.cancellation_token.store(false);
            expl.cwd_completion_suggestions.search_task.result.clear();

            swan_path search_value = path_create(path_cfind_filename(s_cwd_input.data()));

            auto &thread_pool = global_state::thread_pool();
            thread_pool.push_task(find_cwd_completion_suggestions, std::ref(expl.cwd_completion_suggestions), search_value);
        }
    }

    if (!expl.cwd_completion_suggestions.waiting_for_cancel_ack) {
        bool is_input_text_active = ImGui::IsItemActive();
        bool is_input_text_activated = ImGui::IsItemActivated();

        auto popup_label = make_str_static<64>("## cwd_input suggestions expl_%d", expl.id);

        if (is_input_text_activated) {
            imgui::OpenPopup(popup_label.data());
        }

        auto &style = imgui::GetStyle();
        imgui::ScopedStyle<ImVec2> wp(style.WindowPadding, style.FramePadding);

        char const *last_sep = strrchr(s_cwd_input.data(), dir_sep_utf8);
        f32 dist_to_last_sep = imgui::CalcTextSize(s_cwd_input.data(), last_sep + 1).x;

        imgui::SetNextWindowPos(ImVec2( imgui::GetItemRectMin().x + dist_to_last_sep, imgui::GetItemRectMax().y ));

        ImGuiWindowFlags window_flags =
            ImGuiWindowFlags_NoTitleBar|
            ImGuiWindowFlags_NoMove|
            ImGuiWindowFlags_NoResize|
            ImGuiWindowFlags_ChildWindow|
            ImGuiWindowFlags_NoSavedSettings
            // ImGuiWindowFlags_NoNav
        ;

        if (imgui::BeginPopup(popup_label.data(), window_flags)) {
            bool still_searching = expl.cwd_completion_suggestions.search_task.active_token.load() == true;
            bool needs_sort = still_searching || !expl.cwd_completion_suggestions.final_sort_done;

            // if we are no longer searching, this next sort will be the final one.
            // set this flag to avoid redundant sorts thereafter.
            expl.cwd_completion_suggestions.final_sort_done = !still_searching;

            std::scoped_lock lock(expl.cwd_completion_suggestions.search_task.result_mutex);

            using match_t = directory_completion_suggestions::match;
            auto &matches = expl.cwd_completion_suggestions.search_task.result;

            if (needs_sort) {
                std::sort(matches.begin(), matches.end(), [](match_t const &left, match_t const &right) noexcept {
                    if (left.type != right.type) {
                        return (u8)left.type < (u8)right.type;
                    }
                    return path_length(left.directory_name) < path_length(right.directory_name);
                });
            }

            // if (matches.empty()) {
            //     imgui::TextUnformatted("No matches");
            // }
            // else
            for (u64 i = 0; i < matches.size(); ++i) {
                auto const &match = matches[i];
                auto label = make_str_static<1200>("%s ## %zu", match.directory_name.data(), i);

                if (imgui::Selectable(label.data()) || (is_input_text_enter_pressed && i == 0)) {
                    // imgui::ClearActiveID(); //? Not sure what this does, seems to have no effect

                    while (path_pop_back_if_not(s_cwd_input, dir_sep_utf8));
                    (void) path_append(s_cwd_input, match.directory_name.data());

                    expl.cwd = s_cwd_input;

                    auto [cwd_exists, _] = expl.update_cwd_entries(query_filesystem, expl.cwd.data());
                    cwd_exists_after_edit = cwd_exists;
                    if (path_is_empty(expl.latest_valid_cwd) || !path_loosely_same(expl.cwd, expl.latest_valid_cwd)) {
                        expl.advance_history(expl.cwd);
                    }
                    expl.set_latest_valid_cwd(expl.cwd); // this may mutate filter
                    (void) expl.update_cwd_entries(filter, expl.cwd.data());
                }
            }

            if (is_input_text_enter_pressed || (!is_input_text_active && !imgui::IsWindowFocused())) {
                bool restore_focus_to_window_under_popup = false;
                //   ^^ To prevent window focus from bouncing back when clicking to different window while our suggestions popup is open

                imgui::CloseCurrentPopup(restore_focus_to_window_under_popup);
            }

            imgui::EndPopup();
        }
    }
#endif
