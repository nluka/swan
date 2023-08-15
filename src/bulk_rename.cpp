#ifndef SWAN_BULK_RENAME_CPP
#define SWAN_BULK_RENAME_CPP

#include <string>
#include <cassert>
#include <iostream>

#include <shlwapi.h>

#include "path.hpp"

namespace bulk_rename {

    using path_t = swan::path_t;

    struct apply_pattern_result {
        bool success;
        std::array<char, 128> error_msg;
    };

    apply_pattern_result apply_pattern(
        char const *before,
        std::array<char, (256 * 4) + 1> &after,
        char const *pattern,
        i32 counter,
        u64 bytes) noexcept(true)
    {
        assert(pattern != nullptr);

        std::array<char, 128> err_msg = {};

        after = {};

        if (pattern[0] == '\0') {
            snprintf(err_msg.data(), err_msg.size(), "empty pattern");
            return { false, err_msg };
        }

        // char const *expressions[] = {
        //     "counter",
        //     "before",
        //     "bytes",
        // };

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
                    {
                        closing_chevron_pos = i;
                        u64 expr_len = closing_chevron_pos - opening_chevron_pos - 1;

                        // std::cout << "[" << opening_chevron_pos << "," << closing_chevron_pos << "] " << expr_len << ' ' << pattern + opening_chevron_pos + 1 << '\n';

                        if (expr_len == 0) {
                            after = {};
                            snprintf(err_msg.data(), err_msg.size(), "empty expression starting at position %zu", opening_chevron_pos);
                            return { false, err_msg };
                        }

                        auto expr_equals = [&](char const *known_expr) {
                            return StrCmpNIA(pattern + opening_chevron_pos + 1, known_expr, expr_len) == 0;
                        };

                        bool known_expression = true;
                        i32 written = 0;
                        if (expr_equals("counter")) {
                            written = snprintf(after.data() + after_insert_idx, after.size() - after_insert_idx, "%d", counter);
                        }
                        else if (expr_equals("before")) {

                        }
                        else if (expr_equals("bytes")) {

                        }
                        else {
                            known_expression = false;
                        }

                        if (!known_expression) {
                            after = {};
                            snprintf(err_msg.data(), err_msg.size(), "unknown expression starting at position %zu", opening_chevron_pos);
                            return { false, err_msg };
                        }
                        else if ( (written - 1) > (after.size() - after_insert_idx) ) {
                            after = {};
                            snprintf(err_msg.data(), err_msg.size(), "not enough space for pattern");
                            return { false, err_msg };
                        }
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
                if (after_insert_idx == after.size()) {
                    after = {};
                    snprintf(err_msg.data(), err_msg.size(), "not enough space for pattern");
                    return { false, err_msg };
                } else {
                    after[after_insert_idx++] = ch;
                }
            }
        }

        return { true, "" };
    }

} // namespace bulk_rename

#endif // SWAN_BULK_RENAME_CPP
