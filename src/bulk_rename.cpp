#ifndef SWAN_BULK_RENAME_CPP
#define SWAN_BULK_RENAME_CPP

#include <string>
#include <cassert>
#include <iostream>

#include <shlwapi.h>

#include "common.hpp"
#include "path.hpp"

namespace bulk_rename {

    using path_t = swan::path_t;

    struct apply_pattern_result {
        bool success;
        std::array<char, 128> error_msg;
    };

    apply_pattern_result apply_pattern(
        char const *name,
        char const *ext,
        std::array<char, (256 * 4) + 1> &after,
        char const *pattern,
        i32 counter,
        u64 bytes,
        bool squish_adjacent_spaces) noexcept(true)
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

                    // std::cout << "[" << opening_chevron_pos << "," << closing_chevron_pos << "] " << expr_len << ' ' << pattern + opening_chevron_pos + 1 << '\n';

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

    struct rename_pair {
        basic_dir_ent *before;
        path_t after;

        // for ntest
        bool operator!=(rename_pair const &other) const noexcept(true)
        {
            return this->before != other.before || !swan::path_equals_exactly(this->after, other.after);
        }

        // for ntest
        friend std::ostream& operator<<(std::ostream &os, rename_pair const &r)
        {
            return os << "B:[" << (r.before ? r.before->path.data() : "nullptr") << "] A:[" << r.after.data() << ']';
        }
    };

    struct collision_1 {
        basic_dir_ent *dest_dirent;
        rename_pair rename;

        // for ntest
        bool operator!=(collision_1 const &other) const noexcept(true)
        {
            return
                !swan::path_equals_exactly(other.dest_dirent->path, this->dest_dirent->path) ||
                !swan::path_equals_exactly(other.rename.before->path, this->rename.before->path) ||
                !swan::path_equals_exactly(other.rename.after, this->rename.after);
        }

        // for ntest
        friend std::ostream& operator<<(std::ostream &os, collision_1 const &c)
        {
            return os << "D:[" << (c.dest_dirent ? c.dest_dirent->path.data() : "nullptr")
                      << "] B:[" << c.rename.before->path.data()
                      << "] A:[" << c.rename.after.data() << ']';
        }
    };

    std::vector<collision_1> find_collisions_1(
        std::vector<explorer_window::dir_ent> &dest,
        std::vector<rename_pair> &renames) noexcept(true)
    {
        std::vector<collision_1> collisions = {};
        collisions.reserve(dest.size());

        static std::vector<explorer_window::dir_ent *> unaffected_dirents = {};
        unaffected_dirents.clear();
        unaffected_dirents.reserve(dest.size());

        for (auto &dest_dirent : dest) {
            bool affected = false;

            for (auto &rename : renames) {
                if (rename.before == &dest_dirent.basic) {
                    affected = true;
                    break;
                }
            }

            if (!affected) {
                unaffected_dirents.push_back(&dest_dirent);
            }
        }

        for (auto &rename : renames) {
            for (auto &unaffected_dirent : unaffected_dirents) {
                if (rename.before == &unaffected_dirent->basic) {
                    continue;
                }
                if (swan::path_equals_exactly(unaffected_dirent->basic.path, rename.after)) {
                    collision_1 c;
                    c.dest_dirent = &unaffected_dirent->basic;
                    c.rename = rename;
                    collisions.push_back(c);
                }
            }
        }

        return collisions;
    }

    struct collision_2 {
        basic_dir_ent *dest_dirent;
        u64 first_rename_pair_idx;
        u64 last_rename_pair_idx;

        // for ntest
        bool operator!=(collision_2 const &other) const noexcept(true)
        {
            return
                this->first_rename_pair_idx != other.first_rename_pair_idx ||
                this->last_rename_pair_idx != other.last_rename_pair_idx ||
                this->dest_dirent != other.dest_dirent;
        }

        // for ntest
        friend std::ostream& operator<<(std::ostream &os, collision_2 const &c)
        {
            os << "D:[" << (c.dest_dirent ? c.dest_dirent->path.data() : "") << "] ["
               << c.first_rename_pair_idx << ',' << c.last_rename_pair_idx << ']';
            return os;
        }
    };

    void sort_renames_dup_elem_sequences_after_non_dups(std::vector<rename_pair> &renames) noexcept(true)
    {
        using swan::path_equals_exactly;

        // sort renames such that adjacently equal renames appear at the end.
        // examples:
        //  [1,2,5,7,2,4] -> [2,2,6,5,4,1]
        //  [0,0,1,5,5,2] -> [5,5,0,0,2,1]
        // (I couldn't figure out how to do it in ascending order... descending will do.)

        std::stable_sort(renames.begin(), renames.end(), [&](rename_pair const &a, rename_pair const &b) {
            i32 cmp = strcmp(a.after.data(), b.after.data());
            if (cmp == 0) {
                return false;
            } else {
                return cmp > 0;
            }
        });
    }

    std::vector<collision_2> find_collisions_2(
        std::vector<explorer_window::dir_ent> &dest,
        std::vector<rename_pair> &renames) noexcept(true)
    {
        std::vector<collision_2> collisions = {};

        if (renames.empty()) {
            return collisions;
        }

        collisions.reserve(dest.size());

        sort_renames_dup_elem_sequences_after_non_dups(renames);

        static std::vector<explorer_window::dir_ent *> unaffected_dirents = {};
        unaffected_dirents.clear();
        unaffected_dirents.reserve(dest.size());

        for (auto &dest_dirent : dest) {
            if (!dest_dirent.is_selected) {
                unaffected_dirents.push_back(&dest_dirent);
            }
        }

        u64 const npos = std::string::npos;

        auto exists = [npos](u64 pos) {
            return pos != npos;
        };

        auto find_conflict_in_dest = [&](rename_pair const &rename) -> basic_dir_ent* {
            for (auto &dirent : unaffected_dirents) {
                if (swan::path_equals_exactly(dirent->basic.path, rename.after)) {
                    return &dirent->basic;
                }
            }
            return nullptr;
        };

        auto adj_begin = std::adjacent_find(renames.begin(), renames.end(), [](rename_pair const &r0, rename_pair const &r1) {
            return swan::path_equals_exactly(r0.after, r1.after);
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
        if (adj_begin != renames.end() && swan::path_equals_exactly(renames.front().after, renames.back().after)) {
            ptrdiff_t start_index = std::distance(renames.begin(), adj_begin);
            u64 first = (u64)start_index;
            u64 last = renames.size() - 1;
            auto conflict = find_conflict_in_dest(renames[first]);
            collisions.emplace_back(conflict, first, last);
        } else {
            ptrdiff_t start_index = std::distance(renames.begin(), adj_begin);
            u64 i = (u64)start_index + 1;
            u64 first = i - 1, last = npos;

            for (; i < renames.size(); ++i) {
                if (!swan::path_equals_exactly(renames[i].after, renames[first].after)) {
                    last = i-1;
                    auto conflict = find_conflict_in_dest(renames[first]);
                    collisions.emplace_back(conflict, first, last);

                    first = i;
                    last = npos;
                }
            }
        }

        return collisions;
    }

} // namespace bulk_rename

#endif // SWAN_BULK_RENAME_CPP
