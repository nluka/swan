#pragma once

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

s32 directory_exists(char const *path) noexcept
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

s64 compute_diff_ms(time_point_t start, time_point_t end) noexcept
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

    s64 ms_diff = compute_diff_ms(start, end);
    s64 one_minute = 60'000;
    s64 one_hour = one_minute * 60;
    s64 one_day = one_hour * 24;

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

s32 utf8_to_utf16(char const *utf8_text, wchar_t *utf16_text, u64 utf16_text_capacity) noexcept
{
    s32 chars_written = MultiByteToWideChar(CP_UTF8, 0, utf8_text, -1, utf16_text, (s32)utf16_text_capacity);

    return chars_written;
}

s32 utf16_to_utf8(wchar_t const *utf16_text, char *utf8_text, u64 utf8_text_capacity) noexcept
{
    s32 chars_written = WideCharToMultiByte(CP_UTF8, 0, utf16_text, -1, utf8_text, (s32)utf8_text_capacity, "!", nullptr);

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

char const *lorem_ipsum() noexcept
{
    char const *data =
        "Lorem ipsum dolor sit amet, consectetur adipiscing elit. "
        "Ut quis elit ut lorem pharetra hendrerit vel ut lacus. "
        "Integer congue velit ut ipsum hendrerit, quis auctor elit aliquam. "
        "Phasellus nec venenatis nulla. "
        "Suspendisse nunc elit, pharetra ac suscipit tincidunt, volutpat et mauris. "
        "Curabitur elementum pulvinar vestibulum. "
        "Curabitur odio turpis, molestie quis scelerisque nec, pharetra at turpis. "
        "Mauris dignissim velit sit amet erat aliquam luctus. "
        "Sed eu augue eu ex ullamcorper posuere a vel arcu. "
        "Vivamus at ligula urna. "
        "Praesent ac quam urna. "
        "Class aptent taciti sociosqu ad litora torquent per conubia nostra, per inceptos himenaeos. "
        "Donec euismod consectetur sapien, quis porta mi sagittis ac. "
        "Nullam cursus rutrum erat, sed facilisis eros interdum eget. "
        "Proin vitae tincidunt velit. "
        "Morbi consectetur lacus dolor, quis congue dui consequat euismod. "
        "Proin in gravida diam. "
        "Fusce sit amet euismod urna. "
        "Quisque quis orci sit amet mi facilisis egestas vel vitae nunc. "
        "Aliquam tincidunt mauris vitae tincidunt porttitor. "
        "Nunc tincidunt, nibh sed varius ornare, lectus erat ornare sem, non semper magna odio sit amet erat. "
        "Praesent imperdiet justo neque, eu malesuada diam fringilla quis. "
        "Vestibulum eget iaculis est. "
        "Nam ultricies turpis at risus vulputate, congue eleifend risus tincidunt. "
        "Nulla scelerisque dui tortor, et mollis ex egestas at. "
        "Duis quis ante non nulla ultrices rutrum. "
        "Class aptent taciti sociosqu ad litora torquent per conubia nostra, per inceptos himenaeos. "
        "Quisque in justo luctus, sagittis magna a, ullamcorper diam. "
        "Integer efficitur vestibulum lacus sed vulputate. "
        "Maecenas justo lorem, convallis nec tempor vitae, ullamcorper et ligula. "
        "Morbi finibus nulla sed risus posuere, non pharetra lorem pellentesque. "
        "Nam posuere odio eu orci euismod scelerisque. "
        "Mauris commodo lorem a finibus volutpat. ";

    return data;

    // std::array<char, 2048> retval;
    // static_assert(lengthof(data) <= retval.size());
    // std::memcpy(retval.data(), data, lengthof(data));
    // return retval;
}

file_name_ext::file_name_ext(char *path) noexcept
{
    this->name = get_file_name(path);
    this->ext = get_file_ext(name);
    this->dot = ext ? ext - 1 : nullptr;
    if (this->dot) {
        *this->dot = '\0';
    }
}

file_name_ext::~file_name_ext() noexcept
{
    if (this->dot) {
        *this->dot = '.';
    }
}

char *get_file_name(char *path) noexcept
{
    // C:\code\swan\src\explorer_window.cpp
    //                  ^^^^^^^^^^^^^^^^^^^ what we are after
    // src/swan.cpp
    //     ^^^^^^^^ what we are after

    assert(path != nullptr);

    char *just_the_file_name = path;

    std::string_view view(just_the_file_name);

    u64 last_sep_pos = view.find_last_of("\\/");

    if (last_sep_pos != std::string::npos) {
        just_the_file_name += last_sep_pos + 1;
    }

    return just_the_file_name;
}

char const *cget_file_name(char const *path) noexcept
{
    // C:\code\swan\src\explorer_window.cpp
    //                  ^^^^^^^^^^^^^^^^^^^ what we are after
    // src/swan.cpp
    //     ^^^^^^^^ what we are after

    assert(path != nullptr);

    char const *just_the_file_name = path;

    std::string_view view(just_the_file_name);

    u64 last_sep_pos = view.find_last_of("\\/");

    if (last_sep_pos != std::string::npos) {
        just_the_file_name += last_sep_pos + 1;
    }

    return just_the_file_name;
}

char *get_file_ext(char *path) noexcept
{
    // C:\code\swan\src\explorer_window.cpp
    //                                  ^^^ what we are after
    // src/swan.cpp
    //          ^^^ what we are after

    assert(path != nullptr);

    char *just_the_file_ext = path;

    std::string_view view(just_the_file_ext);

    u64 last_dot_pos = view.find_last_of(".");

    if (last_dot_pos != std::string::npos) {
        just_the_file_ext += last_dot_pos + 1;
        return just_the_file_ext;
    } else {
        return nullptr;
    }
}

std::string_view get_everything_minus_file_name(char const *path) noexcept
{
    // C:\code\swan\src\explorer_window.cpp
    // ^^^^^^^^^^^^^^^^^ what we are after
    // src/swan.cpp
    // ^^^^ what we are after

    assert(path != nullptr);

    std::string_view full_path_view(path);

    u64 last_sep_pos = full_path_view.find_last_of("\\/");

    if (last_sep_pos == std::string::npos) {
        return path;
    }
    else {
        return std::string_view(path, last_sep_pos + 1);
    }
}
