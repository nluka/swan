#pragma once

#include "stdafx.hpp"

#if defined(NDEBUG)
#   define DEBUG_MODE 0
#   define RELEASE_MODE 1
#else
#   define DEBUG_MODE 1
#   define RELEASE_MODE 0
#endif

#if DEBUG_MODE
#define WCOUT_IF_DEBUG(x) std::wcout << '[' << std::source_location::current().file_name() << ':' << std::source_location::current().line() << "] " << x
#else
#define WCOUT_IF_DEBUG(x) do {} while(0)
#endif

// TIME RELATED TYPES AND FUNCTIONS

    typedef std::chrono::high_resolution_clock::time_point time_point_precise_t;
    typedef std::chrono::system_clock::time_point time_point_system_t;

    time_point_precise_t get_time_precise() noexcept;
    time_point_system_t get_time_system() noexcept;
    time_point_system_t extract_system_time_from_istream(std::istream &in_stream) noexcept;
    s64 time_diff_ms(time_point_precise_t start, time_point_precise_t end) noexcept;
    s64 time_diff_ms(time_point_system_t start, time_point_system_t end) noexcept;
    s64 time_diff_us(time_point_precise_t start, time_point_precise_t end) noexcept;
    s64 time_diff_us(time_point_system_t start, time_point_system_t end) noexcept;
    std::array<char, 64> time_diff_str(time_point_precise_t start, time_point_precise_t end) noexcept;
    std::array<char, 64> time_diff_str(time_point_system_t start, time_point_system_t end) noexcept;

/// MISCELLANEOUS FUNCTIONS AND TYPES

    /// Toggle bool state.
    void flip_bool(bool &b) noexcept;

    /// Combine 2 `u32` values into a single `u64` via bitshifting.
    u64 two_u32_to_one_u64(u32 low, u32 high) noexcept;

    /// Seed the `fast_rand` function.
    void seed_fast_rand(u64) noexcept;

    /// Get a random unsigned integer in range [min, max]. Seed generator with `seed_fast_rand` function.
    u64 fast_rand(u64 min = 1, u64 max = u64(-1)) noexcept;

    /// Returns about 2000 chars of lorem ipsum text.
    char const *lorem_ipsum() noexcept;

    /// Return `true` at a chance of (1 / probability_fraction).
    bool chance(f64 probability_fraction) noexcept;

    bool set_thread_priority(s32 priority_relative_to_normal) noexcept;

    s32 utf8_to_utf16(char const *utf8_text, wchar_t *utf16_text, u64 utf16_text_capacity, std::source_location sloc = std::source_location::current()) noexcept;

    s32 utf16_to_utf8(wchar_t const *utf16_text, char *utf8_text, u64 utf8_text_capacity, std::source_location sloc = std::source_location::current()) noexcept;

    std::pair<bool, std::string> utf8_lowercase(char const *utf8_text) noexcept;

    struct build_mode
    {
        bool debug;
        bool release;
        char const *str;
    };

    build_mode get_build_mode() noexcept;

    std::string make_str(char const *fmt, ...) noexcept;

    std::optional<bool> win32_is_mouse_inside_window(HWND hwnd) noexcept;

// FILESYSTEM RELATED FUNCTIONS

    /// Returns truthy int if `path_utf8` is a valid directory. Accepts Unicode characters. Performs UTF8 to UTF16 conversion. Expensive function, don't call it often.
    s32 directory_exists(char const *path_utf8) noexcept;

    /// Formats a FILETIME into a buffer. Returns length in `.first` and the formatted buffer in `.second`.
    std::pair<s32, std::array<char, 64>> filetime_to_string(FILETIME *time) noexcept;
    std::array<char, 32> format_file_size(u64 file_size, u64 unit_multiplier) noexcept;
    void format_file_size(u64 file_size, char *out, u64 out_size, u64 unit_multiplier) noexcept;
    wchar_t const *windows_illegal_filename_chars() noexcept;
    wchar_t const *windows_illegal_path_chars() noexcept;

// C-STRING FUNCTIONS

    bool cstr_eq(char const *s1, char const *s2) noexcept;
    bool cstr_empty(char const *s) noexcept;
    bool cstr_empty(wchar_t const *s) noexcept;
    bool cstr_starts_with(char const *str, char const *prefix) noexcept;
    bool cstr_ends_with(char const *str, char const *suffix) noexcept;
    u64 cstr_erase_adjacent_spaces(char *str, u64 len = 0) noexcept;
    void cstr_fill(wchar_t *s, wchar_t fill_ch) noexcept;
    void cstr_clear(char *s) noexcept;
    void cstr_clear(wchar_t *s) noexcept;
    char const *cstr_ltrim(char const *s, std::initializer_list<char> const &chars) noexcept;
    char *cstr_rtrim(char *s) noexcept;
    bool cstr_last_non_whitespace_is_one_of(char const *str, u64 len, char const *test_str) noexcept;

// PATH FUNCTIONS AND TYPES

    char *              path_find_filename   (char          *path) noexcept;
    wchar_t *           path_find_filename   (wchar_t       *path) noexcept;
    char *              path_find_file_ext   (char          *path) noexcept;
    char const *        path_cfind_filename  (char    const *path) noexcept;
    wchar_t const *     path_cfind_filename  (wchar_t const *path) noexcept;
    char const *        path_cfind_file_ext  (char    const *path) noexcept;
    std::string_view    path_extract_location(char    const *path) noexcept;
    bool                path_drive_like      (char    const *path, u64 len = 0) noexcept; // len optional

    /// @brief Object to split a file path into 2 pieces: name and extension.
    /// It does so by finding the appropriate [.] character and replacing it with NUL on construction.
    /// `name` is a C-string pointing to the name, `dot` points to the NUL character where the [.] character was,
    /// `ext` is a C-string pointing to the extension (equivalent to `dot+1`). When destructed, the [.] character is restored,
    /// returning the path to its original state. If the given path does not have an extension, `ext` and `dot` are nullptr.
    struct temp_filename_extension_splitter
    {
        char *name = nullptr;
        char *ext = nullptr;
        char *dot = nullptr;

        temp_filename_extension_splitter() = delete;
        temp_filename_extension_splitter(char *path) noexcept;
        ~temp_filename_extension_splitter() noexcept;
    };

// FUNCTION TEMPLATES

    // Returns the size of a static C-style array at compile time.
    template <typename ElemTy, u64 Length>
    consteval u64 lengthof(ElemTy (&)[Length]) noexcept
    {
        return Length;
    }

    /// Creates a formatted buffer with a maximum length of `Size`.
    template <u64 Size>
    std::array<char, Size> make_str_static(char const *fmt, ...) noexcept
    {
        std::array<char, Size> retval;

        va_list args;
        va_start(args, fmt);
        [[maybe_unused]] s32 cnt = vsnprintf(retval.data(), retval.max_size(), fmt, args);
        // assert(cnt > 0);
        assert(cnt < retval.max_size());
        va_end(args);

        return retval;
    }

    /// Returns `if_single` when `num == 1`, `if_zero_or_multiple` otherwise.
    template <typename Ty>
    char const *pluralized(Ty num, char const *if_single, char const *if_zero_or_multiple) noexcept
    {
        return num == 1 ? if_single : if_zero_or_multiple;
    }

    /// Returns true if `test_val` in `possible_values`.
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

    /// Increments `val`, or wraps it back to `min` if increment would exceed `max`.
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

    /// Decrements `val`, or wraps it back to `max` if increment would subceed `min`.
    template <typename Ty>
    Ty &dec_or_wrap(Ty &val, Ty const &min, Ty const &max) noexcept
    {
        if (val == min) {
            val = max; // wrap around
        } else {
            --val;
        }
        return val;
    }

    template <typename Ty>
    void bit_set(Ty &val, u64 bit_pos) noexcept
    {
        val = val | (u64(1) << bit_pos);
    }

    template <typename Ty>
    void bit_clear(Ty &val, u64 bit_pos) noexcept
    {
        val = val & ~(u64(1) << bit_pos);
    }

    template <typename IntTy>
    constexpr
    u8 count_digits(IntTy n) noexcept
    {
        // who knows, maybe there will be 128 or 256 bit integers in the future...
        static_assert(sizeof(IntTy) <= 8);

        if (n == 0)
            return 1;

        u8 count = 0;
        while (n != 0) {
            n = n / 10;
            ++count;
        }

        return count;
    }
