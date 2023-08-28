#ifndef SWAN_UTIL_CPP
#define SWAN_UTIL_CPP

#include <iostream>
#include <cassert>
#include <cstring>

#include <windows.h>

#include "util.hpp"

static u64 s_fast_rand_seed = {};
void seed_fast_rand(u64 v) noexcept
{
    s_fast_rand_seed = v;
}

u64 fast_rand(u64 min, u64 max) noexcept
{
    s_fast_rand_seed ^= (s_fast_rand_seed << 21);
    s_fast_rand_seed ^= (s_fast_rand_seed >> 35);
    s_fast_rand_seed ^= (s_fast_rand_seed << 4);

    u64 range = static_cast<uint64_t>(max - min) + 1;
    u64 rand_num = s_fast_rand_seed % range;

    return rand_num;
}

bool chance(f64 probability_fraction) noexcept
{
    u64 max_value = static_cast<u64>(1.0 / probability_fraction);
    u64 random_number = fast_rand(0, max_value);
    return random_number == 0;
}

void flip_bool(bool &b) noexcept
{
    b ^= true;
}

u64 two_u32_to_one_u64(u32 low, u32 high) noexcept
{
    u64 result = {};
    result = static_cast<u64>(high) << 32;
    result |= static_cast<u64>(low);
    return result;
}

i32 directory_exists(char const *path) noexcept
{
    DWORD attributes = GetFileAttributesA(path);
    return (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY));
}

void format_file_size(
    u64 file_size,
    char *out,
    u64 out_size,
    u64 unit_multiplier) noexcept
{
    char const *units[] = { "B", "KB", "MB", "GB", "TB" };
    u64 constexpr largest_unit_idx = (sizeof(units) / sizeof(*units)) - 1;
    u64 unit_idx = 0;

    f64 size = static_cast<double>(file_size);

    while (size >= 1024 && unit_idx < largest_unit_idx) {
        size /= unit_multiplier;
        ++unit_idx;
    }

    char const *const fmt =
        unit_idx == 0
        // no digits after decimal point for bytes
        // because showing a fraction of a byte doesn't make sense
        ? "%.0lf %s"
        // 2 digits after decimal points for units above Bytes
        // greater than bytes
        : "%.2lf %s";

    snprintf(out, out_size, fmt, size, units[unit_idx]);
}

i64 compute_diff_ms(time_point_t start, time_point_t end) noexcept
{
    auto start_ms = std::chrono::time_point_cast<std::chrono::milliseconds>(start);
    auto end_ms = std::chrono::time_point_cast<std::chrono::milliseconds>(end);
    auto diff_ms = end_ms - start_ms;
    return diff_ms.count();
}

time_point_t current_time() noexcept
{
    return std::chrono::high_resolution_clock::now();
}

std::array<char, 64> compute_when_str(time_point_t start, time_point_t end) noexcept
{
    std::array<char, 64> out = {};

    i64 ms_diff = compute_diff_ms(start, end);
    i64 one_minute = 60'000;
    i64 one_hour = one_minute * 60;
    i64 one_day = one_hour * 24;

    if (ms_diff < one_minute) {
        strncpy(out.data(), "just now", out.size());
    }
    else if (ms_diff < one_hour) {
        u64 minutes = u64(ms_diff / one_minute);
        snprintf(out.data(), out.size(), "%zu min%s ago", minutes, minutes == 1 ? "" : "s");
    }
    else if (ms_diff < one_day) {
        u64 hours = u64(ms_diff / one_hour);
        snprintf(out.data(), out.size(), "%zu hour%s ago", hours, hours == 1 ? "" : "s");
    }
    else {
        u64 days = u64(ms_diff / one_day);
        snprintf(out.data(), out.size(), "%zu day%s ago", days, days == 1 ? "" : "s");
    }

    return out;
}

i32 utf8_to_utf16(char const *utf8_text, wchar_t *utf16_text, u64 utf16_text_capacity) noexcept
{
    i32 chars_written = MultiByteToWideChar(CP_UTF8, 0, utf8_text, -1, utf16_text, (i32)utf16_text_capacity);

    return chars_written;
}

i32 utf16_to_utf8(wchar_t const *utf16_text, char *utf8_text, u64 utf8_text_capacity) noexcept
{
    i32 chars_written = WideCharToMultiByte(CP_UTF8, 0, utf16_text, -1, utf8_text, (i32)utf8_text_capacity, "!", nullptr);

    return chars_written;
}

bool streq(char const *s1, char const *s2) noexcept
{
    return strcmp(s1, s2) == 0;
}

u64 remove_adjacent_spaces(char *str, u64 len) noexcept
{
    assert(str != nullptr);

    if (len == 0) {
        len = strlen(str);
    }

    bool in_space = false;
    u64 write_index = 0;
    u64 spaces_removed = 0;

    for (u64 i = 0; i < len; i++) {
        if (str[i] == ' ') {
            if (!in_space) {
                str[write_index++] = ' ';
                in_space = true;
            } else {
                spaces_removed++;
            }
        } else {
            str[write_index++] = str[i];
            in_space = false;
        }
    }

    if (write_index > 0 && str[write_index - 1] == ' ') {
        write_index--; // Remove trailing space, if any
    }

    str[write_index] = '\0'; // Null-terminate the modified string

    return spaces_removed;
}

#endif // SWAN_UTIL_CPP
