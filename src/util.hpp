#pragma once

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

s32 directory_exists(char const *path) noexcept;

void format_file_size(
    u64 file_size,
    char *out,
    u64 out_size,
    u64 unit_multiplier) noexcept;

s64 compute_diff_ms(time_point_t start, time_point_t end) noexcept;

std::array<char, 64> compute_when_str(
    time_point_t start,
    time_point_t end) noexcept;

void seed_fast_rand(u64) noexcept;

u64 fast_rand(u64 min, u64 max) noexcept;

s32 utf8_to_utf16(char const *utf8_text, wchar_t *utf16_text, u64 utf16_text_capacity) noexcept;

s32 utf16_to_utf8(wchar_t const *utf16_text, char *utf8_text, u64 utf8_text_capacity) noexcept;

bool streq(char const *s1, char const *s2) noexcept;

bool strempty(char const *s) noexcept;

u64 remove_adjacent_spaces(char *str, u64 len = 0) noexcept;

// Has a 1 in probability_fraction chance to return true.
bool chance(f64 probability_fraction) noexcept;

char const *lorem_ipsum() noexcept;

char *get_file_name(char *path) noexcept;
char const *cget_file_name(char const *path) noexcept;

char *get_file_ext(char *path) noexcept;
// char const *cget_file_ext(char const *path) noexcept;

std::string_view get_everything_minus_file_name(char const *path) noexcept;

struct file_name_ext
{
    char *name;
    char *ext;
    char *dot;

    file_name_ext(char *path) noexcept;
    ~file_name_ext() noexcept;
};

wchar_t const *windows_illegal_filename_chars() noexcept;

void init_empty_cstr(char *s) noexcept;
void init_empty_cstr(wchar_t *s) noexcept;
