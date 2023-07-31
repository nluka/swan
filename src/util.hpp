#ifndef SWAN_UTIL_HPP
#define SWAN_UTIL_HPP

#include <array>
#include <chrono>

#include "primitives.hpp"

// Returns the size of a static C-style array at compile time.
template <typename ElemTy, u64 Length>
consteval u64 lengthof(ElemTy (&)[Length]) noexcept(true)
{
    return Length;
}

typedef std::chrono::high_resolution_clock::time_point time_point_t;

time_point_t current_time() noexcept(true);

void flip_bool(bool &b) noexcept(true);

u64 two_u32_to_one_u64(u32 low, u32 high) noexcept(true);

i32 directory_exists(char const *path) noexcept(true);

void format_file_size(
    u64 file_size,
    char *out,
    u64 out_size,
    u64 unit_multiplier) noexcept(true);

i64 compute_diff_ms(time_point_t start, time_point_t end) noexcept(true);

std::array<char, 64> compute_when_str(
    time_point_t start,
    time_point_t end) noexcept(true);

void seed_fast_rand(u64) noexcept(true);

u64 fast_rand(u64 min, u64 max) noexcept(true);

#endif // SWAN_UTIL_HPP
