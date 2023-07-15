#ifndef SWAN_PATH_CPP
#define SWAN_PATH_CPP

#include <array>
#include <cassert>
#include <cstring>

#include <windows.h>
#include <shlwapi.h>
#undef min

#include "path.hpp"

using swan::path_t;

path_t swan::path_create(char const *data) noexcept(true)
{
    path_t p = {};

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

u16 swan::path_length(path_t const &path) noexcept(true)
{
    return (u16)strnlen(path.data(), UINT16_MAX);
}

char swan::path_pop_back(path_t &path) noexcept(true)
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

bool swan::path_pop_back_if(path_t &path, char if_ch) noexcept(true)
{
    u64 len = path_length(path);

    if (len > 0 && (path[len - 1] == if_ch)) {
        path[len - 1] = '\0';
        return true;
    } else {
        return false;
    }
}

bool swan::path_pop_back_if_not(path_t &path, char if_not_ch) noexcept(true)
{
    u64 len = path_length(path);

    if (len > 0 && (path[len - 1] != if_not_ch)) {
        path[len - 1] = '\0';
        return true;
    } else {
        return false;
    }
}

bool swan::path_is_empty(path_t const &path) noexcept(true)
{
    assert(path.size() > 0);

    return path[0] == '\0';
}

bool swan::path_ends_with(path_t const &path, char const *end) noexcept(true)
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

bool swan::path_ends_with_one_of(path_t const &path, char const *chars) noexcept(true)
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

void swan::path_clear(path_t &path) noexcept(true)
{
    assert(path.size() > 0);

    path[0] = '\0';
}

void swan::path_force_separator(path_t &path, char dir_separator) noexcept(true)
{
    u64 path_len = path_length(path);

    for (u64 i = 0; i < path_len; ++i) {
        if (strchr("\\/", path.data()[i])) {
            path[i] = dir_separator;
        }
    }
}

u64 swan::path_append(
    path_t &path,
    char const *str,
    char dir_separator,
    bool prepend_sep,
    bool postpend_sep) noexcept(true)
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

bool swan::path_loosely_same(path_t const &p1, path_t const &p2) noexcept(true)
{
    u16 p1_len = path_length(p1);
    u16 p2_len = path_length(p2);
    i32 len_diff = +((i32)p1_len - (i32)p2_len);

    if (len_diff <= 1) {
        return StrCmpNIA(p1.data(), p2.data(), std::min(p1_len, p2_len)) == 0;
    }
    else {
        return false;
    }
}

path_t swan::path_squish_adjacent_separators(path_t const &path) noexcept(true)
{
    // shoutout to ChatGPT for writing this for me

    // Variable to store the cleaned-up path
    path_t cleaned_path;
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

#endif // SWAN_PATH_CPP
