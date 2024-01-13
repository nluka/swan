#pragma once

#include "stdafx.hpp"

#if !defined(NDEBUG)
#define WCOUT_IF_DEBUG(x) std::wcout << '[' << std::source_location::current().file_name() << ':' << std::source_location::current().line() << "] " << x
#else
#define WCOUT_IF_DEBUG(x) do {} while(0)
#endif

// Returns the size of a static C-style array at compile time.
template <typename ElemTy, u64 Length>
consteval u64 lengthof(ElemTy (&)[Length]) noexcept
{
    return Length;
}

typedef std::chrono::high_resolution_clock::time_point precise_time_point_t;
typedef std::chrono::system_clock::time_point system_time_point_t;

precise_time_point_t current_time_precise() noexcept;
system_time_point_t current_time_system() noexcept;

s64 compute_diff_ms(precise_time_point_t start, precise_time_point_t end) noexcept;
s64 compute_diff_ms(system_time_point_t start, system_time_point_t end) noexcept;

s64 compute_diff_us(precise_time_point_t start, precise_time_point_t end) noexcept;
s64 compute_diff_us(system_time_point_t start, system_time_point_t end) noexcept;

std::array<char, 64> compute_when_str(precise_time_point_t start, precise_time_point_t end) noexcept;
std::array<char, 64> compute_when_str(system_time_point_t start, system_time_point_t end) noexcept;

void flip_bool(bool &b) noexcept;

u64 two_u32_to_one_u64(u32 low, u32 high) noexcept;

s32 directory_exists(char const *path) noexcept;

void format_file_size(
    u64 file_size,
    char *out,
    u64 out_size,
    u64 unit_multiplier) noexcept;

void seed_fast_rand(u64) noexcept;

u64 fast_rand(u64 min = 1, u64 max = u64(-1)) noexcept;

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

/// @brief Object to split a file path into 2 pieces: name and extension.
/// It does so by finding the appropriate [.] character and replacing it with NUL on construction.
/// `name` is a C-string pointing to the name, `dot` points to the NUL character where the [.] character was,
/// `ext` is a C-string pointing to the extension (equivalent to `dot+1`). When destructed, the [.] character is restored,
/// returning the path to its original state. If the given path does not have an extension, `ext` and `dot` are nullptr.
struct file_name_extension_splitter
{
    char *name = nullptr;
    char *ext = nullptr;
    char *dot = nullptr;

    file_name_extension_splitter() = delete;
    file_name_extension_splitter(char *path) noexcept;
    ~file_name_extension_splitter() noexcept;
};

wchar_t const *windows_illegal_filename_chars() noexcept;
wchar_t const *windows_illegal_path_chars() noexcept;

void init_empty_cstr(char *s) noexcept;
void init_empty_cstr(wchar_t *s) noexcept;

bool set_thread_priority(s32 priority_relative_to_normal) noexcept;

char *rtrim(char *szX) noexcept;

bool last_non_whitespace_is_one_of(char const *str, u64 len, char const *test_str) noexcept;

std::string make_str(char const *fmt, ...) noexcept;

template <typename Ty>
bool one_of(Ty const &test_val, std::initializer_list<Ty> const &possible_values) noexcept
{
    for (auto const &val : possible_values) {
        if (val == test_val) {
            return true;
        }
    }
    return false;
}

template <typename Ty>
Ty &inc_or_wrap(Ty &val, Ty const &min, Ty const &max) noexcept
{
    if (val == max) {
        val = min; // wrap around
    } else {
        ++val;
    }
    return val;
}
