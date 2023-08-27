#include <array>
#include <cassert>
#include <cstring>
#include <algorithm>

#include <windows.h>
#include <shlwapi.h>
#undef min
#undef max

#include "path.hpp"

swan_path_t path_create(char const *data) noexcept
{
    swan_path_t p = {};

    if (data == nullptr) {
        return p;
    }

    u16 data_len = (u16)strnlen(data, UINT16_MAX);

    if (data_len >= p.max_size()) {
        return p;
    }

    strcpy(p.data(), data);

    return p;
}

u16 path_length(swan_path_t const &path) noexcept
{
    return (u16)strnlen(path.data(), UINT16_MAX);
}

bool path_equals_exactly(swan_path_t const &p1, char const *p2) noexcept
{
    return strcmp(p1.data(), p2) == 0;
}

bool path_equals_exactly(swan_path_t const &p1, swan_path_t const &p2) noexcept
{
    return strcmp(p1.data(), p2.data()) == 0;
}

char path_pop_back(swan_path_t &path) noexcept
{
    u64 len = path_length(path);

    if (len > 0) {
        char back = path[len - 1];
        path[len - 1] = '\0';
        return back;
    }
    else {
        return '\0';
    }
}

bool path_pop_back_if(swan_path_t &path, char if_ch) noexcept
{
    u64 len = path_length(path);

    if (len > 0 && (path[len - 1] == if_ch)) {
        path[len - 1] = '\0';
        return true;
    } else {
        return false;
    }
}

bool path_pop_back_if_not(swan_path_t &path, char if_not_ch) noexcept
{
    u64 len = path_length(path);

    if (len > 0 && (path[len - 1] != if_not_ch)) {
        path[len - 1] = '\0';
        return true;
    } else {
        return false;
    }
}

bool path_is_empty(swan_path_t const &path) noexcept
{
    assert(path.size() > 0);

    return path[0] == '\0';
}

bool path_ends_with(swan_path_t const &path, char const *end) noexcept
{
    assert(end != nullptr);

    u64 path_len = path_length(path);
    u64 end_len = strlen(end);

    if (path_len == 0 || end_len > path_len) {
        return false;
    }
    else {
        return strcmp(path.data() + (path_len - end_len), end) == 0;
    }
}

bool path_ends_with_one_of(swan_path_t const &path, char const *chars) noexcept
{
    assert(chars != nullptr);
    assert(strlen(chars) > 0);

    u64 path_len = path_length(path);

    if (path_len == 0) {
        return false;
    }
    else {
        char last_ch = path[path_len - 1];
        return strchr(chars, last_ch);
    }
}

void path_clear(swan_path_t &path) noexcept
{
    assert(path.size() > 0);

    path[0] = '\0';
}

void path_force_separator(swan_path_t &path, char dir_separator) noexcept
{
    u64 path_len = path_length(path);

    for (u64 i = 0; i < path_len; ++i) {
        if (strchr("\\/", path.data()[i])) {
            path[i] = dir_separator;
        }
    }
}

u64 path_append(
    swan_path_t &path,
    char const *str,
    char dir_separator,
    bool prepend_sep,
    bool postpend_sep) noexcept
{
    if (str == nullptr) {
        return 0;
    }

    u64 str_len = strlen(str);

    if (str_len == 0) {
        return 0;
    }

    u64 path_len = path_length(path);

    u64 space_left = path.size() - 1 - path_len;

    static_assert(u8(false) == 0);
    static_assert(u8(true) == 1);
    u64 desired_append_len = str_len + u8(prepend_sep) + u8(postpend_sep);

    bool can_fit = space_left >= desired_append_len;

    if (!can_fit) {
        return 0;
    }

    char const separator_buf[] = { dir_separator, '\0' };

    if (prepend_sep && path[path_len - 1] != dir_separator) {
        (void) strncat(path.data(), separator_buf, 1);
    }

    (void) strncat(path.data(), str, str_len);

    if (postpend_sep) {
        (void) strncat(path.data(), separator_buf, 1);
    }

    return 1;
}

bool path_loosely_same(swan_path_t const &p1, swan_path_t const &p2) noexcept
{
    u16 p1_len = path_length(p1);
    u16 p2_len = path_length(p2);
    i32 len_diff = (i32)std::max(p1_len, p2_len) - (i32)std::min(p1_len, p2_len);

    if (len_diff <= 1) {
        return StrCmpNIA(p1.data(), p2.data(), std::min(p1_len, p2_len)) == 0;
    }
    else {
        return false;
    }
}

swan_path_t path_squish_adjacent_separators(swan_path_t const &path) noexcept
{
    // shoutout to ChatGPT for writing this for me

    // Variable to store the cleaned-up path
    swan_path_t cleaned_path;
    u64 cleaned_index = 0;
    u64 path_len = path_length(path);

    char previous_char = '\0';

    for (u64 i = 0; i < path_len; ++i)
    {
        char current_char = path[i];

        if (strchr("\\/", current_char) && strchr("\\/", previous_char))
        {
            // Skip the additional consecutive slashes
            continue;
        }

        // Add the current character to the cleaned-up path
        cleaned_path[cleaned_index] = current_char;
        ++cleaned_index;

        // Update the previous character
        previous_char = current_char;
    }

    // Fill the remaining characters in the cleaned-up path with null characters
    for (; cleaned_index < cleaned_path.size(); ++cleaned_index)
    {
        cleaned_path[cleaned_index] = '\0';
    }

    return cleaned_path;
}
