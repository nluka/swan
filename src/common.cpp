#include <algorithm>
#include <cassert>
#include <fstream>
#include <thread>

#include "BS_thread_pool.hpp"
#include "common.hpp"
#include "path.hpp"
#include "on_scope_exit.hpp"

using namespace swan;

#if defined(NDEBUG)
bool debug_log_package::s_logging_enabled = false;
#   define MAX_FILE_OPS 1000
#else
bool debug_log_package::s_logging_enabled = true;
#   define MAX_FILE_OPS 10
#endif

static boost::circular_buffer<file_operation> s_file_ops_buffer(MAX_FILE_OPS);
static std::vector<path_t> s_pins = {};
static BS::thread_pool s_thread_pool(1);
ImGuiTextBuffer debug_log_package::s_debug_buffer = {};

boost::circular_buffer<file_operation> const &get_file_ops_buffer() noexcept(true)
{
    return s_file_ops_buffer;
}

std::vector<path_t> const &get_pins() noexcept(true)
{
    return s_pins;
}

bool pin(path_t &path, char dir_separator) noexcept(true)
{
    path_force_separator(path, dir_separator);

    try {
        s_pins.push_back(path);
        return true;
    } catch (...) {
        return false;
    }
}

void unpin(u64 pin_idx) noexcept(true)
{
    [[maybe_unused]] u64 last_idx = s_pins.size() - 1;

    assert(pin_idx <= last_idx);

    s_pins.erase(s_pins.begin() + pin_idx);
}

void swap_pins(u64 pin1_idx, u64 pin2_idx) noexcept(true)
{
    assert(pin1_idx != pin2_idx);

    if (pin1_idx > pin2_idx) {
        u64 temp = pin1_idx;
        pin1_idx = pin2_idx;
        pin2_idx = temp;
    }

    std::swap(*(s_pins.begin() + pin1_idx), *(s_pins.begin() + pin2_idx));
}

u64 find_pin_idx(path_t const &path) noexcept(true)
{
    for (u64 i = 0; i < s_pins.size(); ++i) {
        if (path_loosely_same(s_pins[i], path)) {
            return i;
        }
    }
    return std::string::npos;
}

bool save_pins_to_disk() noexcept(true)
{
    try {
        std::ofstream out("data/pins.txt");
        if (!out) {
            return false;
        }

        auto const &pins = get_pins();
        for (auto const &pin : pins) {
            out << pin.data() << '\n';
        }

        return true;
    }
    catch (...) {
        return false;
    }
}

void update_pin_dir_separators(char new_dir_separator) noexcept(true)
{
    for (auto &pin : s_pins) {
        path_force_separator(pin, new_dir_separator);
    }
}

std::pair<bool, u64> load_pins_from_disk(char dir_separator) noexcept(true)
{
    try {
        std::ifstream in("data/pins.txt");
        if (!in) {
            return { false, 0 };
        }

        s_pins.clear();

        std::string temp = {};
        temp.reserve(MAX_PATH);

        u64 num_loaded_successfully = 0;

        while (std::getline(in, temp)) {
            path_t temp2;

            if (temp.length() < temp2.size()) {
                strcpy(temp2.data(), temp.c_str());
                pin(temp2, dir_separator);
                ++num_loaded_successfully;
            }

            temp.clear();
        }

        return { true, num_loaded_successfully };
    }
    catch (...) {
        return { false, 0 };
    }
}

bool explorer_options::save_to_disk() const noexcept(true)
{
    try {
        std::ofstream out("data/explorer_options.txt", std::ios::binary);

        if (!out) {
            return false;
        }

        static_assert(i8(1) == i8(true));
        static_assert(i8(0) == i8(false));

        out << "auto_refresh_interval_ms " << this->auto_refresh_interval_ms << '\n';
        out << "adaptive_refresh_threshold " << this->adaptive_refresh_threshold << '\n';
        out << "ref_mode " << (i32)this->ref_mode << '\n';
        out << "binary_size_system " << (i32)this->binary_size_system << '\n';
        out << "show_cwd_len " << (i32)this->show_cwd_len << '\n';
        out << "show_debug_info " << (i32)this->show_debug_info << '\n';
        out << "show_dotdot_dir " << (i32)this->show_dotdot_dir << '\n';
        out << "unix_directory_separator " << (i32)this->unix_directory_separator << '\n';

        return true;
    }
    catch (...) {
        return false;
    }
}

bool explorer_options::load_from_disk() noexcept(true)
{
    try {
        std::ifstream in("data/explorer_options.txt", std::ios::binary);
        if (!in) {
            return false;
        }

        static_assert(i8(1) == i8(true));
        static_assert(i8(0) == i8(false));

        std::string what = {};
        what.reserve(100);
        char bit_ch = 0;

        {
            in >> what;
            assert(what == "auto_refresh_interval_ms");
            in >> (i32 &)this->auto_refresh_interval_ms;
        }
        {
            in >> what;
            assert(what == "adaptive_refresh_threshold");
            in >> (i32 &)this->adaptive_refresh_threshold;
        }
        {
            in >> what;
            assert(what == "ref_mode");
            in >> (i32 &)this->ref_mode;
        }
        {
            in >> what;
            assert(what == "binary_size_system");
            in >> bit_ch;
            this->binary_size_system = bit_ch == '1' ? 1 : 0;
        }
        {
            in >> what;
            assert(what == "show_cwd_len");
            in >> bit_ch;
            this->show_cwd_len = bit_ch == '1' ? 1 : 0;
        }
        {
            in >> what;
            assert(what == "show_debug_info");
            in >> bit_ch;
            this->show_debug_info = bit_ch == '1' ? 1 : 0;
        }
        {
            in >> what;
            assert(what == "show_dotdot_dir");
            in >> bit_ch;
            this->show_dotdot_dir = bit_ch == '1' ? 1 : 0;
        }
        {
            in >> what;
            assert(what == "unix_directory_separator");
            in >> bit_ch;
            this->unix_directory_separator = bit_ch == '1' ? 1 : 0;
        }

        return true;
    }
    catch (...) {
        return false;
    }
}

bool windows_options::save_to_disk() const noexcept(true)
{
    try {
        std::ofstream out("data/windows_options.txt", std::ios::binary);
        if (!out) {
        return false;
        }

        out << "show_pinned " << this->show_pinned << '\n';
        out << "show_file_operations " << this->show_file_operations << '\n';
        out << "show_explorer_0 " << this->show_explorer_0 << '\n';
        out << "show_explorer_1 " << this->show_explorer_1 << '\n';
        out << "show_explorer_2 " << this->show_explorer_2 << '\n';
        out << "show_explorer_3 " << this->show_explorer_3 << '\n';
        out << "show_analytics " << this->show_analytics << '\n';
    #if !defined (NDEBUG)
        out << "show_demo " << this->show_demo << '\n';
        out << "show_debug_log " << this->show_debug_log << '\n';
    #endif

        return true;
    }
    catch (...) {
        return false;
    }
}

bool windows_options::load_from_disk() noexcept(true)
{
    try {
        std::ifstream in("data/windows_options.txt", std::ios::binary);
        if (!in) {
            return false;
        }

        static_assert(i8(1) == i8(true));
        static_assert(i8(0) == i8(false));

        std::string what = {};
        what.reserve(100);
        char bit_ch = 0;

        {
            in >> what;
            assert(what == "show_pinned");
            in >> bit_ch;
            this->show_pinned = bit_ch == '1' ? 1 : 0;
        }
        {
            in >> what;
            assert(what == "show_file_operations");
            in >> bit_ch;
            this->show_file_operations = bit_ch == '1' ? 1 : 0;
        }
        {
            in >> what;
            assert(what == "show_explorer_0");
            in >> bit_ch;
            this->show_explorer_0 = bit_ch == '1' ? 1 : 0;
        }
        {
            in >> what;
            assert(what == "show_explorer_1");
            in >> bit_ch;
            this->show_explorer_1 = bit_ch == '1' ? 1 : 0;
        }
        {
            in >> what;
            assert(what == "show_explorer_2");
            in >> bit_ch;
            this->show_explorer_2 = bit_ch == '1' ? 1 : 0;
        }
        {
            in >> what;
            assert(what == "show_explorer_3");
            in >> bit_ch;
            this->show_explorer_3 = bit_ch == '1' ? 1 : 0;
        }
        {
            in >> what;
            assert(what == "show_analytics");
            in >> bit_ch;
            this->show_analytics = bit_ch == '1' ? 1 : 0;
        }

    #if !defined(NDEBUG)
        {
            in >> what;
            assert(what == "show_demo");
            in >> bit_ch;
            this->show_demo = bit_ch == '1' ? 1 : 0;
        }
        {
            in >> what;
            assert(what == "show_debug_log");
            in >> bit_ch;
            this->show_debug_log = bit_ch == '1' ? 1 : 0;
        }
    #endif

        return true;
    }
    catch (...) {
        return false;
    }
}

char const *get_just_file_name(char const *std__source_location__file_path) noexcept(true)
{
    // C:\code\swan\src\explorer_window.cpp
    //                  ^^^^^^^^^^^^^^^^^^^ what we are after
    // src/swan.cpp
    //     ^^^^^^^^ what we are after

    char const *just_the_file_name = std__source_location__file_path;

    std::string_view view(just_the_file_name);

    u64 last_sep_pos = view.find_last_of("\\/");

    if (last_sep_pos != std::string::npos) {
        just_the_file_name += last_sep_pos + 1;
    }

    return just_the_file_name;
}

std::string get_last_error_string() noexcept(true)
{
    DWORD error_code = GetLastError();
    if (error_code == 0) {
        return "No error.";
    }

    LPSTR buffer = nullptr;
    DWORD buffer_size = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        error_code,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPSTR>(&buffer),
        0,
        nullptr
    );

    if (buffer_size == 0) {
        return "Error formatting message.";
    }

    std::string error_message(buffer, buffer + buffer_size);
    LocalFree(buffer);

    // Remove trailing newline characters
    while (!error_message.empty() && (error_message.back() == '\r' || error_message.back() == '\n')) {
        error_message.pop_back();
    }

    return error_message;
}
