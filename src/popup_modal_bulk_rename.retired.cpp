#include "stdafx.hpp"
#include "common_functions.hpp"
#include "data_types.hpp"
#include "imgui_dependent_functions.hpp"
#include "scoped_timer.hpp"

struct bulk_rename_compiled_pattern
{
    struct op
    {
        enum class type : u8
        {
            insert_char,
            insert_name,
            insert_ext,
            insert_dotext,
            insert_size,
            insert_counter,
            insert_slice,
        };

        type kind;
        char ch = 0;
        bool explicit_first = false;
        bool explicit_last = false;
        u16 slice_first = 0;
        u16 slice_last = UINT16_MAX;

        bool operator!=(op const &other) const noexcept; // for ntest
        friend std::ostream& operator<<(std::ostream &os, op const &r); // for ntest
    };

    std::vector<op> ops;
    bool squish_adjacent_spaces;
};

struct bulk_rename_compile_pattern_result
{
    bool success;
    bulk_rename_compiled_pattern compiled_pattern;
    std::array<char, 256> error;
};

struct bulk_rename_transform_result
{
    bool success;
    std::array<char, 256> error;
};

struct bulk_rename_op
{
    basic_dirent *before;
    swan_path after;
    std::atomic_char result;

    bulk_rename_op(basic_dirent *before, char const *after) noexcept;

    bulk_rename_op(bulk_rename_op const &other) noexcept; // for emplace_back
    bulk_rename_op &operator=(bulk_rename_op const &other) noexcept; // for emplace_back

    bool operator!=(bulk_rename_op const &other) const noexcept; // for ntest
    friend std::ostream& operator<<(std::ostream &os, bulk_rename_op const &r); // for ntest
};

struct bulk_rename_collision
{
    basic_dirent *dest_dirent;
    u64 first_rename_pair_idx;
    u64 last_rename_pair_idx;

    bool operator!=(bulk_rename_collision const &other) const noexcept; // for ntest
    friend std::ostream& operator<<(std::ostream &os, bulk_rename_collision const &c); // for ntest
};

struct bulk_rename_find_collisions_result
{
    std::vector<bulk_rename_collision> collisions;
    std::vector<bulk_rename_op> sorted_renames;
};

bulk_rename_compile_pattern_result bulk_rename_compile_pattern(char const *pattern, bool squish_adjacent_spaces) noexcept;

bulk_rename_transform_result bulk_rename_execute_transform(
    bulk_rename_compiled_pattern compiled_pattern,
    swan_path &after,
    char const *name,
    char const *ext,
    s32 counter,
    u64 bytes) noexcept;

void sort_renames_dup_elem_sequences_after_non_dups(std::vector<bulk_rename_op> &renames) noexcept;

// Slow function which allocates & deallocates memory. Cache the result, don't call this function every frame.
bulk_rename_find_collisions_result bulk_rename_find_collisions(
    std::vector<explorer_window::dirent> &dest,
    std::vector<bulk_rename_op> const &renames) noexcept;

bool bulk_rename_collision::operator!=(bulk_rename_collision const &other) const noexcept // for ntest
{
    return
        this->first_rename_pair_idx != other.first_rename_pair_idx ||
        this->last_rename_pair_idx != other.last_rename_pair_idx ||
        this->dest_dirent != other.dest_dirent;
}

bulk_rename_op &bulk_rename_op::operator=(bulk_rename_op const &other) noexcept // for emplace_back
{
    this->before = other.before;
    this->after = other.after;
    this->result = other.result.load();
    return *this;
}

bulk_rename_op::bulk_rename_op(const bulk_rename_op &other) noexcept // for emplace_back
    : before(other.before)
    , after(other.after)
    , result(other.result.load())
{
}

bulk_rename_op::bulk_rename_op(basic_dirent *before, char const *after) noexcept
    : before(before)
    , after(path_create(after))
    , result('\0')
{
}

bool bulk_rename_op::operator!=(bulk_rename_op const &other) const noexcept // for ntest
{
    return this->before != other.before || !path_equals_exactly(this->after, other.after);
}

std::ostream& operator<<(std::ostream &os, bulk_rename_op const &r) // for ntest
{
    return os << "B:[" << (r.before ? r.before->path.data() : "nullptr") << "] A:[" << r.after.data() << ']';
}

std::ostream& operator<<(std::ostream &os, bulk_rename_collision const &c) // for ntest
{
    return os << "D:[" << (c.dest_dirent ? c.dest_dirent->path.data() : "")
              << "] [" << c.first_rename_pair_idx << ',' << c.last_rename_pair_idx << ']';
}

bool bulk_rename_compiled_pattern::op::operator!=(bulk_rename_compiled_pattern::op const &other) const noexcept // for ntest
{
    return this->kind != other.kind || this->ch != other.ch;
}

std::ostream& operator<<(std::ostream &os, bulk_rename_compiled_pattern::op const &op) // for ntest
{
    using op_type = bulk_rename_compiled_pattern::op::type;

    char const *op_str = "";
    switch (op.kind) {
        case op_type::insert_char:    op_str = "insert_char";    break;
        case op_type::insert_name:    op_str = "insert_name";    break;
        case op_type::insert_ext:     op_str = "insert_ext";     break;
        case op_type::insert_size:    op_str = "insert_size";    break;
        case op_type::insert_counter: op_str = "insert_counter"; break;
        case op_type::insert_slice:   op_str = "insert_slice";   break;
        default:                      op_str = "unknown_op";     break;
    }

    os << op_str;
    if (op.kind == op_type::insert_char) {
        os << ' ';
        if (op.ch == '\0') os << "NUL";
        else               os << op.ch;
    }

    return os;
}

bulk_rename_find_collisions_result
bulk_rename_find_collisions(std::vector<explorer_window::dirent> &dest, std::vector<bulk_rename_op> const &renames_in) noexcept
{
    std::vector<bulk_rename_collision> collisions = {};

    if (renames_in.empty()) {
        return { collisions, {} };
    }

    auto renames = renames_in; // make a copy cuz we gotta sort this sucker
    sort_renames_dup_elem_sequences_after_non_dups(renames);

    collisions.reserve(dest.size());

    static std::vector<explorer_window::dirent *> unaffected_dirents = {};
    unaffected_dirents.clear();
    unaffected_dirents.reserve(dest.size());

    for (auto &dest_dirent : dest) {
        if (!dest_dirent.is_selected) {
            unaffected_dirents.push_back(&dest_dirent);
        }
    }

    u64 const npos = (u64)-1;

    auto find_conflict_in_dest = [&](bulk_rename_op const &rename) -> basic_dirent* {
        for (auto &dirent : unaffected_dirents) {
            if (path_equals_exactly(dirent->basic.path, rename.after)) {
                return &dirent->basic;
            }
        }
        return nullptr;
    };

    auto adj_begin = std::adjacent_find(renames.begin(), renames.end(), [](bulk_rename_op const &r0, bulk_rename_op const &r1) noexcept {
        return path_equals_exactly(r0.after, r1.after);
    });

    // handle unique "after"s
    {
        u64 i = 0;
        for (auto it = renames.begin(); it != adj_begin; ++it, ++i) {
            auto conflict = find_conflict_in_dest(*it);
            if (conflict) {
                u64 first = i, last = i;
                collisions.emplace_back(conflict, first, last);
            }
        }
    }
    // handle non-unique "after"s
    {
        ptrdiff_t start_index = std::distance(renames.begin(), adj_begin);

        if (adj_begin != renames.end() && path_equals_exactly(renames.front().after, renames.back().after)) {
            u64 first = (u64)start_index;
            u64 last = renames.size() - 1;
            auto conflict = find_conflict_in_dest(renames[first]);
            collisions.emplace_back(conflict, first, last);
        }
        else {
            u64 i = (u64)start_index + 1;
            u64 first = i - 1, last = npos;

            for (; i < renames.size(); ++i) {
                if (!path_equals_exactly(renames[i].after, renames[first].after)) {
                    last = i-1;
                    auto conflict = find_conflict_in_dest(renames[first]);
                    collisions.emplace_back(conflict, first, last);

                    first = i;
                    last = npos;
                }
            }
        }
    }

    return { collisions, renames };
}

bulk_rename_compile_pattern_result bulk_rename_compile_pattern(char const *pattern, bool squish_adjacent_spaces) noexcept
{
    assert(pattern != nullptr);

    bulk_rename_compile_pattern_result result = {};
    auto &error = result.error;
    auto &success = result.success;
    auto &compiled = result.compiled_pattern;

    compiled.squish_adjacent_spaces = squish_adjacent_spaces;

    if (pattern[0] == '\0') {
        success = false;
        compiled = {};
        snprintf(error.data(), error.size(), "empty pattern");
        return result;
    }

    u64 const npos = (u64)-1;
    u64 opening_chevron_pos = npos;
    u64 closing_chevron_pos = npos;

    for (u64 i = 0; pattern[i] != '\0'; ++i) {
        char ch = pattern[i];

        if (ch == '\0') {
            break;
        }

        if ((s32)ch <= 31 || (s32)ch == 127 || strchr("\\/\"|?*", ch)) {
            success = false;
            compiled = {};
            snprintf(error.data(), error.size(), "illegal filename character %d at position %zu", ch, i);
            return result;
        }

        bool inside_chevrons = opening_chevron_pos != npos;

        if (ch == '<') {
            if (inside_chevrons) {
                success = false;
                compiled = {};
                snprintf(error.data(), error.size(), "unexpected '<' at position %zu, unclosed '<' at position %zu", i, opening_chevron_pos);
                return result;
            } else {
                opening_chevron_pos = i;
            }
        }
        else if (ch == '>') {
            if (inside_chevrons) {
                closing_chevron_pos = i;
                u64 expr_len = closing_chevron_pos - opening_chevron_pos - 1;
                char const *expr = pattern + opening_chevron_pos + 1;

                if (expr_len == 0) {
                    success = false;
                    compiled = {};
                    snprintf(error.data(), error.size(), "empty expression starting at position %zu", opening_chevron_pos);
                    return result;
                }

                auto expr_equals = [&](char const *known_expr) noexcept {
                    return StrCmpNIA(expr, known_expr, (s32)expr_len) == 0;
                };
                auto expr_begins_with = [&](char const *text) noexcept {
                    return StrCmpNIA(expr, text, (s32)strlen(text)) == 0;
                };

                bool known_expression = true;
                bulk_rename_compiled_pattern::op op = {};

                if (expr_equals("name")) {
                    op.kind = bulk_rename_compiled_pattern::op::type::insert_name;
                }
                else if (expr_equals("dotext")) {
                    op.kind = bulk_rename_compiled_pattern::op::type::insert_dotext;
                }
                else if (expr_equals("ext")) {
                    op.kind = bulk_rename_compiled_pattern::op::type::insert_ext;
                }
                else if (expr_equals("counter")) {
                    op.kind = bulk_rename_compiled_pattern::op::type::insert_counter;
                }
                else if (expr_equals("bytes")) {
                    op.kind = bulk_rename_compiled_pattern::op::type::insert_size;
                }
                else {
                    static std::regex s_valid_slice_with_start_end ("^[0-9]{1,5}, *[0-9]{1,5}$");
                    static std::regex s_valid_slice_with_start     ("^[0-9]{1,5}, *$");
                    static std::regex s_valid_slice_with_end                 ("^, *[0-9]{1,5}$");

                    std::string_view expr_view(expr, expr_len);
                    static std::string expr_str(100, '\0');
                    expr_str.clear();
                    expr_str.append(expr_view);

                    std::istringstream iss(expr_str);

                    op.kind = bulk_rename_compiled_pattern::op::type::insert_slice;

                    if (std::regex_match(expr_str, s_valid_slice_with_start_end)) {
                        op.explicit_first = true;
                        op.explicit_last = true;
                    }
                    else if (std::regex_match(expr_str, s_valid_slice_with_start)) {
                        op.explicit_first = true;
                    }
                    else if (std::regex_match(expr_str, s_valid_slice_with_end)) {
                        op.explicit_last = true;
                    }
                    else {
                        known_expression = false;
                    }

                    if (known_expression) {
                        if (op.explicit_first) {
                            iss >> op.slice_first;
                        }
                        while (strchr(", ", iss.peek())) {
                            char dummy = 0;
                            iss.read(&dummy, 1);
                        }
                        if (op.explicit_last) {
                            iss >> op.slice_last;
                        }
                    }

                    if (op.slice_first > op.slice_last) {
                        success = false;
                        compiled = {};
                        snprintf(error.data(), error.size(), "slice expression starting at position %zu is malformed, first is greater than last", opening_chevron_pos);
                        return result;
                    }
                }

                if (!known_expression) {
                    success = false;
                    compiled = {};
                    snprintf(error.data(), error.size(), "unknown expression starting at position %zu", opening_chevron_pos);
                    return result;
                } else {
                    compiled.ops.push_back(op);
                }

                // reset
                opening_chevron_pos = npos;
                closing_chevron_pos = npos;
            } else {
                success = false;
                compiled = {};
                snprintf(error.data(), error.size(), "unexpected '>' at position %zu - no preceding '<' found", i);
                return result;
            }
        }
        else if (inside_chevrons) {
            // do nothing
        }
        else {
            // non-special character

            if (squish_adjacent_spaces && i > 0 && pattern[i-1] == ' ' && ch == ' ') {
                // don't insert multiple adjacent spaces
            } else {
                bulk_rename_compiled_pattern::op op = {};
                op.kind = bulk_rename_compiled_pattern::op::type::insert_char;
                op.ch = ch;
                compiled.ops.push_back(op);
            }
        }
    }

    if (opening_chevron_pos != npos && closing_chevron_pos == npos) {
        success = false;
        compiled = {};
        snprintf(error.data(), error.size(), "unclosed '<' at position %zu", opening_chevron_pos);
        return result;
    }

    success = true;
    return result;
}

bulk_rename_transform_result bulk_rename_transform(
    bulk_rename_compiled_pattern compiled_pattern,
    swan_path &after,
    char const *name,
    char const *ext,
    s32 counter,
    u64 bytes) noexcept
{
    bulk_rename_transform_result result = {};
    after = {};
    u64 after_insert_idx = 0;

    auto report_error = [&](char const *msg) -> bulk_rename_transform_result & {
        result.success = false;
        (void) strncpy(result.error.data(), msg, result.error.max_size());
        return result;
    };

    for (auto const &op : compiled_pattern.ops) {
        u64 space_left = after.max_size() - after_insert_idx;
        char *out = after.data() + after_insert_idx;

        switch (op.kind) {
            using op_type = bulk_rename_compiled_pattern::op::type;

            case op_type::insert_char: {
                if (after_insert_idx < after.max_size()) {
                    after[after_insert_idx++] = op.ch;
                } else {
                    return report_error("not enough space for pattern");
                }
                break;
            }
            case op_type::insert_name: {
                u64 name_len = strlen(name);
                if (name_len > space_left) {
                    return report_error("not enough space for pattern");
                }
                (void) strcat(out, name);
                after_insert_idx += name_len;
                break;
            }
            case op_type::insert_ext: {
                if (ext == nullptr) {
                    break;
                }
                u64 name_len = strlen(ext);
                if (name_len > space_left) {
                    return report_error("not enough space for pattern");
                }
                (void) strcat(out, ext);
                after_insert_idx += name_len;
                break;
            }
            case op_type::insert_dotext: {
                if (ext == nullptr) {
                    break;
                }
                if (after_insert_idx >= after.max_size()) {
                    return report_error("not enough space for pattern");
                }
                after[after_insert_idx++] = '.';
                u64 len = strlen(ext);
                if (len > space_left) {
                    return report_error("not enough space for pattern");
                }
                (void) strcat(out, ext);
                after_insert_idx += len;
                break;
            }
            case op_type::insert_size: {
                auto buffer = make_str_static<21>("%zu", bytes);
                u64 buffer_len = strlen(buffer.data());
                if (buffer_len > space_left) {
                    return report_error("not enough space for pattern");
                }
                (void) strcat(out, buffer.data());
                after_insert_idx += buffer_len;
                break;
            }
            case op_type::insert_counter: {
                auto buffer = make_str_static<11>("%d", counter);
                u64 buffer_len = strlen(buffer.data());
                if (buffer_len > space_left) {
                    return report_error("not enough space for pattern");
                }
                (void) strcat(out, buffer.data());
                after_insert_idx += buffer_len;
                break;
            }
            case op_type::insert_slice: {
                swan_path full = path_create(name);

                if (ext != nullptr) {
                    [[maybe_unused]] auto success1 = path_append(full, ".");
                    assert(success1);

                    [[maybe_unused]] auto success2 = path_append(full, ext);
                    assert(success2);
                }

                u64 full_len = path_length(full);

                u64 actual_slice_last = op.slice_last;

                if (!op.explicit_first) {
                    // don't do anything because op.slice_first is already 0 as it should be in this case
                }
                if (!op.explicit_last) {
                    actual_slice_last = full_len - 1;
                }

                u64 len = actual_slice_last - op.slice_first + 1;
                assert(len > 0);

                if (op.slice_first >= full_len || actual_slice_last >= full_len) {
                    return report_error("slice goes out of bounds");
                }
                if (len > space_left) {
                    return report_error("not enough space for pattern");
                }
                (void) strncat(out, full.data() + op.slice_first, len);
                after_insert_idx += len;
                break;
            }
        }
    }

    u64 spaces_removed = 0;
    if (compiled_pattern.squish_adjacent_spaces) {
        spaces_removed = remove_adjacent_spaces(after.data(), path_length(after));
    }

    result.success = true;
    return result;
}

void sort_renames_dup_elem_sequences_after_non_dups(std::vector<bulk_rename_op> &renames) noexcept
{
    // sort renames such that adjacently equal renames appear at the end.
    // examples:
    //  [1,2,5,7,2,4] -> [2,2,6,5,4,1]
    //  [0,0,1,5,5,2] -> [5,5,0,0,2,1]
    // (I couldn't figure out how to do it in ascending order... descending will do.)

    std::stable_sort(renames.begin(), renames.end(), [&](bulk_rename_op const &a, bulk_rename_op const &b) noexcept {
        s32 cmp = strcmp(a.after.data(), b.after.data());
        if (cmp == 0) {
            return false;
        } else {
            return cmp > 0;
        }
    });
}

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

    if (g_open) {
        imgui::OpenPopup(swan_popup_modals::label_bulk_rename);
        center_window_and_set_size_when_appearing(800, 600);
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
        g_initiating_expl = nullptr;
        g_initiating_expl_selected_dirents.clear();
        g_on_rename_callback = {};

        imgui::CloseCurrentPopup();
    };

    bool recompute = false;

    imgui::AlignTextToFramePadding();
    imgui::TextUnformatted("Pattern");
    imgui::SameLine();
    auto help = render_help_indicator(true);
    if (help.hovered && ImGui::BeginTooltip()) {
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

                    auto transform = bulk_rename_execute_transform(s_pattern_compile_res.compiled_pattern, after, name_ext.name,
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

    // bulk_rename_compile_pattern, bulk_rename_execute_transform;
    #if 0
    {
        // bool squish_adjacent_spaces = false;
        swan_path after = {};

        auto assert_failed_compile = [](char const *pattern, char const *expected_error, std::source_location sloc = std::source_location::current()) noexcept {
            auto const [success, compiled, err_msg] = bulk_rename_compile_pattern(pattern, false);
            ntest::assert_bool(false, success, sloc);
            ntest::assert_cstr(expected_error, err_msg.data(), ntest::default_str_opts(), sloc);
            ntest::assert_stdvec({}, compiled.ops, sloc);
        };

        assert_failed_compile("", "empty pattern");
        assert_failed_compile("asdf\t", "illegal filename character 9 at position 4");
        assert_failed_compile("data<<", "unexpected '<' at position 5, unclosed '<' at position 4");
        assert_failed_compile("<", "unclosed '<' at position 0");
        assert_failed_compile(">", "unexpected '>' at position 0 - no preceding '<' found");
        assert_failed_compile("<>", "empty expression starting at position 0");
        assert_failed_compile("data_<bogus>", "unknown expression starting at position 5");
        assert_failed_compile("<,>", "unknown expression starting at position 0");
        assert_failed_compile("<0>", "unknown expression starting at position 0");
        assert_failed_compile("<  >", "unknown expression starting at position 0");
        assert_failed_compile("<123456,>", "unknown expression starting at position 0");
        assert_failed_compile("<1,0>", "slice expression starting at position 0 is malformed, first is greater than last");

        struct compile_and_transform_test_args
        {
            char const *name;
            char const *ext;
            char const *pattern;
            u64 size;
            s32 counter;
            bool squish_adjacent_spaces;
            char const *expected_after;
        };

        auto assert_successful_compile_and_transform = [](compile_and_transform_test_args args, std::source_location sloc = std::source_location::current()) noexcept {
            auto const [success_compile, compiled, err_msg_compile] = bulk_rename_compile_pattern(args.pattern, args.squish_adjacent_spaces);
            ntest::assert_bool(true, success_compile, sloc);
            ntest::assert_cstr("", err_msg_compile.data(), ntest::default_str_opts(), sloc);
            ntest::assert_bool(args.squish_adjacent_spaces, compiled.squish_adjacent_spaces, sloc);

            swan_path after = {};

            auto [success_transform, err_msg_transform] = bulk_rename_execute_transform(compiled, after, args.name, args.ext, args.counter, args.size);
            ntest::assert_bool(true, success_transform, sloc);
            ntest::assert_cstr(args.expected_after, after.data(), ntest::default_str_opts(), sloc);
            ntest::assert_cstr("", err_msg_transform.data(), ntest::default_str_opts(), sloc);
        };

        assert_successful_compile_and_transform({ .name="before", .ext="txt",
                                                  .pattern="<counter>", .size={}, .counter=0, .squish_adjacent_spaces={},
                                                  .expected_after="0" });

        assert_successful_compile_and_transform({ .name="before", .ext="txt",
                                                  .pattern="<counter>", .size={}, .counter=100, .squish_adjacent_spaces={},
                                                  .expected_after="100" });

        assert_successful_compile_and_transform({ .name="BEFORE", .ext="txt",
                                                  .pattern="something_<name>_bla", .size={}, .counter={}, .squish_adjacent_spaces={},
                                                  .expected_after="something_BEFORE_bla" });

        assert_successful_compile_and_transform({ .name="BEFORE", .ext="TXT",
                                                  .pattern="<name>  <bytes>.<ext>", .size=42, .counter={}, .squish_adjacent_spaces=false,
                                                  .expected_after="BEFORE  42.TXT" });

        assert_successful_compile_and_transform({ .name="BEFORE", .ext="TXT",
                                                  .pattern="<name>  <bytes>.<ext>", .size=42, .counter={}, .squish_adjacent_spaces=true,
                                                  .expected_after="BEFORE 42.TXT" });

        assert_successful_compile_and_transform({ .name="before", .ext="txt",
                                                  .pattern="29.  Gladiator Boss", .size={}, .counter={}, .squish_adjacent_spaces=false,
                                                  .expected_after="29.  Gladiator Boss" });

        assert_successful_compile_and_transform({ .name="before", .ext="txt",
                                                  .pattern="29.  Gladiator Boss", .size={}, .counter={}, .squish_adjacent_spaces=true,
                                                  .expected_after="29. Gladiator Boss" });

        assert_successful_compile_and_transform({ .name="before", .ext="txt",
                                                  .pattern="<0,>", .size={}, .counter={}, .squish_adjacent_spaces={},
                                                  .expected_after="before.txt" });

        assert_successful_compile_and_transform({ .name="before", .ext="txt",
                                                  .pattern="<,3>", .size={}, .counter={}, .squish_adjacent_spaces={},
                                                  .expected_after="befo" });

        assert_successful_compile_and_transform({ .name="before", .ext="txt",
                                                  .pattern="<0,0>", .size={}, .counter={}, .squish_adjacent_spaces={},
                                                  .expected_after="b" });

        assert_successful_compile_and_transform({ .name="before", .ext="txt",
                                                  .pattern="<0,5>", .size={}, .counter={}, .squish_adjacent_spaces={},
                                                  .expected_after="before" });

        assert_successful_compile_and_transform({ .name="before", .ext="txt",
                                                  .pattern="<0,6>", .size={}, .counter={}, .squish_adjacent_spaces={},
                                                  .expected_after="before." });

        assert_successful_compile_and_transform({ .name="before", .ext="txt",
                                                  .pattern="<0,7>", .size={}, .counter={}, .squish_adjacent_spaces={},
                                                  .expected_after="before.t" });

        assert_successful_compile_and_transform({ .name="before", .ext="txt",
                                                  .pattern="<0,9>", .size={}, .counter={}, .squish_adjacent_spaces={},
                                                  .expected_after="before.txt" });

        assert_successful_compile_and_transform({ .name="before", .ext="txt",
                                                  .pattern="<9,9>", .size={}, .counter={}, .squish_adjacent_spaces={},
                                                  .expected_after="t" });

        assert_successful_compile_and_transform({ .name="Some Long File Name", .ext="txt",
                                                  .pattern="<5,8>  <15,18>_<counter>_<bytes>.<ext>", .size=42, .counter=21, .squish_adjacent_spaces=true,
                                                  .expected_after="Long Name_21_42.txt" });

        assert_successful_compile_and_transform({ .name="01-01-2023 Report Name", .ext="docx",
                                                  .pattern="<0,10> asdf", .size={}, .counter={}, .squish_adjacent_spaces=true,
                                                  .expected_after="01-01-2023 asdf" });

        auto assert_successful_compile_but_failed_transform = [](compile_and_transform_test_args args, std::source_location sloc = std::source_location::current()) noexcept {
            // using args.expected_after for expected error

            auto const [success_compile, compiled, err_msg_compile] = bulk_rename_compile_pattern(args.pattern, args.squish_adjacent_spaces);
            ntest::assert_bool(true, success_compile, sloc);
            ntest::assert_cstr("", err_msg_compile.data(), ntest::default_str_opts(), sloc);
            ntest::assert_bool(args.squish_adjacent_spaces, compiled.squish_adjacent_spaces, sloc);

            swan_path after = {};

            auto [success_transform, err_msg_transform] = bulk_rename_execute_transform(compiled, after, args.name, args.ext, args.counter, args.size);
            ntest::assert_bool(false, success_transform, sloc);
            ntest::assert_cstr(args.expected_after, err_msg_transform.data(), ntest::default_str_opts(), sloc);
        };

        assert_successful_compile_but_failed_transform({ .name="before", .ext="txt",
                                                         .pattern="<0,10>", .size={}, .counter={}, .squish_adjacent_spaces={},
                                                         .expected_after="slice goes out of bounds" });

        assert_successful_compile_but_failed_transform({ .name="before", .ext="txt",
                                                         .pattern="<10,10>", .size={}, .counter={}, .squish_adjacent_spaces={},
                                                         .expected_after="slice goes out of bounds" });

        assert_successful_compile_but_failed_transform({ .name="before", .ext="txt",
                                                         .pattern="<5,10>", .size={}, .counter={}, .squish_adjacent_spaces={},
                                                         .expected_after="slice goes out of bounds" });

        assert_successful_compile_but_failed_transform({ .name="before", .ext="txt",
                                                         .pattern="<10,20>", .size={}, .counter={}, .squish_adjacent_spaces={},
                                                         .expected_after="slice goes out of bounds" });
    }
    #endif

    // bulk_rename_sort_renames_dup_elem_sequences_after_non_dups;
    #if 0
    {
        {
            std::vector<bulk_rename_op> input_renames = {};
            sort_renames_dup_elem_sequences_after_non_dups(input_renames);
            ntest::assert_stdvec({}, input_renames);
        }

        {
            std::vector<bulk_rename_op> input_renames = {
                bulk_rename_op( nullptr, "file1" ),
            };
            sort_renames_dup_elem_sequences_after_non_dups(input_renames);
            ntest::assert_stdvec({
                bulk_rename_op( nullptr, "file1" ),
            }, input_renames);
        }

        {
            std::vector<bulk_rename_op> input_renames = {
                bulk_rename_op( nullptr, "file2" ),
                bulk_rename_op( nullptr, "file2" ),
            };
            sort_renames_dup_elem_sequences_after_non_dups(input_renames);
            ntest::assert_stdvec({
                bulk_rename_op( nullptr, "file2" ),
                bulk_rename_op( nullptr, "file2" ),
            }, input_renames);
        }

        {
            std::vector<bulk_rename_op> input_renames = {
                bulk_rename_op( nullptr, "file3" ),
                bulk_rename_op( nullptr, "file3" ),
                bulk_rename_op( nullptr, "file3" ),
            };
            sort_renames_dup_elem_sequences_after_non_dups(input_renames);
            ntest::assert_stdvec({
                bulk_rename_op( nullptr, "file3" ),
                bulk_rename_op( nullptr, "file3" ),
                bulk_rename_op( nullptr, "file3" ),
            }, input_renames);
        }

        {
            std::vector<bulk_rename_op> input_renames = {
                bulk_rename_op( nullptr, "file1" ),
                bulk_rename_op( nullptr, "file2" ),
            };
            sort_renames_dup_elem_sequences_after_non_dups(input_renames);
            ntest::assert_stdvec({
                bulk_rename_op( nullptr, "file2" ),
                bulk_rename_op( nullptr, "file1" ),
            }, input_renames);
        }

        {
            std::vector<bulk_rename_op> input_renames = {
                bulk_rename_op( nullptr, "file1" ),
                bulk_rename_op( nullptr, "file2" ),
                bulk_rename_op( nullptr, "file3" ),
            };
            sort_renames_dup_elem_sequences_after_non_dups(input_renames);
            ntest::assert_stdvec({
                bulk_rename_op( nullptr, "file3" ),
                bulk_rename_op( nullptr, "file2" ),
                bulk_rename_op( nullptr, "file1" ),
            }, input_renames);
        }

        {
            std::vector<bulk_rename_op> input_renames = {
                bulk_rename_op( nullptr, "file4" ),
                bulk_rename_op( nullptr, "file5" ),
                bulk_rename_op( nullptr, "file6" ),
            };
            sort_renames_dup_elem_sequences_after_non_dups(input_renames);
            ntest::assert_stdvec({
                bulk_rename_op( nullptr, "file6" ),
                bulk_rename_op( nullptr, "file5" ),
                bulk_rename_op( nullptr, "file4" ),
            }, input_renames);
        }

        {
            std::vector<bulk_rename_op> input_renames = {
                bulk_rename_op( nullptr, "file1" ),
                bulk_rename_op( nullptr, "file2" ),
                bulk_rename_op( nullptr, "file3" ),
                bulk_rename_op( nullptr, "file4" ),
            };
            sort_renames_dup_elem_sequences_after_non_dups(input_renames);
            ntest::assert_stdvec({
                bulk_rename_op( nullptr, "file4" ),
                bulk_rename_op( nullptr, "file3" ),
                bulk_rename_op( nullptr, "file2" ),
                bulk_rename_op( nullptr, "file1" ),
            }, input_renames);
        }

        {
            std::vector<bulk_rename_op> input_renames = {
                bulk_rename_op( nullptr, "apple" ),
                bulk_rename_op( nullptr, "banana" ),
                bulk_rename_op( nullptr, "cherry" ),
                bulk_rename_op( nullptr, "apple" ),
                bulk_rename_op( nullptr, "banana" ),
                bulk_rename_op( nullptr, "date" ),
            };
            sort_renames_dup_elem_sequences_after_non_dups(input_renames);
            ntest::assert_stdvec({
                bulk_rename_op( nullptr, "date" ),
                bulk_rename_op( nullptr, "cherry" ),
                bulk_rename_op( nullptr, "banana" ),
                bulk_rename_op( nullptr, "banana" ),
                bulk_rename_op( nullptr, "apple" ),
                bulk_rename_op( nullptr, "apple" ),
            }, input_renames);
        }
    }
    #endif

    // bulk_rename_find_collisions;
    #if 0
    {
        using ent_kind = basic_dirent::kind;

        auto create_basic_dirent = [](char const *path, ent_kind type) -> basic_dirent {
            basic_dirent ent = {};
            ent.type = type;
            ent.path = path_create(path);
            return ent;
        };

        {
            std::vector<explorer_window::dirent> dest = {};
            std::vector<bulk_rename_op> input_renames = {};
            std::vector<bulk_rename_collision> expected_collisions = {};

            auto actual = bulk_rename_find_collisions(dest, input_renames);

            ntest::assert_stdvec(expected_collisions, actual.collisions);
        }

        {
            std::vector<explorer_window::dirent> dest = {
                { .basic = create_basic_dirent("file1.cpp", ent_kind::file), .is_filtered_out = 0, .is_selected = 0 },
                { .basic = create_basic_dirent("file2.cpp", ent_kind::file), .is_filtered_out = 0, .is_selected = 0 },
                { .basic = create_basic_dirent("file3.cpp", ent_kind::file), .is_filtered_out = 0, .is_selected = 0 },
                { .basic = create_basic_dirent("file4.cpp", ent_kind::file), .is_filtered_out = 0, .is_selected = 0 },
            };
            std::vector<bulk_rename_op> input_renames = {
                // none
            };
            std::vector<bulk_rename_collision> expected_collisions = {
                // none
            };

            auto actual = bulk_rename_find_collisions(dest, input_renames);

            ntest::assert_stdvec(expected_collisions, actual.collisions);
        }

        {
            std::vector<explorer_window::dirent> dest = {
                { .basic = create_basic_dirent("file0", ent_kind::file), .is_filtered_out = 0, .is_selected = 1 },
                { .basic = create_basic_dirent("file1", ent_kind::file), .is_filtered_out = 0, .is_selected = 1 },
                { .basic = create_basic_dirent("file2", ent_kind::file), .is_filtered_out = 0, .is_selected = 1 },
                { .basic = create_basic_dirent("file3", ent_kind::file), .is_filtered_out = 0, .is_selected = 1 },
            };
            std::vector<bulk_rename_op> input_renames = {
                bulk_rename_op( &dest[0].basic, "file1" ),
                bulk_rename_op( &dest[1].basic, "file2" ),
                bulk_rename_op( &dest[2].basic, "file3" ),
                bulk_rename_op( &dest[3].basic, "file4" ),
            };
            std::vector<bulk_rename_collision> expected_collisions = {
                // none
            };

            auto actual = bulk_rename_find_collisions(dest, input_renames);

            ntest::assert_stdvec(expected_collisions, actual.collisions);
        }

        {
            std::vector<explorer_window::dirent> dest = {
                { .basic = create_basic_dirent("file0", ent_kind::file), .is_filtered_out = 0, .is_selected = 1 },
                { .basic = create_basic_dirent("file1", ent_kind::file), .is_filtered_out = 0, .is_selected = 1 },
                { .basic = create_basic_dirent("file2", ent_kind::file), .is_filtered_out = 0, .is_selected = 1 },
                { .basic = create_basic_dirent("file3", ent_kind::file), .is_filtered_out = 0, .is_selected = 0 },
            };
            std::vector<bulk_rename_op> input_renames = {
                bulk_rename_op( &dest[0].basic, "file1" ),
                bulk_rename_op( &dest[1].basic, "file2" ),
                bulk_rename_op( &dest[2].basic, "file3" ),
                //! NOTE: this will get sorted in bulk_rename_find_collisions, order to check against will be:
                // file3
                // file2
                // file1
            };
            std::vector<bulk_rename_collision> expected_collisions = {
                { .dest_dirent = &dest[3].basic, .first_rename_pair_idx = 0, .last_rename_pair_idx = 0 },
            };

            auto actual = bulk_rename_find_collisions(dest, input_renames);

            ntest::assert_stdvec(expected_collisions, actual.collisions);
        }

        {
            std::vector<explorer_window::dirent> dest = {
                { .basic = create_basic_dirent("file0", ent_kind::file), .is_filtered_out = 0, .is_selected = 1 },
                { .basic = create_basic_dirent("file1", ent_kind::file), .is_filtered_out = 0, .is_selected = 1 },
                { .basic = create_basic_dirent("file2", ent_kind::file), .is_filtered_out = 0, .is_selected = 1 },
                { .basic = create_basic_dirent("file3", ent_kind::file), .is_filtered_out = 0, .is_selected = 0 },
            };
            std::vector<bulk_rename_op> input_renames = {
                bulk_rename_op( &dest[0].basic, "file3" ),
                bulk_rename_op( &dest[1].basic, "file3" ),
                bulk_rename_op( &dest[2].basic, "file3" ),
            };
            std::vector<bulk_rename_collision> expected_collisions = {
                { .dest_dirent = &dest[3].basic, .first_rename_pair_idx = 0, .last_rename_pair_idx = 2 },
            };

            auto actual = bulk_rename_find_collisions(dest, input_renames);

            ntest::assert_stdvec(expected_collisions, actual.collisions);
        }

        {
            std::vector<explorer_window::dirent> dest = {
                { .basic = create_basic_dirent(".cpp", ent_kind::file), .is_filtered_out = 0, .is_selected = 1 },
                { .basic = create_basic_dirent(".cpp", ent_kind::file), .is_filtered_out = 0, .is_selected = 1 },
                { .basic = create_basic_dirent(".cpp", ent_kind::file), .is_filtered_out = 0, .is_selected = 1 },
                { .basic = create_basic_dirent(".cpp", ent_kind::file), .is_filtered_out = 0, .is_selected = 0 },
            };
            std::vector<bulk_rename_op> input_renames = {
                bulk_rename_op( &dest[0].basic, "file5" ),
                bulk_rename_op( &dest[1].basic, "file5" ),
                bulk_rename_op( &dest[2].basic, "file5" ),
            };
            std::vector<bulk_rename_collision> expected_collisions = {
                { .dest_dirent = nullptr, .first_rename_pair_idx = 0, .last_rename_pair_idx = 2 },
            };

            auto actual = bulk_rename_find_collisions(dest, input_renames);

            ntest::assert_stdvec(expected_collisions, actual.collisions);
        }

        {
            std::vector<explorer_window::dirent> dest = {
                { .basic = create_basic_dirent("file0", ent_kind::file), .is_filtered_out = 0, .is_selected = 1 },
                { .basic = create_basic_dirent("file1", ent_kind::file), .is_filtered_out = 0, .is_selected = 1 },
                { .basic = create_basic_dirent("file2", ent_kind::file), .is_filtered_out = 0, .is_selected = 1 },
                { .basic = create_basic_dirent("file3", ent_kind::file), .is_filtered_out = 0, .is_selected = 0 },
                { .basic = create_basic_dirent("file4", ent_kind::file), .is_filtered_out = 0, .is_selected = 0 },
                { .basic = create_basic_dirent("file5", ent_kind::file), .is_filtered_out = 0, .is_selected = 0 },
            };
            std::vector<bulk_rename_op> input_renames = {
                bulk_rename_op( &dest[0].basic, "file3" ),
                bulk_rename_op( &dest[1].basic, "file4" ),
                bulk_rename_op( &dest[2].basic, "file5" ),
                //! NOTE: this will get sorted in bulk_rename_find_collisions, order to check against will be:
                // file5
                // file4
                // file3
            };
            std::vector<bulk_rename_collision> expected_collisions = {
                { .dest_dirent = &dest[5].basic, .first_rename_pair_idx = 0, .last_rename_pair_idx = 0 },
                { .dest_dirent = &dest[4].basic, .first_rename_pair_idx = 1, .last_rename_pair_idx = 1 },
                { .dest_dirent = &dest[3].basic, .first_rename_pair_idx = 2, .last_rename_pair_idx = 2 },
            };

            auto actual = bulk_rename_find_collisions(dest, input_renames);

            ntest::assert_stdvec(expected_collisions, actual.collisions);
        }

        {
            std::vector<explorer_window::dirent> dest = {
                { .basic = create_basic_dirent("file0", ent_kind::file), .is_filtered_out = 0, .is_selected = 1 },
                { .basic = create_basic_dirent("file1", ent_kind::file), .is_filtered_out = 0, .is_selected = 1 },
                { .basic = create_basic_dirent("file2", ent_kind::file), .is_filtered_out = 0, .is_selected = 0 },
            };
            std::vector<bulk_rename_op> input_renames = {
                bulk_rename_op( &dest[0].basic, "file2" ),
                bulk_rename_op( &dest[1].basic, "file2" ),
            };
            std::vector<bulk_rename_collision> expected_collisions = {
                { .dest_dirent = &dest[2].basic, .first_rename_pair_idx = 0, .last_rename_pair_idx = 1 },
            };

            auto actual = bulk_rename_find_collisions(dest, input_renames);

            ntest::assert_stdvec(expected_collisions, actual.collisions);
        }

        {
            std::vector<explorer_window::dirent> dest = {
                { .basic = create_basic_dirent("file0", ent_kind::file), .is_filtered_out = 0, .is_selected = 1 },
                { .basic = create_basic_dirent("file1", ent_kind::file), .is_filtered_out = 0, .is_selected = 0 },
                { .basic = create_basic_dirent("file2", ent_kind::file), .is_filtered_out = 0, .is_selected = 1 },
                { .basic = create_basic_dirent("file3", ent_kind::file), .is_filtered_out = 0, .is_selected = 0 },
                { .basic = create_basic_dirent("file4", ent_kind::file), .is_filtered_out = 0, .is_selected = 1 },
            };
            std::vector<bulk_rename_op> input_renames = {
                bulk_rename_op( &dest[0].basic, "file1" ),
                bulk_rename_op( &dest[2].basic, "fileX" ),
                bulk_rename_op( &dest[4].basic, "file3" ),
                //! NOTE: this will get sorted in bulk_rename_find_collisions, order to check against will be:
                // fileX
                // file3
                // file1
            };
            std::vector<bulk_rename_collision> expected_collisions = {
                { .dest_dirent = &dest[3].basic, .first_rename_pair_idx = 1, .last_rename_pair_idx = 1 },
                { .dest_dirent = &dest[1].basic, .first_rename_pair_idx = 2, .last_rename_pair_idx = 2 },
            };

            auto actual = bulk_rename_find_collisions(dest, input_renames);

            ntest::assert_stdvec(expected_collisions, actual.collisions);
        }

        {
            std::vector<explorer_window::dirent> dest = {
                { .basic = create_basic_dirent("00b4dP", ent_kind::file), .is_filtered_out = 0, .is_selected = 1 },
                { .basic = create_basic_dirent("00BbVr", ent_kind::file), .is_filtered_out = 0, .is_selected = 1 },
                { .basic = create_basic_dirent("01MWKO", ent_kind::file), .is_filtered_out = 0, .is_selected = 1 },
                { .basic = create_basic_dirent("01Sw7U", ent_kind::file), .is_filtered_out = 0, .is_selected = 1 },
                { .basic = create_basic_dirent("02MugF", ent_kind::file), .is_filtered_out = 0, .is_selected = 1 },
            };
            std::vector<bulk_rename_op> input_renames = {
                bulk_rename_op( &dest[0].basic, "0b" ),
                bulk_rename_op( &dest[1].basic, "0b" ),
                bulk_rename_op( &dest[2].basic, "0b" ),
                bulk_rename_op( &dest[3].basic, "0b" ),
                bulk_rename_op( &dest[4].basic, "0b" ),
                //! NOTE: this will get sorted in bulk_rename_find_collisions
            };
            std::vector<bulk_rename_collision> expected_collisions = {
                { .dest_dirent = nullptr, .first_rename_pair_idx = 0, .last_rename_pair_idx = 4 },
            };

            auto actual = bulk_rename_find_collisions(dest, input_renames);

            ntest::assert_stdvec(expected_collisions, actual.collisions);
        }

        {
            std::vector<explorer_window::dirent> dest = {
                { .basic = create_basic_dirent("bla_bla", ent_kind::file), .is_filtered_out = 0, .is_selected = 1 },
                { .basic = create_basic_dirent("test1",   ent_kind::file), .is_filtered_out = 0, .is_selected = 1 },
                { .basic = create_basic_dirent("test2",   ent_kind::file), .is_filtered_out = 0, .is_selected = 0 },
            };
            std::vector<bulk_rename_op> input_renames = {
                bulk_rename_op( &dest[0].basic, "bla_2" ),
                bulk_rename_op( &dest[1].basic, "test2" ),
                //! NOTE: this will get sorted in bulk_rename_find_collisions
            };
            std::vector<bulk_rename_collision> expected_collisions = {
                { .dest_dirent = &dest[2].basic, .first_rename_pair_idx = 0, .last_rename_pair_idx = 0 },
            };

            auto actual = bulk_rename_find_collisions(dest, input_renames);

            ntest::assert_stdvec(expected_collisions, actual.collisions);
            ntest::assert_cstr(path_create("test1").data(), actual.sorted_renames[0].before->path.data());
            ntest::assert_cstr(path_create("bla_bla").data(), actual.sorted_renames[1].before->path.data());
        }
    }
    #endif
