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
    };

    struct collision {
        basic_dir_ent *dest_dirent;
        rename_pair rename;

        // for ntest
        bool operator!=(collision const &other) const noexcept(true)
        {
            return
                !swan::path_equals_exactly(other.dest_dirent->path, this->dest_dirent->path) ||
                !swan::path_equals_exactly(other.rename.before->path, this->rename.before->path) ||
                !swan::path_equals_exactly(other.rename.after, this->rename.after);
        }

        // for ntest
        friend std::ostream& operator<<(std::ostream &os, collision const &c)
        {
            os << "D:[" << c.dest_dirent->path.data()
               << "] B:[" << c.rename.before->path.data()
               << "] A:[" << c.rename.after.data() << ']';
            return os;
        }
    };

    std::vector<collision> find_collisions(
        std::vector<explorer_window::dir_ent> &dest,
        std::vector<rename_pair> &renames) noexcept(true)
    {
        std::vector<collision> collisions = {};

        std::vector<explorer_window::dir_ent *> unaffected_dirents = {};
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
                    collision c;
                    c.dest_dirent = &unaffected_dirent->basic;
                    c.rename = rename;
                    collisions.push_back(c);
                }
            }
        }

        return collisions;
    }

} // namespace bulk_rename

#endif // SWAN_BULK_RENAME_CPP
