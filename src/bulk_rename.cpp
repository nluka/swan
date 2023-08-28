#pragma once

#include <string>
#include <cassert>
#include <iostream>

#include <shlwapi.h>

#include "common.hpp"
#include "path.hpp"

// for ntest
bool bulk_rename_op::operator!=(bulk_rename_op const &other) const noexcept
{
    return this->before != other.before || !path_equals_exactly(this->after, other.after);
}

// for ntest
std::ostream& operator<<(std::ostream &os, bulk_rename_op const &r)
{
    return os << "B:[" << (r.before ? r.before->path.data() : "nullptr") << "] A:[" << r.after.data() << ']';
}

// for ntest
bool bulk_rename_collision::operator!=(bulk_rename_collision const &other) const noexcept
{
    return
        this->first_rename_pair_idx != other.first_rename_pair_idx ||
        this->last_rename_pair_idx != other.last_rename_pair_idx ||
        this->dest_dirent != other.dest_dirent;
}

// for ntest
std::ostream& operator<<(std::ostream &os, bulk_rename_collision const &c)
{
    return os << "D:[" << (c.dest_dirent ? c.dest_dirent->path.data() : "")
                << "] [" << c.first_rename_pair_idx << ',' << c.last_rename_pair_idx << ']';
}

bulk_rename_transform_result bulk_rename_transform(
    char const *name,
    char const *ext,
    std::array<char, (256 * 4) + 1> &after,
    char const *pattern,
    i32 counter,
    u64 bytes,
    bool squish_adjacent_spaces) noexcept
{
    assert(pattern != nullptr);

    std::array<char, 128> err_msg = {};

    after = {};

    if (pattern[0] == '\0') {
        snprintf(err_msg.data(), err_msg.size(), "empty pattern");
        return { false, err_msg };
    }

    u64 opening_chevron_pos = std::string::npos;
    u64 closing_chevron_pos = std::string::npos;
    u64 after_insert_idx = 0;

    for (u64 i = 0; pattern[i] != '\0'; ++i) {
        char ch = pattern[i];

        if (ch == '\0') {
            break;
        }

        char const *which_illegal_char = strchr("\\/\"|?*", ch);

        if (which_illegal_char) {
            after = {};
            snprintf(err_msg.data(), err_msg.size(), "contains illegal filename character '%c' at position %zu", *which_illegal_char, i);
            return { false, err_msg };
        }

        bool inside_chevrons = opening_chevron_pos != std::string::npos;

        if (ch == '<') {
            if (inside_chevrons) {
                after = {};
                snprintf(err_msg.data(), err_msg.size(), "unexpected '<' at position %zu, unclosed '<' at position %zu", i, opening_chevron_pos);
                return { false, err_msg };
            } else {
                opening_chevron_pos = i;
            }
        }
        else if (ch == '>') {
            if (inside_chevrons) {
                closing_chevron_pos = i;
                u64 expr_len = closing_chevron_pos - opening_chevron_pos - 1;

                if (expr_len == 0) {
                    after = {};
                    snprintf(err_msg.data(), err_msg.size(), "empty expression starting at position %zu", opening_chevron_pos);
                    return { false, err_msg };
                }

                auto expr_equals = [&](char const *known_expr) {
                    return StrCmpNIA(pattern + opening_chevron_pos + 1, known_expr, (i32)expr_len) == 0;
                };

                bool known_expression = true;
                i32 written = 0;
                u64 space_left = after.size() - after_insert_idx;
                char *out = after.data() + after_insert_idx;

                if (expr_equals("name")) {
                    u64 len = strlen(name);

                    if (len <= space_left) {
                        strcat(out, name);
                        u64 spaces_removed = 0;
                        if (squish_adjacent_spaces) {
                            spaces_removed = remove_adjacent_spaces(out, len);
                        }
                        after_insert_idx += len - spaces_removed;
                    }
                }
                else if (expr_equals("ext")) {
                    if (ext != nullptr) {
                        u64 len = strlen(ext);
                        if (len <= space_left) {
                            strcat(out, ext);
                            after_insert_idx += len;
                        }
                    }
                }
                else if (expr_equals("counter")) {
                    char buffer[11] = {};
                    written = snprintf(buffer, lengthof(buffer), "%d", counter);
                    if (written <= space_left) {
                        strcat(out, buffer);
                        after_insert_idx += written;
                    }
                }
                else if (expr_equals("bytes")) {
                    char buffer[21] = {};
                    written = snprintf(buffer, lengthof(buffer), "%zu", bytes);
                    if (written <= space_left) {
                        strcat(out, buffer);
                        after_insert_idx += written;
                    }
                }
                else {
                    known_expression = false;
                }

                if (!known_expression) {
                    after = {};
                    snprintf(err_msg.data(), err_msg.size(), "unknown expression starting at position %zu", opening_chevron_pos);
                    return { false, err_msg };
                }
                else if (written > space_left) {
                    after = {};
                    snprintf(err_msg.data(), err_msg.size(), "not enough space for pattern");
                    return { false, err_msg };
                }

                // reset
                opening_chevron_pos = std::string::npos;
                closing_chevron_pos = std::string::npos;
            } else {
                after = {};
                snprintf(err_msg.data(), err_msg.size(), "unexpected '>' at position %zu - no preceding '<' found", i);
                return { false, err_msg };
            }
        }
        else if (inside_chevrons) {
            // do nothing
        }
        else {
            // non-special character

            if (after_insert_idx == after.size()) {
                after = {};
                snprintf(err_msg.data(), err_msg.size(), "not enough space for pattern");
                return { false, err_msg };
            }

            if (squish_adjacent_spaces && i > 0 && pattern[i-1] == ' ' && ch == ' ') {
                // don't insert multiple adjacent spaces
            } else {
                after[after_insert_idx++] = ch;
            }
        }
    }

    return { true, "" };
}

void sort_renames_dup_elem_sequences_after_non_dups(std::vector<bulk_rename_op> &renames) noexcept
{
    // sort renames such that adjacently equal renames appear at the end.
    // examples:
    //  [1,2,5,7,2,4] -> [2,2,6,5,4,1]
    //  [0,0,1,5,5,2] -> [5,5,0,0,2,1]
    // (I couldn't figure out how to do it in ascending order... descending will do.)

    std::stable_sort(renames.begin(), renames.end(), [&](bulk_rename_op const &a, bulk_rename_op const &b) {
        i32 cmp = strcmp(a.after.data(), b.after.data());
        if (cmp == 0) {
            return false;
        } else {
            return cmp > 0;
        }
    });
}

std::vector<bulk_rename_collision> bulk_rename_find_collisions(
    std::vector<explorer_window::dirent> &dest,
    std::vector<bulk_rename_op> &renames) noexcept
{
    std::vector<bulk_rename_collision> collisions = {};

    if (renames.empty()) {
        return collisions;
    }

    collisions.reserve(dest.size());

    sort_renames_dup_elem_sequences_after_non_dups(renames);

    static std::vector<explorer_window::dirent *> unaffected_dirents = {};
    unaffected_dirents.clear();
    unaffected_dirents.reserve(dest.size());

    for (auto &dest_dirent : dest) {
        if (!dest_dirent.is_selected) {
            unaffected_dirents.push_back(&dest_dirent);
        }
    }

    u64 const npos = std::string::npos;

    auto find_conflict_in_dest = [&](bulk_rename_op const &rename) -> basic_dirent* {
        for (auto &dirent : unaffected_dirents) {
            if (path_equals_exactly(dirent->basic.path, rename.after)) {
                return &dirent->basic;
            }
        }
        return nullptr;
    };

    auto adj_begin = std::adjacent_find(renames.begin(), renames.end(), [](bulk_rename_op const &r0, bulk_rename_op const &r1) {
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

    return collisions;
}
