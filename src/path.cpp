#include "stdafx.hpp"
#include "path.hpp"

swan_path path_create(char const *data, u64 count) noexcept
{
    swan_path p = {};

#if DEBUG_MODE
    assert(data != nullptr);
    assert(strlen(data) < p.max_size());
#endif

    strncpy(p.data(), data, std::min(count, p.max_size() - 1));

    return p;
}

u16 path_length(swan_path const &path) noexcept
{
    return (u16)strnlen(path.data(), UINT16_MAX);
}

bool path_equals_exactly(swan_path const &p1, char const *p2) noexcept
{
    return strcmp(p1.data(), p2) == 0;
}

bool path_equals_exactly(swan_path const &p1, swan_path const &p2) noexcept
{
    return strcmp(p1.data(), p2.data()) == 0;
}

char path_pop_back(swan_path &path) noexcept
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

bool path_pop_back_if(swan_path &path, char if_ch) noexcept
{
    u64 len = path_length(path);

    if (len > 0 && (path[len - 1] == if_ch)) {
        path[len - 1] = '\0';
        return true;
    } else {
        return false;
    }
}

bool path_pop_back_if(swan_path &path, char const *chs) noexcept
{
    u64 len = path_length(path);

    if (len > 0 && strchr(chs, path[len - 1])) {
        path[len - 1] = '\0';
        return true;
    } else {
        return false;
    }
}

bool path_pop_back_if_not(swan_path &path, char if_not_ch) noexcept
{
    u64 len = path_length(path);

    if (len > 0 && (path[len - 1] != if_not_ch)) {
        path[len - 1] = '\0';
        return true;
    } else {
        return false;
    }
}

bool path_is_empty(swan_path const &path) noexcept
{
    return path[0] == '\0';
}

bool path_ends_with(swan_path const &path, char const *end) noexcept
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

bool path_ends_with_one_of(swan_path const &path, char const *chars) noexcept
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

void path_clear(swan_path &path) noexcept
{
    assert(path.size() > 0);
    path[0] = '\0';
}

void path_force_separator(swan_path &path, char dir_separator) noexcept
{
    u64 path_len = path_length(path);

    for (u64 i = 0; i < path_len; ++i) {
        if (strchr("\\/", path.data()[i])) {
            path[i] = dir_separator;
        }
    }
}

u64 path_append(
    swan_path &path,
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

bool path_loosely_same(char const *p1, char const *p2, u64 p1_len, u64 p2_len) noexcept
{
    if (p1_len == u64(-1)) {
        p1_len = strlen(p1);
    }
    if (p2_len == u64(-1)) {
        p2_len = strlen(p2);
    }

    s32 len_diff = (s32)std::max(p1_len, p2_len) - (s32)std::min(p1_len, p2_len);
    assert(len_diff >= 0);

    if (len_diff == 0) {
        return StrCmpNIA(p1, p2, (s32)std::min(p1_len, p2_len)) == 0;
    }
    else {
        bool same_beginning = StrCmpNIA(p1, p2, (s32)std::min(p1_len, p2_len)) == 0;
        bool all_rest_are_separators = {};

        if (p1_len > p2_len) { // p1 is longer one
            all_rest_are_separators = std::all_of(p1 + p1_len - len_diff,
                                                  p1 + p1_len,
                                                  [](char ch) noexcept { return strchr("\\/", ch); });
        } else { // p2 is longer one
            all_rest_are_separators = std::all_of(p2 + p2_len - len_diff,
                                                  p2 + p2_len,
                                                  [](char ch) noexcept { return strchr("\\/", ch); });
        }

        return same_beginning && all_rest_are_separators;
    }
}

bool path_loosely_same(swan_path const &p1, char const *p2, u64 p2_len) noexcept
{
    return path_loosely_same(p1.data(), p2, path_length(p1), p2_len);
}

bool path_loosely_same(char const *p1, swan_path const &p2, u64 p1_len) noexcept
{
    return path_loosely_same(p1, p2.data(), p1_len, path_length(p2));
}

bool path_loosely_same(swan_path const &p1, swan_path const &p2) noexcept
{
    return path_loosely_same(p1.data(), p2.data(), path_length(p1), path_length(p2));
}

swan_path path_squish_adjacent_separators(swan_path const &path) noexcept
{
    // shoutout to ChatGPT for writing this for me

    swan_path cleaned_path;
    u64 cleaned_index = 0;
    u64 path_len = path_length(path);

    char previous_char = '\0';

    for (u64 i = 0; i < path_len; ++i) {
        char current_char = path[i];

        if (strchr("\\/", current_char) && strchr("\\/", previous_char)) {
            continue; // Skip the additional consecutive slashes
        }

        // Add the current character to the cleaned-up path
        cleaned_path[cleaned_index] = current_char;
        ++cleaned_index;

        // Update the previous character
        previous_char = current_char;
    }

    // Fill the remaining characters in the cleaned-up path with NUL characters
    for (; cleaned_index < cleaned_path.size(); ++cleaned_index) {
        cleaned_path[cleaned_index] = '\0';
    }

    return cleaned_path;
}

swan_path path_reconstruct_canonically(char const *path_utf8, char dir_sep_utf8) noexcept
{
    swan_path retval;

    try {
        std::filesystem::path fs_path(path_utf8);
        std::filesystem::path canonical_path = std::filesystem::canonical(fs_path);
        std::string canonical_string = canonical_path.string();
        retval = path_create(canonical_string.c_str());
    }
    catch (...) {
        retval = path_create(path_utf8);
    }

    return retval;
}
