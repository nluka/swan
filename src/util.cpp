#ifndef SWAN_UTIL_CPP
#define SWAN_UTIL_CPP

#include <iostream>
#include <cassert>
#include <cstring>

#include <windows.h>

#include "util.hpp"

static u64 s_fast_rand_seed = {};
void seed_fast_rand(u64 v) noexcept(true)
{
    s_fast_rand_seed = v;
}

u64 fast_rand(u64 min, u64 max) noexcept(true)
{
    s_fast_rand_seed ^= (s_fast_rand_seed << 21);
    s_fast_rand_seed ^= (s_fast_rand_seed >> 35);
    s_fast_rand_seed ^= (s_fast_rand_seed << 4);

    u64 range = static_cast<uint64_t>(max - min) + 1;
    u64 rand_num = s_fast_rand_seed % range;

    return rand_num;
}

void flip_bool(bool &b) noexcept(true)
{
    b ^= true;
}

u64 two_u32_to_one_u64(u32 low, u32 high) noexcept(true)
{
    u64 result = {};
    result = static_cast<u64>(high) << 32;
    result |= static_cast<u64>(low);
    return result;
}

i32 directory_exists(char const *path) noexcept(true)
{
    DWORD attributes = GetFileAttributesA(path);
    return (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY));
}

void format_file_size(
    u64 file_size,
    char *out,
    u64 out_size,
    u64 unit_multiplier) noexcept(true)
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

i64 compute_diff_ms(time_point_t start, time_point_t end) noexcept(true)
{
    auto start_ms = std::chrono::time_point_cast<std::chrono::milliseconds>(start);
    auto end_ms = std::chrono::time_point_cast<std::chrono::milliseconds>(end);
    auto diff_ms = end_ms - start_ms;
    return diff_ms.count();
}

time_point_t current_time() noexcept(true)
{
    return std::chrono::high_resolution_clock::now();
}

std::array<char, 64> compute_when_str(time_point_t start, time_point_t end) noexcept(true)
{
    std::array<char, 64> out = {};

    i64 ms_diff = compute_diff_ms(start, end);
    // i64 ten_seconds = 10'000;
    // i64 thirty_seconds = 30'000;
    i64 one_minute = 60'000;
    i64 one_hour = one_minute * 60;
    i64 one_day = one_hour * 24;

    // if (ms_diff < ten_seconds) {
    //     strncpy(out.data(), "just now", out.size());
    // }
    // else if (ms_diff < thirty_seconds) {
    //      strncpy(out.data(), "< 30 sec ago", out.size());
    // }
    // else if (ms_diff < one_minute) {
    //     strncpy(out.data(), "< 1 minute ago", out.size());
    // }
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

#endif // SWAN_UTIL_CPP
