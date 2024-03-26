#include "stdafx.hpp"
#include "common_functions.hpp"
#include "imgui_dependent_functions.hpp"
#include "scoped_timer.hpp"

namespace bulk_rename_modal_global_state
{
    static bool                                 g_open = false;
    static explorer_window *                    g_initiating_expl = nullptr;
    static std::vector<explorer_window::dirent> g_initiating_expl_selected_dirents = {};
    static std::function<void ()>               g_on_rename_callback = {};
}

void swan_popup_modals::open_bulk_rename(explorer_window &expl_opened_from, std::function<void ()> on_rename_callback) noexcept
{
    using namespace bulk_rename_modal_global_state;

    g_open = true;
    bit_set(global_state::popup_modals_open_bit_field(), swan_popup_modals::bit_pos_bulk_rename);

    g_initiating_expl_selected_dirents.clear();
    for (auto const &dirent : expl_opened_from.cwd_entries) {
        if (dirent.is_selected) {
            g_initiating_expl_selected_dirents.push_back(dirent);
        }
    }

    assert(g_initiating_expl == nullptr);
    g_initiating_expl = &expl_opened_from;

    g_on_rename_callback = on_rename_callback;
}

bool swan_popup_modals::is_open_bulk_rename() noexcept
{
    using namespace bulk_rename_modal_global_state;

    return g_open;
}

static
void set_clipboard_to_slice(ImGuiInputTextState *state) noexcept
{
    assert(state != nullptr);

    s32 start = state->GetSelectionStart();
    s32 end = state->GetSelectionEnd();

    if (start > end) { // reverse selection
        std::swap(start, end);
    }

    end -= 1;

    char slice[64];
    (void) snprintf(slice, lengthof(slice), "<%d,%d>", start, end);
    imgui::SetClipboardText(slice);
}

struct path_comparator
{
    char dir_sep_utf8;
    swan_path bulk_rename_parent_dir;

    bool operator()(bulk_rename_op const &lhs, recent_file const &rhs) const noexcept
    {
        swan_path lhs_full_path = bulk_rename_parent_dir;
        [[maybe_unused]] bool success = path_append(lhs_full_path, lhs.before->path.data(), dir_sep_utf8, true);
        assert(success);
        return strcmp(lhs_full_path.data(), rhs.path.data()) < 0;
    }
    bool operator()(recent_file const &lhs, bulk_rename_op const &rhs) const noexcept
    {
        swan_path rhs_full_path = bulk_rename_parent_dir;
        [[maybe_unused]] bool success = path_append(rhs_full_path, rhs.before->path.data(), dir_sep_utf8, true);
        assert(success);
        return strcmp(lhs.path.data(), rhs_full_path.data()) < 0;
    }
};

static
void update_recent_files(std::vector<bulk_rename_op> &renames, swan_path const &renames_parent_path) noexcept
{
    std::sort(renames.begin(), renames.end(), [](bulk_rename_op const &lhs, bulk_rename_op const &rhs) noexcept {
        return strcmp(lhs.before->path.data(), rhs.before->path.data()) < 0;
    });

    path_comparator comparator = { global_state::settings().dir_separator_utf8, renames_parent_path };

    auto recent_files = global_state::recent_files_get();
    {
        std::scoped_lock recent_files_lock(*recent_files.mutex);

        for (auto &rf : *recent_files.container) {
            auto range = std::equal_range(renames.begin(), renames.end(), rf, comparator);

            if (range.first != renames.end()) {
                bulk_rename_op const &matching_rnm = *range.first;
                swan_path rf_new_full_path = comparator.bulk_rename_parent_dir;
                if (path_append(rf_new_full_path, matching_rnm.after.data(), comparator.dir_sep_utf8, true)) {
                    rf.path = rf_new_full_path;
                }
            }
        }
    }

    (void) global_state::recent_files_save_to_disk();
}

void swan_popup_modals::render_bulk_rename() noexcept
{
    using namespace bulk_rename_modal_global_state;

    center_window_and_set_size_when_appearing(800, 600);

    if (g_open) {
        imgui::OpenPopup(swan_popup_modals::label_bulk_rename);
    }
    if (!imgui::BeginPopupModal(swan_popup_modals::label_bulk_rename, nullptr)) {
        return;
    }

    assert(g_initiating_expl != nullptr);
    assert(!g_initiating_expl_selected_dirents.empty());

    auto &expl = *g_initiating_expl;
    auto &selection = g_initiating_expl_selected_dirents;

    wchar_t dir_sep_utf16 = global_state::settings().dir_separator_utf16;

    static char s_pattern_utf8[512] = "<name><dotext>";
    static s32 s_counter_start = 1;
    static s32 s_counter_step = 1;
    static bool s_squish_adjacent_spaces = true;

    enum class bulk_rename_state : s32
    {
        nil,
        in_progress,
        done,
        cancelled,
    };

    static std::atomic<bulk_rename_state> s_rename_state = bulk_rename_state::nil;
    static std::atomic<u64> s_num_renames_success = 0;
    static std::atomic<u64> s_num_renames_fail = 0;
    static std::atomic<u64> s_num_renames_total = 0;

    static bool s_initial_computed = false;
    static bulk_rename_compile_pattern_result s_pattern_compile_res = {};
    static std::vector<bulk_rename_op> s_sorted_renames = {};
    static std::vector<bulk_rename_op> s_renames = {};
    static std::vector<bulk_rename_collision> s_collisions;
    static f64 s_transform_us = {};
    static f64 s_collisions_us = {};

    auto cleanup_and_close_popup = [&]() noexcept {
        s_rename_state.store(bulk_rename_state::nil);
        s_num_renames_success.store(0);
        s_num_renames_fail.store(0);
        s_num_renames_total.store(0);
        s_initial_computed = false;
        s_pattern_compile_res = {};
        s_renames.clear();
        s_sorted_renames.clear();
        s_collisions.clear();
        s_transform_us = {};
        s_collisions_us = {};

        g_open = false;
        bit_clear(global_state::popup_modals_open_bit_field(), swan_popup_modals::bit_pos_bulk_rename);

        g_initiating_expl = nullptr;
        g_initiating_expl_selected_dirents.clear();
        g_on_rename_callback = {};

        imgui::CloseCurrentPopup();
    };

    bool recompute = false;

    imgui::AlignTextToFramePadding();
    imgui::TextUnformatted("Pattern");
    imgui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered() && ImGui::BeginTooltip()) {
        char const *tooltip =
            "Interpolate the pattern with:\n"
            "\n"
            "Expression  Description                 Example   \n"
            "----------  --------------------------  ----------\n"
            "<name>      File name minus extension   [Song].mp3\n"
            "<ext>       File extension              Song.[mp3]\n"
            "<dotext>    Dot + file extension        Song[.mp3]\n"
            "<counter>   Uses start and step inputs            \n"
            "<bytes>     File size in bytes                    \n"
        ;

        ImGui::TextUnformatted(tooltip);
        ImGui::EndTooltip();
    }
    imgui::SameLine();
    {
        imgui::ScopedAvailWidth w = {};

        recompute |= imgui::InputText(
            "## bulk_rename_pattern", s_pattern_utf8, lengthof(s_pattern_utf8),
            ImGuiInputTextFlags_CallbackCharFilter, filter_chars_callback, (void *)L"\\/\"|?*"
            // don't filter <>, we use them for interpolating the pattern with name, counter, etc.
        );
    }

    imgui::Spacing();

    imgui::AlignTextToFramePadding();
    {
        imgui::TextUnformatted("Cnt start");
        imgui::SameLine();
        imgui::ScopedItemWidth iw(imgui::CalcTextSize("_").x * 20);
        recompute |= imgui::InputInt("## counter_start", &s_counter_start);
    }
    imgui::SameLineSpaced(1);
    {
        imgui::TextUnformatted("Cnt step");
        imgui::SameLine();
        imgui::ScopedItemWidth iw(imgui::CalcTextSize("_").x * 20);
        recompute |= imgui::InputInt("## counter_step ", &s_counter_step);
    }
    imgui::SameLineSpaced(1);
    {
        imgui::TextUnformatted("Compress spaces");
        imgui::SameLine();
        recompute |= imgui::Checkbox("## squish_adjacent_spaces", &s_squish_adjacent_spaces);
    }

    u64 num_transform_errors = 0;

    if (!s_initial_computed || recompute) {
        print_debug_msg("[ %d ] compiling pattern & recomputing renames/collisions", expl.id);

        s_renames.reserve(selection.size());
        s_renames.clear();
        s_sorted_renames.clear();

        s_collisions.reserve(selection.size());
        s_collisions.clear();

        s_pattern_compile_res = bulk_rename_compile_pattern(s_pattern_utf8, s_squish_adjacent_spaces);

        if (s_pattern_compile_res.success) {
            {
                scoped_timer<timer_unit::MICROSECONDS> tranform_timer(&s_transform_us);

                s32 counter = s_counter_start;

                for (auto &dirent : selection) {
                    file_name_extension_splitter name_ext(dirent.basic.path.data());
                    swan_path after;

                    auto transform = bulk_rename_transform(s_pattern_compile_res.compiled_pattern, after, name_ext.name,
                                                           name_ext.ext, counter, dirent.basic.size);

                    if (transform.success) {
                        s_renames.emplace_back(&dirent.basic, after.data());
                    } else {
                        ++num_transform_errors;
                    }

                    counter += s_counter_step;
                }
            }
            {
                scoped_timer<timer_unit::MICROSECONDS> find_collisions_timer(&s_collisions_us);
                auto result = bulk_rename_find_collisions(expl.cwd_entries, s_renames);
                s_collisions = result.collisions;
                s_sorted_renames = result.sorted_renames;
            }
        }

        s_initial_computed = true;
    }

    bulk_rename_state state = s_rename_state.load();
    u64 success = s_num_renames_success.load();
    u64 fail = s_num_renames_fail.load();
    u64 total = s_num_renames_total.load();

    imgui::Spacing();

    imgui::BeginDisabled(!s_pattern_compile_res.success || !s_collisions.empty() || s_pattern_utf8[0] == '\0' || state != bulk_rename_state::nil);
    bool rename_button_pressed = imgui::Button("Rename" "## bulk_perform");
    imgui::EndDisabled();

    imgui::SameLine();

    if (state == bulk_rename_state::in_progress) {
        if (imgui::Button(ICON_FA_STOP "## bulk_rename")) {
            s_rename_state.store(bulk_rename_state::cancelled);
        }
        if (imgui::IsItemHovered(0, .5f)) {
            imgui::SetTooltip("Cancel operation");
        }
    }
    else {
        if (imgui::Button(ICON_CI_X "## bulk_rename")) {
            if (state == bulk_rename_state::done) {
                update_recent_files(s_renames, g_initiating_expl->cwd);
                g_on_rename_callback();
            }
            cleanup_and_close_popup();
        }
        if (imgui::IsItemHovered(0, .5f)) {
            imgui::SetTooltip("Exit");
        }
    }

    state = s_rename_state.load();

    if (one_of(state, { bulk_rename_state::cancelled, bulk_rename_state::in_progress })) {
        f64 progress = f64(success + fail) / f64(total);
        imgui::SameLineSpaced(1);
        imgui::Text("%3.0lf %%", progress * 100.0);
    }
    else if (state == bulk_rename_state::done) {
        if (fail > 0 || ((success + fail) < total)) {
            f64 progress = f64(success + fail) / f64(total);
            imgui::SameLineSpaced(1);
            imgui::Text("%3.0lf %%", progress * 100.0);
        } else {
            update_recent_files(s_renames, g_initiating_expl->cwd);
            g_on_rename_callback();
            cleanup_and_close_popup();
        }
    }

    {
        static std::string s_error_msg = {};

        bool all_renames_attempted = (success + fail) == total;

        if (!s_pattern_compile_res.success) {
            auto &compile_error = s_pattern_compile_res.error;
            compile_error.front() = (char)toupper(compile_error.front());
            s_error_msg = compile_error.data();
        }
        else if (state == bulk_rename_state::in_progress && fail > 0) {
            s_error_msg = make_str("%zu renames failed!", fail);
            // TODO: show failures
        }
        else if (state == bulk_rename_state::done && all_renames_attempted && fail > 0) {
            s_error_msg = make_str("%zu renames failed!", fail);
            // TODO: show failures
        }
        else if (state == bulk_rename_state::done && !all_renames_attempted) {
            s_error_msg = "Catastrophic failure, unable to attempt all renames!";
        }
        else if (state == bulk_rename_state::cancelled) {
            s_error_msg = "Operation cancelled.";
        }
        else if (!s_collisions.empty()) {
            s_error_msg = "Collisions detected, see below.";
        }
        else {
            s_error_msg = "";
        }

        if (!s_error_msg.empty()) {
            imgui::SameLineSpaced(2);
            imgui::TextColored(error_color(), s_error_msg.c_str());
        }
    }

    imgui::Spacing();

    if (global_state::settings().show_debug_info) {
        imgui::Text("s_transform_us  : %.1lf", s_transform_us);
        imgui::Text("s_collisions_us : %.1lf", s_collisions_us);
        imgui::Spacing();
    }

    if (!s_collisions.empty()) {
        enum collisions_table_col_id : s32
        {
            collisions_table_col_problem,
            collisions_table_col_after,
            collisions_table_col_before,
            collisions_table_col_count,
        };

        s32 table_flags =
            ImGuiTableFlags_SizingStretchProp|
            ImGuiTableFlags_BordersV|
            ImGuiTableFlags_BordersH|
            ImGuiTableFlags_Resizable|
            ImGuiTableFlags_Reorderable|
            ImGuiTableFlags_ScrollY
        ;

        if (imgui::BeginChild("bulk_rename_collisions_child")) {
            if (imgui::BeginTable("bulk_rename_collisions", collisions_table_col_count, table_flags)) {
                imgui::TableSetupColumn("Problem");
                imgui::TableSetupColumn("After");
                imgui::TableSetupColumn("Before");
                ImGui::TableSetupScrollFreeze(0, 1);
                imgui::TableHeadersRow();

                ImGuiListClipper clipper;
                assert(s_collisions.size() <= (u64)INT32_MAX);
                clipper.Begin((s32)s_collisions.size());

                while (clipper.Step())
                for (u64 i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                    auto const &c = s_collisions[i];
                    u64 first = c.first_rename_pair_idx;
                    u64 last = c.last_rename_pair_idx;

                    imgui::TableNextRow();

                    if (imgui::TableSetColumnIndex(collisions_table_col_problem)) {
                        imgui::AlignTextToFramePadding();
                        imgui::TextColored(error_color(), c.dest_dirent ? "Name exists in destination" : "Converge to same name");
                    }

                    if (imgui::TableSetColumnIndex(collisions_table_col_after)) {
                        imgui::AlignTextToFramePadding();
                        if (imgui::Selectable(c.dest_dirent ? c.dest_dirent->path.data() : s_sorted_renames[i].after.data())) {
                            {
                                std::scoped_lock lock(expl.select_cwd_entries_on_next_update_mutex);
                                expl.select_cwd_entries_on_next_update.clear();
                                expl.select_cwd_entries_on_next_update.push_back(c.dest_dirent->path);
                            }

                            expl.deselect_all_cwd_entries();
                            (void) expl.update_cwd_entries(full_refresh, expl.cwd.data());
                            expl.scroll_to_nth_selected_entry_next_frame = 0;

                            cleanup_and_close_popup();
                        }
                    }
                    if (c.dest_dirent && imgui::IsItemHovered()) {
                        imgui::SetTooltip("Click to open entry in destination");
                    }

                    imgui::ScopedStyle<f32> isx(imgui::GetStyle().ItemSpacing.x, 5);

                    if (imgui::TableSetColumnIndex(collisions_table_col_before)) {
                        for (u64 j = first; j <= last; ++j) {
                            f32 width = imgui::CalcTextSize(s_sorted_renames[j].before->path.data()).x + (imgui::GetStyle().FramePadding.x * 2);
                            auto more_msg = make_str_static<64>("... %zu more", last - j + 1);
                            f32 width_more = imgui::CalcTextSize(more_msg.data()).x;

                            if ((width + width_more) > imgui::GetContentRegionAvail().x) {
                                imgui::AlignTextToFramePadding();
                                imgui::TextUnformatted(more_msg.data());
                                break;
                            }

                            imgui::ScopedTextColor tc(get_color(s_sorted_renames[j].before->type));
                            imgui::ScopedItemWidth w(width);

                            auto label = make_str_static<64>("##before_%zu", j);
                            imgui::InputText(label.data(), s_sorted_renames[j].before->path.data(), s_sorted_renames[j].before->path.max_size(), ImGuiInputTextFlags_ReadOnly);
                            imgui::SameLine();

                            if (imgui::IsItemClicked(ImGuiMouseButton_Right)) {
                                ImGuiInputTextState *input_txt_state = imgui::GetInputTextState(imgui::GetID(label.data()));
                                if (input_txt_state->HasSelection()) {
                                    set_clipboard_to_slice(input_txt_state);
                                }
                            }
                        }
                    }
                }
                imgui::EndTable();
            }
        }
        imgui::EndChild();
    }
    else { // show preview
        if (imgui::BeginChild("bulk_rename_child")) {
            if (imgui::BeginTable("bulk_rename_preview", 3, ImGuiTableFlags_Resizable|ImGuiTableFlags_SizingStretchProp|ImGuiTableFlags_ScrollY)) {
                imgui::TableSetupColumn("Before");
                imgui::TableSetupColumn("After");
                imgui::TableSetupColumn("");
                ImGui::TableSetupScrollFreeze(0, 1);
                imgui::TableHeadersRow();

                ImGuiListClipper clipper;
                assert(s_renames.size() <= (u64)INT32_MAX);
                clipper.Begin((s32)s_renames.size());

                while (clipper.Step())
                for (u64 i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                    auto &rename = s_renames[i];
                    auto &before = *rename.before;
                    auto &after = rename.after;
                    auto color = get_color(before.type);

                    imgui::TableNextColumn();
                    {
                        imgui::ScopedAvailWidth w = {};
                        imgui::ScopedTextColor tc(color);

                        auto label = make_str_static<64>("##before_%zu", i);
                        imgui::InputText(label.data(), before.path.data(), before.path.max_size(), ImGuiInputTextFlags_ReadOnly);

                        if (imgui::IsItemClicked(ImGuiMouseButton_Right)) {
                            // TODO: fix flicker (popup closed for 1 frame) when right clicking
                            // TODO: fix crash when right clicking after double clicking the text input to select all
                            ImGuiInputTextState *input_txt_state = imgui::GetInputTextState(imgui::GetID(label.data()));
                            if (input_txt_state->HasSelection()) {
                                set_clipboard_to_slice(input_txt_state);
                            }
                        }
                    }

                    imgui::TableNextColumn();
                    {
                        imgui::ScopedAvailWidth w = {};
                        imgui::ScopedTextColor tc(color);

                        auto label = make_str_static<64>("##after_%zu", i);
                        imgui::InputText(label.data(), after.data(), after.max_size(), ImGuiInputTextFlags_ReadOnly);
                    }

                    imgui::TableNextColumn();
                    {
                        char result = rename.result.load();

                        if (result != 0) {
                            ImVec4 result_color;
                            char const *result_icon = nullptr;

                            if (result == 'Y') {
                                result_color = success_color();
                                result_icon = ICON_CI_CHECK;
                            } else {
                                result_color = error_color();
                                result_icon = ICON_CI_ERROR;
                            }

                            imgui::TextColored(result_color, result_icon);
                        }
                        else {
                            imgui::TextUnformatted("");
                        }
                    }
                }

                imgui::EndTable();
            }
        }
        imgui::EndChild();
    }

    if (rename_button_pressed && s_pattern_compile_res.success && s_collisions.empty() && s_rename_state.load() == bulk_rename_state::nil) {
        auto bulk_rename_task = [](std::vector<bulk_rename_op> &rename_ops, swan_path expl_cwd, wchar_t dir_sep_utf16) noexcept {
            s_rename_state.store(bulk_rename_state::in_progress);
            s_num_renames_total.store(rename_ops.size());

            try {
                std::wstring before_path_utf16 = {};
                std::wstring after_path_utf16 = {};

                for (u64 i = 0; i < rename_ops.size(); ++i) {
                    if (s_rename_state.load() == bulk_rename_state::cancelled) {
                        return;
                    }

                #if 0
                    if (chance(1/100.f)) {
                        ++s_num_renames_fail;
                        continue;
                    }
                #endif
                #if 0
                    if (i == 1) {
                        throw "test";
                    }
                #endif

                    auto &rename = rename_ops[i];

                    wchar_t buffer_cwd_utf16[MAX_PATH];     init_empty_cstr(buffer_cwd_utf16);
                    wchar_t buffer_before_utf16[MAX_PATH];  init_empty_cstr(buffer_before_utf16);
                    wchar_t buffer_after_utf16[MAX_PATH];   init_empty_cstr(buffer_after_utf16);

                    if (!utf8_to_utf16(expl_cwd.data(), buffer_cwd_utf16, lengthof(buffer_cwd_utf16))) {
                        ++s_num_renames_fail;
                        continue;
                    }

                    assert(rename.before != nullptr);

                    if (!utf8_to_utf16(rename.before->path.data(), buffer_before_utf16, lengthof(buffer_before_utf16))) {
                        ++s_num_renames_fail;
                        continue;
                    }

                    before_path_utf16 = buffer_cwd_utf16;
                    if (!before_path_utf16.ends_with(dir_sep_utf16)) {
                        before_path_utf16 += dir_sep_utf16;
                    }
                    before_path_utf16 += buffer_before_utf16;

                    if (!utf8_to_utf16(rename.after.data(), buffer_after_utf16, lengthof(buffer_after_utf16))) {
                        ++s_num_renames_fail;
                        continue;
                    }

                    after_path_utf16 = buffer_cwd_utf16;
                    if (!after_path_utf16.ends_with(dir_sep_utf16)) {
                        after_path_utf16 += dir_sep_utf16;
                    }
                    after_path_utf16 += buffer_after_utf16;

                    WCOUT_IF_DEBUG("[" << before_path_utf16.c_str() << "] -> [" << after_path_utf16.c_str() << "]\n");
                    s32 result = _wrename(before_path_utf16.c_str(), after_path_utf16.c_str());

                    if (result == 0) {
                        rename.result.store('Y');
                        ++s_num_renames_success;
                        // TODO: select renamed entry in expl
                    }
                    else {
                        rename.result.store('N');
                    }
                }
            }
            catch (...) {}

            s_rename_state.store(bulk_rename_state::done);
        };

        // TODO: change assert into proper error handling
        assert(num_transform_errors == 0);

        global_state::thread_pool().push_task(bulk_rename_task, std::ref(s_renames), expl.cwd, dir_sep_utf16);
    }

    if (imgui::IsWindowFocused() && imgui::IsKeyPressed(ImGuiKey_Escape) && s_rename_state.load() != bulk_rename_state::in_progress) {
        if (state == bulk_rename_state::done) {
            update_recent_files(s_renames, g_initiating_expl->cwd);
            g_on_rename_callback();
        }
        cleanup_and_close_popup();
    }

    imgui::EndPopup();
}
