#include "imgui/imgui.h"

#include "common.hpp"
#include "imgui_specific.hpp"
#include "scoped_timer.hpp"

namespace imgui = ImGui;

static bool s_bulk_rename_open = false;
static explorer_window *s_bulk_rename_expl = nullptr;
static std::vector<explorer_window::dirent *> const *s_bulk_rename_selection = nullptr;
static std::function<void ()> s_bulk_rename_on_rename_finish_callback = {};

char const *swan_id_bulk_rename_popup_modal() noexcept
{
    return "Bulk Rename";
}

void swan_open_popup_modal_bulk_rename(
    explorer_window &expl,
    std::vector<explorer_window::dirent *> const &selection,
    std::function<void ()> on_rename_finish_callback) noexcept
{
    s_bulk_rename_open = true;

    assert(s_bulk_rename_selection == nullptr);
    s_bulk_rename_selection = &selection;

    assert(s_bulk_rename_expl == nullptr);
    s_bulk_rename_expl = &expl;

    s_bulk_rename_on_rename_finish_callback = on_rename_finish_callback;
}

bool swan_is_popup_modal_open_bulk_rename() noexcept
{
    return s_bulk_rename_open;
}

void swan_render_popup_modal_bulk_rename() noexcept
{
    if (s_bulk_rename_open) {
        imgui::OpenPopup(swan_id_bulk_rename_popup_modal());
    }
    if (!imgui::BeginPopupModal(swan_id_bulk_rename_popup_modal(), nullptr)) {
        return;
    }

    assert(s_bulk_rename_expl != nullptr);
    assert(s_bulk_rename_selection != nullptr);

    auto &expl = *s_bulk_rename_expl;
    auto &selection = *s_bulk_rename_selection;

    wchar_t dir_sep_utf16 = get_explorer_options().dir_separator_utf16();

    static char pattern_utf8[512] = "<name><dotext>";
    static s32 counter_start = 1;
    static s32 counter_step = 1;
    static bool squish_adjacent_spaces = true;

    enum class bulk_rename_state : s32
    {
        nil,
        in_progress,
        done,
        cancelled,
    };

    static std::atomic<bulk_rename_state> rename_state = bulk_rename_state::nil;
    static std::atomic<u64> num_renames_success(0);
    static std::atomic<u64> num_renames_fail(0);
    static std::atomic<u64> num_renames_total = 0;

    static bool initial_computed = false;
    static bulk_rename_compile_pattern_result pattern_compile_res = {};
    static std::vector<bulk_rename_op> renames = {};
    static std::vector<bulk_rename_collision> collisions;
    static f64 transform_us = {};
    static f64 collisions_us = {};

    auto cleanup_and_close_popup = [&]() {
        rename_state.store(bulk_rename_state::nil);
        num_renames_success.store(0);
        num_renames_fail.store(0);
        num_renames_total.store(0);
        initial_computed = false;
        pattern_compile_res = {};
        renames.clear();
        collisions.clear();
        transform_us = {};
        collisions_us = {};

        s_bulk_rename_open = false;
        s_bulk_rename_expl = nullptr;
        s_bulk_rename_selection = nullptr;
        s_bulk_rename_on_rename_finish_callback = {};

        imgui::CloseCurrentPopup();
    };

    bool recompute = false;

    recompute |= imgui::InputTextWithHint(
        " Pattern##bulk_rename_pattern", "Rename pattern...", pattern_utf8, lengthof(pattern_utf8),
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

    imgui::Spacing();

    recompute |= imgui::InputInt(" Counter start ", &counter_start);

    imgui::Spacing();

    recompute |= imgui::InputInt(" Counter step ", &counter_step);

    imgui::Spacing();

    recompute |= imgui::Checkbox("Squish adjacent spaces", &squish_adjacent_spaces);

    imgui::Spacing();
    imgui::Separator();
    imgui::Spacing();

    u64 num_transform_errors = 0;

    if (!initial_computed || recompute) {
        debug_log("[%s] recomputing pattern, renames, collisions", expl.name);

        renames.reserve(selection.size());
        renames.clear();

        collisions.reserve(selection.size());
        collisions.clear();

        pattern_compile_res = bulk_rename_compile_pattern(pattern_utf8, squish_adjacent_spaces);

        if (pattern_compile_res.success) {
            {
                scoped_timer<timer_unit::MICROSECONDS> tranform_timer(&transform_us);

                s32 counter = counter_start;

                for (auto &p_dirent : selection) {
                    auto &dirent = *p_dirent;
                    file_name_ext name_ext(dirent.basic.path.data());
                    swan_path_t after;

                    auto transform = bulk_rename_transform(pattern_compile_res.compiled_pattern, after, name_ext.name,
                                                           name_ext.ext, counter, dirent.basic.size);

                    if (transform.success) {
                        renames.emplace_back(&dirent.basic, after);
                    } else {
                        ++num_transform_errors;
                    }

                    counter += counter_step;
                }
            }
            {
                scoped_timer<timer_unit::MICROSECONDS> find_collisions_timer(&collisions_us);
                collisions = bulk_rename_find_collisions(expl.cwd_entries, renames);
            }
        }

        initial_computed = true;
    }

    bulk_rename_state state = rename_state.load();
    u64 success = num_renames_success.load();
    u64 fail = num_renames_fail.load();
    u64 total = num_renames_total.load();

    imgui::BeginDisabled(!pattern_compile_res.success || !collisions.empty() || pattern_utf8[0] == '\0' || state != bulk_rename_state::nil);
    bool rename_button_pressed = imgui::Button("Rename##bulk_perform");
    imgui::EndDisabled();

    imgui::SameLine();

    if (state == bulk_rename_state::in_progress) {
        if (imgui::Button("Abort##bulk_perform")) {
            rename_state.store(bulk_rename_state::cancelled);
        }
    }
    else {
        if (imgui::Button("Exit##bulk_rename")) {
            if (state == bulk_rename_state::done) {
                s_bulk_rename_on_rename_finish_callback();
            }
            cleanup_and_close_popup();
        }
    }

    // imgui::BeginDisabled(state != bulk_rename_state::in_progress);
    // if (imgui::Button("Cancel##bulk_perform")) {
    //     rename_state.store(bulk_rename_state::cancelled);
    // }
    // imgui::EndDisabled();

    state = rename_state.load();

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
                s_bulk_rename_on_rename_finish_callback();
                cleanup_and_close_popup();
            }
            break;
        }
    }

    imgui::Spacing();
    imgui::Separator();
    imgui::Spacing();

    if (get_explorer_options().show_debug_info) {
        imgui::Text("transform_us: %.2lf", transform_us);
        imgui::Text("collisions_us: %.2lf", collisions_us);
        imgui_spacing(2);
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
    else if (!collisions.empty()) { // show collisions
        enum collisions_table_col_id : s32
        {
            collisions_table_col_problem,
            collisions_table_col_after,
            collisions_table_col_before,
            collisions_table_col_count,
        };

        if (imgui::BeginTable("bulk_rename_collisions", collisions_table_col_count, ImGuiTableFlags_SizingStretchProp|ImGuiTableFlags_BordersInnerV)) {
            imgui::TableSetupColumn("Problem");
            imgui::TableSetupColumn("After");
            imgui::TableSetupColumn("Before");
            imgui::TableHeadersRow();

            for (auto const &c : collisions) {
                u64 first = c.first_rename_pair_idx;
                u64 last = c.last_rename_pair_idx;

                imgui::TableNextRow();

                if (imgui::TableSetColumnIndex(collisions_table_col_problem)) {
                    imgui::TextColored(red(), c.dest_dirent ? "Name taken" : "Same result");
                }

                if (imgui::TableSetColumnIndex(collisions_table_col_after)) {
                    imgui::Text(renames.front().after.data());
                }

                if (imgui::TableSetColumnIndex(collisions_table_col_before)) {
                    for (u64 i = first; i < last; ++i) {
                        imgui::TextColored(get_color(renames[i].before->type), "%s\n", renames[i].before->path.data());
                    }
                    imgui::TextColored(get_color(renames[last].before->type), "%s", renames[last].before->path.data());
                }
            }
            imgui::EndTable();
        }
    }
    else if (!pattern_compile_res.success) {
        auto &error = pattern_compile_res.error;
        error.front() = (char)toupper(error.front());

        imgui::PushTextWrapPos(imgui::GetColumnWidth());
        imgui::TextColored(red(), error.data());
        imgui::PopTextWrapPos();
    }
    else { // show preview
        if (imgui::BeginChild("bulk_rename_child")) {
            if (imgui::BeginTable("bulk_rename_preview", 2, ImGuiTableFlags_Resizable|ImGuiTableFlags_SizingStretchProp)) {
                imgui::TableSetupColumn("Before");
                imgui::TableSetupColumn("After");
                imgui::TableHeadersRow();

                ImGuiListClipper clipper;
                assert(renames.size() <= (u64)INT32_MAX);
                clipper.Begin((s32)selection.size());

                while (clipper.Step())
                for (u64 i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                    auto const &rename = renames[i];
                    auto &before = *rename.before;
                    auto &after = rename.after;
                    auto color = get_color(before.type);

                    imgui::TableNextColumn();
                    imgui::TextColored(color, before.path.data());

                    imgui::TableNextColumn();
                    imgui::TextColored(color, after.data());
                }

                imgui::EndTable();
            }
        }
        imgui::EndChild();
    }

    if (rename_button_pressed && pattern_compile_res.success && collisions.empty() && rename_state.load() == bulk_rename_state::nil) {
        auto bulk_rename_task = [](std::vector<bulk_rename_op> rename_ops, swan_path_t expl_cwd, wchar_t dir_sep_utf16) {
            rename_state.store(bulk_rename_state::in_progress);
            num_renames_total.store(rename_ops.size());

            try {
                std::wstring before_path_utf16 = {};
                std::wstring after_path_utf16 = {};

                for (u64 i = 0; i < rename_ops.size(); ++i) {
                    if (rename_state.load() == bulk_rename_state::cancelled) {
                        return;
                    }

                #if 0
                    if (chance(1/100.f)) {
                        ++num_renames_fail;
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
                    s32 utf_written = 0;

                    utf_written = utf8_to_utf16(expl_cwd.data(), buffer_cwd_utf16, lengthof(buffer_cwd_utf16));

                    if (utf_written == 0) {
                        debug_log("utf8_to_utf16 failed (expl_cwd -> buffer_cwd_utf16)");
                        ++num_renames_fail;
                        continue;
                    }

                    assert(rename.before != nullptr);
                    utf_written = utf8_to_utf16(rename.before->path.data(), buffer_before_utf16, lengthof(buffer_before_utf16));

                    if (utf_written == 0) {
                        debug_log("utf8_to_utf16 failed (rename.before.path -> buffer_before_utf16)");
                        ++num_renames_fail;
                        continue;
                    }

                    before_path_utf16 = buffer_cwd_utf16;
                    if (!before_path_utf16.ends_with(dir_sep_utf16)) {
                        before_path_utf16 += dir_sep_utf16;
                    }
                    before_path_utf16 += buffer_before_utf16;

                    utf_written = utf8_to_utf16(rename.after.data(), buffer_after_utf16, lengthof(buffer_after_utf16));

                    if (utf_written == 0) {
                        debug_log("utf8_to_utf16 failed (rename.after -> buffer_after_utf16)");
                        ++num_renames_fail;
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
                        ++num_renames_success;
                    }
                }
            }
            catch (...) {}

            rename_state.store(bulk_rename_state::done);
        };

        if (collisions.empty()) {
            // TODO: change assert into proper error handling
            assert(num_transform_errors == 0);

            get_thread_pool().push_task(bulk_rename_task, renames, expl.cwd, dir_sep_utf16);
        }
    }

    if (imgui::IsWindowFocused() && imgui::IsKeyPressed(ImGuiKey_Escape) && rename_state.load() != bulk_rename_state::in_progress) {
        if (state == bulk_rename_state::done) {
            s_bulk_rename_on_rename_finish_callback();
        }
        cleanup_and_close_popup();
    }

    imgui::EndPopup();
}
