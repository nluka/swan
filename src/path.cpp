#ifndef SWAN_PATH_CPP
#define SWAN_PATH_CPP

#include <array>
#include <cassert>

#include <Windows.h>

#include "primitives.cpp"

typedef std::array<char, MAX_PATH> path_t;

u64 path_length(path_t const &path) noexcept
{
    return strlen(path.data());
}

bool path_is_empty(path_t const &path)
{
    assert(path.size() > 0);

    return path[0] == '\0';
}

bool path_ends_with(path_t const &path, char const *end)
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

bool path_ends_with_one_of(path_t const &path, char const *chars)
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

void path_clear(path_t &path)
{
    assert(path.size() > 0);

    path[0] = '\0';
}

enum class path_append_result : i32
{
    nil = -1,
    success = 0,
    exceeds_max_path,
};

path_append_result path_append(path_t &path, char const *str, bool prepend_slash = false)
{
    static_assert(false == 0);
    static_assert(true == 1);

    assert(str != nullptr);

    u64 str_len = strlen(str);

    assert(str_len > 0);

    u64 space_left = path.size() - path_length(path) - 1;
    bool can_fit = space_left >= str_len + u64(prepend_slash);

    if (!can_fit) {
        return path_append_result::exceeds_max_path;
    }

    if (prepend_slash && !path_ends_with_one_of(path, "\\/")) {
        (void) strncat(path.data(), "\\", 1);
    }

    (void) strncat(path.data(), str, str_len);

    return path_append_result::success;
}

#endif // SWAN_PATH_CPP
