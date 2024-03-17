#include "common_functions.hpp"
#include "path.hpp"

bool bulk_rename_op::operator!=(bulk_rename_op const &other) const noexcept // for ntest
{
    return this->before != other.before || !path_equals_exactly(this->after, other.after);
}

std::ostream& operator<<(std::ostream &os, bulk_rename_op const &r) // for ntest
{
    return os << "B:[" << (r.before ? r.before->path.data() : "nullptr") << "] A:[" << r.after.data() << ']';
}

bool bulk_rename_collision::operator!=(bulk_rename_collision const &other) const noexcept // for ntest
{
    return
        this->first_rename_pair_idx != other.first_rename_pair_idx ||
        this->last_rename_pair_idx != other.last_rename_pair_idx ||
        this->dest_dirent != other.dest_dirent;
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
                    static std::regex valid_slice_with_start_end ("^[0-9]{1,5}, *[0-9]{1,5}$");
                    static std::regex valid_slice_with_start     ("^[0-9]{1,5}, *$");
                    static std::regex valid_slice_with_end                 ("^, *[0-9]{1,5}$");

                    std::string_view expr_view(expr, expr_len);
                    static std::string expr_str(100, '\0');
                    expr_str.clear();
                    expr_str.append(expr_view);

                    std::istringstream iss(expr_str);

                    op.kind = bulk_rename_compiled_pattern::op::type::insert_slice;

                    if (std::regex_match(expr_str, valid_slice_with_start_end)) {
                        op.explicit_first = true;
                        op.explicit_last = true;
                    }
                    else if (std::regex_match(expr_str, valid_slice_with_start)) {
                        op.explicit_first = true;
                    }
                    else if (std::regex_match(expr_str, valid_slice_with_end)) {
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

    for (auto const &op : compiled_pattern.ops) {
        u64 space_left = after.max_size() - after_insert_idx;
        char *out = after.data() + after_insert_idx;
        s32 written = 0;

        switch (op.kind) {
            using op_type = bulk_rename_compiled_pattern::op::type;

            case op_type::insert_char: {
                if (after_insert_idx < after.max_size()) {
                    after[after_insert_idx++] = op.ch;
                } else {
                    result.success = false;
                    (void) strncpy(result.error.data(), "not enough space for pattern", result.error.max_size());
                    return result;
                }
                break;
            }
            case op_type::insert_name: {
                u64 len = strlen(name);
                if (len <= space_left) {
                    (void) strcat(out, name);
                    after_insert_idx += len;
                } else {
                    result.success = false;
                    (void) strncpy(result.error.data(), "not enough space for pattern", result.error.max_size());
                    return result;
                }
                break;
            }
            case op_type::insert_ext: {
                if (ext != nullptr) {
                    u64 len = strlen(ext);
                    if (len <= space_left) {
                        (void) strcat(out, ext);
                        after_insert_idx += len;
                    } else {
                        result.success = false;
                        (void) strncpy(result.error.data(), "not enough space for pattern", result.error.max_size());
                        return result;
                    }
                }
                break;
            }
            case op_type::insert_dotext: {
                if (ext != nullptr) {
                    if (after_insert_idx < after.max_size()) {
                        after[after_insert_idx++] = '.';
                    } else {
                        result.success = false;
                        (void) strncpy(result.error.data(), "not enough space for pattern", result.error.max_size());
                        return result;
                    }

                    u64 len = strlen(ext);
                    if (len <= space_left) {
                        (void) strcat(out, ext);
                        after_insert_idx += len;
                    } else {
                        result.success = false;
                        (void) strncpy(result.error.data(), "not enough space for pattern", result.error.max_size());
                        return result;
                    }
                }
                break;
            }
            case op_type::insert_size: {
                auto buffer = make_str_static<21>("%zu", bytes);
                if (written <= space_left) {
                    (void) strcat(out, buffer.data());
                    after_insert_idx += written;
                } else {
                    result.success = false;
                    (void) strncpy(result.error.data(), "not enough space for pattern", result.error.max_size());
                    return result;
                }
                break;
            }
            case op_type::insert_counter: {
                auto buffer = make_str_static<11>("%d", counter);
                if (written <= space_left) {
                    (void) strcat(out, buffer.data());
                    after_insert_idx += written;
                } else {
                    result.success = false;
                    (void) strncpy(result.error.data(), "not enough space for pattern", result.error.max_size());
                    return result;
                }
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
                    result.success = false;
                    (void) strncpy(result.error.data(), "slice goes out of bounds", result.error.max_size());
                    return result;
                }

                if (len <= space_left) {
                    (void) strncat(out, full.data() + op.slice_first, len);
                    after_insert_idx += len;
                } else {
                    result.success = false;
                    (void) strncpy(result.error.data(), "not enough space for pattern", result.error.max_size());
                    return result;
                }
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

bulk_rename_find_collisions_result bulk_rename_find_collisions(
    std::vector<explorer_window::dirent> &dest,
    std::vector<bulk_rename_op> const &renames_in) noexcept
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
