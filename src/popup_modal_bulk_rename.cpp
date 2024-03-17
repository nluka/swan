#include "stdafx.hpp"
#include "common_functions.hpp"
#include "imgui_dependent_functions.hpp"
#include "scoped_timer.hpp"

namespace bulk_rename_modal_global_state
{
    static bool                                 g_open = false;
    static explorer_window *                    g_expl_opened_from = nullptr;
    static std::vector<explorer_window::dirent> g_expl_dirents_selected = {};
    static std::function<void ()>               g_on_rename_callback = {};
}

void swan_popup_modals::open_bulk_rename(explorer_window &expl_opened_from, std::function<void ()> on_rename_callback) noexcept
{
    using namespace bulk_rename_modal_global_state;

    g_open = true;
    bit_set(global_state::popup_modals_open_bit_field(), swan_popup_modals::bit_pos_bulk_rename);

    g_expl_dirents_selected.clear();
    for (auto const &dirent : expl_opened_from.cwd_entries) {
        if (dirent.is_selected) {
            g_expl_dirents_selected.push_back(dirent);
        }
    }

    assert(g_expl_opened_from == nullptr);
    g_expl_opened_from = &expl_opened_from;

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

void swan_popup_modals::render_bulk_rename() noexcept
{
    using namespace bulk_rename_modal_global_state;

    {
        f32 default_w = 800;
        f32 default_h = 600;

        imgui::SetNextWindowPos(
            {
                (f32(global_state::settings().window_w) / 2.f) - (default_w / 2.f),
                (f32(global_state::settings().window_h) / 2.f) - (default_h / 2.f),
            },
            ImGuiCond_Appearing
        );

        imgui::SetNextWindowSize({ default_w, default_h }, ImGuiCond_Appearing);
    }

    if (g_open) {
        imgui::OpenPopup(swan_popup_modals::label_bulk_rename);
    }
    if (!imgui::BeginPopupModal(swan_popup_modals::label_bulk_rename, nullptr)) {
        return;
    }

    assert(g_expl_opened_from != nullptr);
    assert(!g_expl_dirents_selected.empty());

    auto &expl = *g_expl_opened_from;
    auto &selection = g_expl_dirents_selected;

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

        g_expl_opened_from = nullptr;
        g_expl_dirents_selected.clear();
        g_on_rename_callback = {};

        imgui::CloseCurrentPopup();
    };

    bool recompute = false;

    recompute |= imgui::InputTextWithHint(
        " Pattern##bulk_rename_pattern", "Rename pattern...", s_pattern_utf8, lengthof(s_pattern_utf8),
        ImGuiInputTextFlags_CallbackCharFilter, filter_chars_callback, (void *)L"\\/\"|?*"
        // don't filter <>, we use them for interpolating the pattern with name, counter, etc.
    );

    imgui::SameLine();

    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort) && ImGui::BeginTooltip()) {
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

    recompute |= imgui::InputInt(" Counter start ", &s_counter_start);

    recompute |= imgui::InputInt(" Counter step ", &s_counter_step);

    recompute |= imgui::Checkbox("Squish adjacent spaces", &s_squish_adjacent_spaces);

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
                        s_renames.emplace_back(&dirent.basic, after);
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
    bool rename_button_pressed = imgui::Button("Rename##bulk_perform");
    imgui::EndDisabled();

    imgui::SameLine();

    if (state == bulk_rename_state::in_progress) {
        if (imgui::Button("Abort##bulk_perform")) {
            s_rename_state.store(bulk_rename_state::cancelled);
        }
    }
    else {
        if (imgui::Button("Exit##bulk_rename")) {
            if (state == bulk_rename_state::done) {
                g_on_rename_callback();
            }
            cleanup_and_close_popup();
        }
    }

    state = s_rename_state.load();

    switch (state) {
        default:
        case bulk_rename_state::nil: {
            break;
        }
        case bulk_rename_state::cancelled:
        case bulk_rename_state::in_progress: {
            f32 progress = f32(success + fail) / f32(total);
            imgui::SameLine();
            imgui::ProgressBar(progress);
            break;
        }
        case bulk_rename_state::done: {
            if (fail > 0 || ((success + fail) < total)) {
                f32 progress = f32(success + fail) / f32(total);
                imgui::SameLine();
                imgui::ProgressBar(progress);
            } else {
                g_on_rename_callback();
                cleanup_and_close_popup();
            }
            break;
        }
    }

    imgui::Spacing();

    if (global_state::settings().show_debug_info) {
        imgui::Text("s_transform_us  : %.1lf", s_transform_us);
        imgui::Text("s_collisions_us : %.1lf", s_collisions_us);
        imgui::Spacing();
    }

    bool all_renames_attempted = (success + fail) == total;

    if (state == bulk_rename_state::in_progress && fail > 0) {
        imgui::TextColored(red(), "%zu renames failed!", fail);
        // TODO: show failures
    }
    else if (state == bulk_rename_state::done && all_renames_attempted && fail > 0) {
        imgui::TextColored(red(), "%zu renames failed!", fail);
        // TODO: show failures
    }
    else if (state == bulk_rename_state::done && !all_renames_attempted) {
        imgui::TextColored(red(), "Catastrophic failure, unable to attempt all renames!");
    }
    else if (state == bulk_rename_state::cancelled) {
        imgui::TextColored(red(), "Operation cancelled.");
    }
    else if (!s_collisions.empty()) {
        imgui::TextColored(red(), "Collisions detected, see below.");

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
                        imgui::TextColored(red(), c.dest_dirent ? "Name exists in destination" : "Converge to same name");
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
    else if (!s_pattern_compile_res.success) {
        auto &compile_error = s_pattern_compile_res.error;
        compile_error.front() = (char)toupper(compile_error.front());

        imgui::PushTextWrapPos(imgui::GetColumnWidth());
        SCOPE_EXIT { imgui::PopTextWrapPos(); };

        imgui::TextColored(red(), compile_error.data());
    }
    else { // show preview
        if (imgui::BeginChild("bulk_rename_child")) {
            if (imgui::BeginTable("bulk_rename_preview", 2, ImGuiTableFlags_Resizable|ImGuiTableFlags_SizingStretchProp|ImGuiTableFlags_ScrollY)) {
                imgui::TableSetupColumn("Before");
                imgui::TableSetupColumn("After");
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
                }

                imgui::EndTable();
            }
        }
        imgui::EndChild();
    }

    if (rename_button_pressed && s_pattern_compile_res.success && s_collisions.empty() && s_rename_state.load() == bulk_rename_state::nil) {
        auto bulk_rename_task = [](std::vector<bulk_rename_op> rename_ops,  swan_path expl_cwd, wchar_t dir_sep_utf16) noexcept {
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

                    auto const &rename = rename_ops[i];

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
                        ++s_num_renames_success;
                        // TODO: select renamed entry in expl
                    }
                }
            }
            catch (...) {}

            s_rename_state.store(bulk_rename_state::done);
        };

        if (s_collisions.empty()) {
            // TODO: change assert into proper error handling
            assert(num_transform_errors == 0);

            global_state::thread_pool().push_task(bulk_rename_task, s_renames, expl.cwd, dir_sep_utf16);
        }
    }

    if (imgui::IsWindowFocused() && imgui::IsKeyPressed(ImGuiKey_Escape) && s_rename_state.load() != bulk_rename_state::in_progress) {
        if (state == bulk_rename_state::done) {
            g_on_rename_callback();
        }
        cleanup_and_close_popup();
    }

    imgui::EndPopup();
}
