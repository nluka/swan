#pragma once

#include "stdafx.hpp"
#include "util.hpp"
#include "imgui_dependent_functions.hpp"

static u64 g_fast_rand_seed = {};
void seed_fast_rand(u64 v) noexcept
{
    g_fast_rand_seed = v;
}

u64 fast_rand(u64 min, u64 max) noexcept
{
    g_fast_rand_seed ^= (g_fast_rand_seed << 21);
    g_fast_rand_seed ^= (g_fast_rand_seed >> 35);
    g_fast_rand_seed ^= (g_fast_rand_seed << 4);

    u64 range = static_cast<uint64_t>(max - min) + 1;
    u64 rand_num = g_fast_rand_seed % range;

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

s32 directory_exists(char const *path_utf8) noexcept
{
    wchar_t path_utf16[MAX_PATH];
    if (!utf8_to_utf16(path_utf8, path_utf16, lengthof(path_utf16))) {
        return false;
    }
    DWORD attributes = GetFileAttributesW(path_utf16);
    return (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY));
}

std::array<char, 32> format_file_size(u64 file_size, u64 unit_multiplier) noexcept
{
    std::array<char, 32> retval;
    format_file_size(file_size, retval.data(), retval.max_size(), unit_multiplier);
    return retval;
}

void format_file_size(u64 file_size, char *out, u64 out_size, u64 unit_multiplier) noexcept
{
    assert(unit_multiplier == 1000 || unit_multiplier == 1024);

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

s64 time_diff_ms(time_point_precise_t start, time_point_precise_t end) noexcept
{
    auto start_ms = std::chrono::time_point_cast<std::chrono::milliseconds>(start);
    auto end_ms = std::chrono::time_point_cast<std::chrono::milliseconds>(end);
    auto diff_ms = end_ms - start_ms;
    return diff_ms.count();
}

s64 time_diff_us(time_point_precise_t start, time_point_precise_t end) noexcept
{
    auto start_ms = std::chrono::time_point_cast<std::chrono::microseconds>(start);
    auto end_ms = std::chrono::time_point_cast<std::chrono::microseconds>(end);
    auto diff_us = end_ms - start_ms;
    return diff_us.count();
}

time_point_precise_t get_time_precise() noexcept
{
    return std::chrono::high_resolution_clock::now();
}

time_point_system_t get_time_system() noexcept
{
    return std::chrono::system_clock::now();
}

s64 time_diff_ms(time_point_system_t start, time_point_system_t end) noexcept
{
    auto start_ms = std::chrono::time_point_cast<std::chrono::milliseconds>(start);
    auto end_ms = std::chrono::time_point_cast<std::chrono::milliseconds>(end);
    auto diff_ms = end_ms - start_ms;
    return diff_ms.count();
}

s64 time_diff_us(time_point_system_t start, time_point_system_t end) noexcept
{
    auto start_ms = std::chrono::time_point_cast<std::chrono::microseconds>(start);
    auto end_ms = std::chrono::time_point_cast<std::chrono::microseconds>(end);
    auto diff_us = end_ms - start_ms;
    return diff_us.count();
}

void time_diff_str_impl(std::array<char, 64> &out, s64 ms_diff) noexcept
{
    s64 const one_second = 1'000;
    s64 const one_minute = one_second * 60;
    s64 const one_hour = one_minute * 60;
    s64 const one_day = one_hour * 24;

    bool in_future = ms_diff < 0;
    char const *tense = in_future ? "in future" : "ago";
    ms_diff = abs(ms_diff);

    if (ms_diff < one_minute) {
    #if 1
        u64 seconds = u64(ms_diff / one_second);
        snprintf(out.data(), out.size(), "%zus %s", seconds, tense);
    #else
        if (in_future) {
            u64 seconds = u64(ms_diff / one_second);
            snprintf(out.data(), out.size(), "%zus in future", seconds);
        } else {
            snprintf(out.data(), out.size(), "now");
        }
    #endif
    }
    else if (ms_diff < one_hour) {
        u64 minutes = u64(ms_diff / one_minute);
        snprintf(out.data(), out.size(), "%zum %s", minutes, tense);
    }
    else if (ms_diff < one_day) {
        u64 hours = u64(ms_diff / one_hour);
        snprintf(out.data(), out.size(), "%zuh %s", hours, tense);
    }
    else {
        u64 days = u64(ms_diff / one_day);
        snprintf(out.data(), out.size(), "%zud %s", days, tense);
    }
}

std::array<char, 64> time_diff_str(time_point_precise_t start, time_point_precise_t end) noexcept
{
    std::array<char, 64> out = {};
    s64 ms_diff = time_diff_ms(start, end);
    time_diff_str_impl(out, ms_diff);
    return out;
}

std::array<char, 64> time_diff_str(time_point_system_t start, time_point_system_t end) noexcept
{
    std::array<char, 64> out = {};
    s64 ms_diff = time_diff_ms(start, end);
    time_diff_str_impl(out, ms_diff);
    return out;
}

s32 utf8_to_utf16(char const *utf8_text, wchar_t *utf16_text, u64 utf16_text_capacity, std::source_location sloc) noexcept
{
    assert(utf8_text != nullptr);
    assert(utf16_text != nullptr);
    assert(utf16_text_capacity > 0);

    s32 chars_written = MultiByteToWideChar(CP_UTF8, 0, utf8_text, -1, utf16_text, (s32)utf16_text_capacity);

    if (chars_written == 0) {
        auto last_error = get_last_winapi_error();
        print_debug_msg({ "FAILED utf8_to_utf16: %d %s", sloc }, last_error.code, last_error.formatted_message.c_str());
    }

    return chars_written;
}

s32 utf16_to_utf8(wchar_t const *utf16_text, char *utf8_text, u64 utf8_text_capacity, std::source_location sloc) noexcept
{
    assert(utf16_text != nullptr);
    assert(utf8_text != nullptr);
    assert(utf8_text_capacity > 0);

    s32 chars_written = WideCharToMultiByte(CP_UTF8, 0, utf16_text, -1, utf8_text, (s32)utf8_text_capacity, "!", nullptr);

    if (chars_written == 0) {
        auto last_error = get_last_winapi_error();
        print_debug_msg({ "FAILED utf16_to_utf8: %d %s", sloc }, last_error.code, last_error.formatted_message.c_str());
    }

    return chars_written;
}

bool cstr_eq(char const *s1, char const *s2) noexcept
{
    return strcmp(s1, s2) == 0;
}

u64 cstr_erase_adjacent_spaces(char *str, u64 len) noexcept
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
}

temp_filename_extension_splitter::temp_filename_extension_splitter(char *path) noexcept
{
    this->name = path_find_filename(path);
    this->ext = path_find_file_ext(name);
    this->dot = ext ? ext - 1 : nullptr;
    if (this->dot) {
        *this->dot = '\0';
    }
}

temp_filename_extension_splitter::~temp_filename_extension_splitter() noexcept
{
    if (this->dot) {
        *this->dot = '.';
    }
}

char *path_find_filename(char *path) noexcept
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

wchar_t *path_find_filename(wchar_t *path) noexcept
{
    // C:\code\swan\src\explorer_window.cpp
    //                  ^^^^^^^^^^^^^^^^^^^ what we are after
    // src/swan.cpp
    //     ^^^^^^^^ what we are after

    assert(path != nullptr);

    wchar_t *just_the_file_name = path;

    std::wstring_view view(just_the_file_name);

    u64 last_sep_pos = view.find_last_of(L"\\/");

    if (last_sep_pos != std::string::npos) {
        just_the_file_name += last_sep_pos + 1;
    }

    return just_the_file_name;
}

wchar_t const *path_cfind_filename(wchar_t const *path) noexcept
{
    // C:\code\swan\src\explorer_window.cpp
    //                  ^^^^^^^^^^^^^^^^^^^ what we are after
    // src/swan.cpp
    //     ^^^^^^^^ what we are after

    assert(path != nullptr);

    wchar_t const *just_the_file_name = path;

    std::wstring_view view(just_the_file_name);

    u64 last_sep_pos = view.find_last_of(L"\\/");

    if (last_sep_pos != std::string::npos) {
        just_the_file_name += last_sep_pos + 1;
    }

    return just_the_file_name;
}

char const *path_cfind_filename(char const *path) noexcept
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

char *path_find_file_ext(char *path) noexcept
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

char const *path_cfind_file_ext(char const *path) noexcept
{
    // C:\code\swan\src\explorer_window.cpp
    //                                  ^^^ what we are after
    // src/swan.cpp
    //          ^^^ what we are after

    assert(path != nullptr);

    char const *just_the_file_ext = path;

    std::string_view view(just_the_file_ext);

    u64 last_dot_pos = view.find_last_of(".");

    if (last_dot_pos != std::string::npos) {
        just_the_file_ext += last_dot_pos + 1;
        return just_the_file_ext;
    } else {
        return nullptr;
    }
}

std::string_view path_extract_location(char const *path) noexcept
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

bool cstr_empty(char const *s) noexcept
{
    assert(s != nullptr);
    return s[0] == '\0';
}

bool cstr_empty(wchar_t const *s) noexcept
{
    assert(s != nullptr);
    return s[0] == L'\0';
}

wchar_t const *windows_illegal_filename_chars() noexcept
{
    return L"\\/<>\"|?*";
}

wchar_t const *windows_illegal_path_chars() noexcept
{
    return L"<>\"|?*";
}

void cstr_clear(char *s) noexcept
{
    assert(s != nullptr);
    s[0] = '\0';
}

void cstr_clear(wchar_t *s) noexcept
{
    assert(s != nullptr);
    s[0] = L'\0';
}

bool set_thread_priority(s32 priority_relative_to_normal) noexcept
{
    HANDLE handle = GetCurrentThread();
    auto result = SetThreadPriority(handle, priority_relative_to_normal);
    return result;
}

char const *cstr_ltrim(char const *s, std::initializer_list<char> const &chars) noexcept
{
    char const *retval = s;
    while (one_of(*retval, chars)) {
        ++retval;
    }
    return retval;
}

char *cstr_rtrim(char *szX) noexcept
{
    auto i = (s64)strlen(szX);
    while(szX[--i] == ' ') {
        szX[i] = '\0';
    }
    return szX;
}

bool cstr_last_non_whitespace_is_one_of(char const *str, u64 len, char const *test_str) noexcept
{
    if (str == NULL || test_str == NULL || len == 0) {
        // Handle invalid input
        return false;
    }

    s64 last_index = len - 1;

    // Find the index of the last non-whitespace character
    while (last_index >= 0 && isspace(str[last_index])) {
        last_index--;
    }

    // Check if the last non-whitespace character is one of the characters in test_str
    if (last_index >= 0) {
        const char last_char = str[last_index];

        while (*test_str != '\0') {
            if (*test_str == last_char) {
                return true;
            }
            test_str++;
        }
    }

    return false;
}

std::string make_str(char const *fmt, ...) noexcept
{
    s32 const buf_len = 1024;
    static char s_buffer[buf_len];

    va_list args;
    va_start(args, fmt);
    [[maybe_unused]] s32 cnt = vsnprintf(s_buffer, sizeof(s_buffer), fmt, args);
    assert(cnt > 0);
    assert(cnt < buf_len);
    va_end(args);

    return std::string(s_buffer);
}

build_mode get_build_mode() noexcept
{
    build_mode retval = {};

#if DEBUG_MODE
    retval.debug = true;
    retval.release = false;
    retval.str = "Debug";
#else
    retval.debug = false;
    retval.release = true;
    retval.str = "Release";
#endif

    return retval;
}

std::pair<s32, std::array<char, 64>> filetime_to_string(FILETIME *time) noexcept
{
    std::array<char, 64> buffer_raw;
    std::array<char, 64> buffer_final;

    DWORD flags = FDTF_SHORTDATE | FDTF_SHORTTIME;
    s32 length = SHFormatDateTimeA(time, &flags, buffer_raw.data(), (u32)buffer_raw.size());

    // for some reason, SHFormatDateTimeA will pad parts of the string with ASCII 63 (?)
    // when using LONGDATE or LONGTIME, we are discarding them
    std::copy_if(buffer_raw.begin(), buffer_raw.end(), buffer_final.begin(), [](char ch) noexcept { return ch != '?'; });

    // std::replace(buffer_final.begin(), buffer_final.end(), '-', ' ');

    return { length, buffer_final };
}

bool cstr_starts_with(char const *str, char const *prefix) noexcept
{
    assert(str != nullptr);
    assert(prefix != nullptr);

    return strncmp(prefix, str, strlen(prefix)) == 0;
}

time_point_system_t extract_system_time_from_istream(std::istream &in_stream) noexcept
{
    std::time_t time;
    in_stream >> time;
    time_point_system_t time_point = std::chrono::system_clock::from_time_t(time);
    return time_point;
}

// from https://stackoverflow.com/a/744822
bool cstr_ends_with(const char *str, const char *suffix) noexcept
{
    if (!str || !suffix)
        return 0;
    size_t lenstr = strlen(str);
    size_t lensuffix = strlen(suffix);
    if (lensuffix >  lenstr)
        return 0;
    return strncmp(str + lenstr - lensuffix, suffix, lensuffix) == 0;
}

void cstr_fill(wchar_t *s, wchar_t fill_ch) noexcept
{
    assert(s != nullptr);

    while (*s != L'\0') {
        *s = fill_ch;
        ++s;
    }
}

std::optional<bool> win32_is_mouse_inside_window(HWND hwnd) noexcept
{
    POINT cp;
    if (!GetCursorPos(&cp)) {
        print_debug_msg("FAILED GetCursorPos: %s", get_last_winapi_error().formatted_message.c_str());
        return std::nullopt;
    }

    RECT wr;
    if (!GetWindowRect(hwnd, &wr)) {
        print_debug_msg("FAILED GetWindowRect: %s", get_last_winapi_error().formatted_message.c_str());
        return std::nullopt;
    }

    bool result = cp.x >= wr.left && cp.x <= wr.right && cp.y >= wr.top && cp.y <= wr.bottom;

    if (result) {
        // Get the window handle at the cursor position
        HWND hwnd_under_cursor = WindowFromPoint(cp);

        // If the window under the cursor is not the target window, the cursor is covered
        if (hwnd_under_cursor != hwnd) {
            // Check if the window is a child window of hwnd
            HWND parent = hwnd;
            while (parent != NULL) {
                if (hwnd_under_cursor == parent) {
                    return false; // Covered by a child window
                }
                parent = GetParent(parent);
            }
            // If the window is not a child and it covers the target window
            return false;
        }
    }

// #if DEBUG_MODE
//     print_debug_msg("%s " ICON_FA_MOUSE_POINTER " (%d, %d) " ICON_CI_SCREEN_FULL " (%d, %d), (%d, %d)",
//         (result ? ICON_CI_PASS_FILLED : ICON_CI_CIRCLE_LARGE_FILLED), cp.x, cp.y, wr.left, wr.top, wr.right, wr.bottom);
// #endif

    return result;
}

bool path_drive_like(char const *path, u64 len) noexcept
{
    if (len == 0) len = strlen(path);
    return (len == 2 && IsCharAlphaA(path[0]) && path[1] == ':')
        || (len == 3 && IsCharAlphaA(path[0]) && path[1] == ':' && strchr("\\/", path[2]));
}

std::pair<bool, std::string> utf8_lowercase(char const *utf8_text) noexcept
{
    // Step 1: Convert UTF-8 to UTF-16
    int wchar_cnt = MultiByteToWideChar(CP_UTF8, 0, utf8_text, -1, nullptr, 0);
    if (wchar_cnt == 0) {
        return { false, "Failed to convert UTF-8 to UTF-16." };
    }

    std::vector<wchar_t> utf16_chars(wchar_cnt);
    MultiByteToWideChar(CP_UTF8, 0, utf8_text, -1, utf16_chars.data(), wchar_cnt);

    // Step 2: Convert to lowercase using LCMapStringW
    int lower_char_cnt = LCMapStringW(LOCALE_USER_DEFAULT, LCMAP_LOWERCASE, utf16_chars.data(), -1, nullptr, 0);
    if (lower_char_cnt == 0) {
        return { false, "Failed to map string to lowercase." };
    }

    std::vector<wchar_t> utf16_chars_lower(lower_char_cnt);
    LCMapStringW(LOCALE_USER_DEFAULT, LCMAP_LOWERCASE, utf16_chars.data(), -1, utf16_chars_lower.data(), lower_char_cnt);

    // Step 3: Convert UTF-16 back to UTF-8
    int utf8_char_cnt = WideCharToMultiByte(CP_UTF8, 0, utf16_chars_lower.data(), -1, nullptr, 0, nullptr, nullptr);
    if (utf8_char_cnt == 0) {
        return { false, "Failed to convert UTF-16 to UTF-8." };
    }

    std::vector<char> utf8_chars_lower(utf8_char_cnt);
    WideCharToMultiByte(CP_UTF8, 0, utf16_chars_lower.data(), -1, utf8_chars_lower.data(), utf8_char_cnt, nullptr, nullptr);

    return { true, std::string(utf8_chars_lower.data()) };
}
