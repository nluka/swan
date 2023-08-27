#ifndef SWAN_UTIL_HPP
#define SWAN_UTIL_HPP

#include <array>
#include <chrono>

#include "primitives.hpp"

// Returns the size of a static C-style array at compile time.
template <typename ElemTy, u64 Length>
consteval u64 lengthof(ElemTy (&)[Length]) noexcept
{
    return Length;
}

typedef std::chrono::high_resolution_clock::time_point time_point_t;

time_point_t current_time() noexcept;

void flip_bool(bool &b) noexcept;

u64 two_u32_to_one_u64(u32 low, u32 high) noexcept;

i32 directory_exists(char const *path) noexcept;

void format_file_size(
    u64 file_size,
    char *out,
    u64 out_size,
    u64 unit_multiplier) noexcept;

i64 compute_diff_ms(time_point_t start, time_point_t end) noexcept;

std::array<char, 64> compute_when_str(
    time_point_t start,
    time_point_t end) noexcept;

void seed_fast_rand(u64) noexcept;

u64 fast_rand(u64 min, u64 max) noexcept;

i32 utf8_to_utf16(char const *utf8_text, wchar_t *utf16_text, u64 utf16_text_capacity) noexcept;

i32 utf16_to_utf8(wchar_t const *utf16_text, char *utf8_text, u64 utf8_text_capacity) noexcept;

bool streq(char const *s1, char const *s2) noexcept;

u64 remove_adjacent_spaces(char *str, u64 len = 0) noexcept;

#endif // SWAN_UTIL_HPP
