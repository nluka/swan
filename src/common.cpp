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
#   define MAX_FILE_OPS 1000
#else
#   define MAX_FILE_OPS 10
#endif

static boost::circular_buffer<file_operation> s_file_ops_buffer(MAX_FILE_OPS);
static std::vector<path_t> s_pins = {};
static BS::thread_pool s_thread_pool(1);
ImGuiTextBuffer debug_log_package::s_debug_buffer = {};

std::string get_last_error_string()
{
    DWORD error_code = GetLastError();
    if (error_code == 0)
        return "No error.";

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

    if (buffer_size == 0)
        return "Error formatting message.";

    std::string error_message(buffer, buffer + buffer_size);
    LocalFree(buffer); // Free the allocated buffer

    // Remove trailing newline characters
    while (!error_message.empty() && (error_message.back() == '\r' || error_message.back() == '\n'))
        error_message.pop_back();

    return error_message;
}

bool enqueue_file_op(
    file_operation::type op_type,
    u64 file_size,
    path_t const &src_path,
    path_t const &dest_path,
    char dir_separator) noexcept(true)
{
    // this check ensures we don't circle around and overwrite a file_operation which is in progress.
    {
        auto blank_time = time_point_t();
        if (s_file_ops_buffer.full() && s_file_ops_buffer.front().end_time.load() == blank_time) {
            return false;
        }
    }

    switch (op_type) {
        case file_operation::type::copy: {
            path_t new_file_path = dest_path;
        if (!path_append(new_file_path, get_just_file_name(src_path.data()), dir_separator, true)) {
            debug_log("failed to create new_file_path");
            return false;
        }
        }
    }

    path_t new_file_path;

    if (op_type == file_operation::type::copy || op_type == file_operation::type::move) {
        new_file_path = dest_path;

        if (!path_append(new_file_path, get_just_file_name(src_path.data()), dir_separator, true)) {
            debug_log("failed to create new_file_path");
            return false;
        }

        {
            bool file_exists = false;
            {
                FILE *f = fopen(new_file_path.data(), "rb");
                if (f) {
                    (void) fclose(f);
                    file_exists = true;
                }
                else if (GetFileAttributesA(new_file_path.data()) != INVALID_FILE_ATTRIBUTES) {
                    file_exists = true;
                }
            }

            if (file_exists) {
                // we need to create a new name to prevent overwriting

                static char const rand_chars[] = "1234567890"
                                                 "abcdefghijklmnopqrstuvwxyz"
                                                 "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

                constexpr u64 num_rand_chars_to_pick = 5;
                std::array<char, 1 + num_rand_chars_to_pick + 1> rand_str = {};

                rand_str[0] = '_';

                for (auto iter = rand_str.begin() + 1; iter != rand_str.end(); ++iter) {
                    u64 rand_idx = fast_rand(0, lengthof(rand_chars) - 2);
                    char rand_ch = rand_chars[rand_idx];
                    *iter = rand_ch;
                }

                if (!path_append(new_file_path, rand_str.data())) {
                    debug_log("failed to append rand_str.data() to new_file_path");
                    return false;
                }
            }
        }
    }

    {
        file_operation file_op(op_type, file_size, src_path, new_file_path);
        try {
            s_file_ops_buffer.push_back(file_op);
        }
        catch (...) {
            debug_log("s_file_ops_buffer.push_back(file_op) threw");
            return false;
        }
    }

    u64 file_op_idx = s_file_ops_buffer.size() - 1;

    try {
        switch (op_type) {
            case file_operation::type::copy: {
                s_thread_pool.push_task([file_op_idx] {
                    file_operation *file_op = &s_file_ops_buffer[file_op_idx];

                    file_op_progress_callback_user_data user_data = {};
                    user_data.file_op = file_op;

                    BOOL cancel = false;

                    file_op->start_time.store(current_time());

                    BOOL success = CopyFileExA(
                        file_op->src_path.data(), file_op->dest_path.data(),
                        file_op_progress_callback, (void *)&user_data,
                        &cancel, COPY_FILE_FAIL_IF_EXISTS);

                    file_op->end_time.store(current_time());
                    file_op->success = success;

                    debug_log("CopyFileExA(src = [%s], dst = [%s]) result: %d",
                        file_op->src_path.data(), file_op->dest_path.data(), success);

                    if (!success) {
                        debug_log(get_last_error_string().c_str());
                    }
                });

                return true;
            }
            case file_operation::type::remove: {
                s_thread_pool.push_task([file_op_idx] {
                    file_operation *file_op = &s_file_ops_buffer[file_op_idx];

                    file_op->start_time.store(current_time());

                    BOOL success = DeleteFileA(file_op->src_path.data());


                    file_op->end_time.store(current_time());
                    file_op->total_bytes_transferred.store(file_op->total_file_size.load());
                    file_op->success = success;

                    debug_log("DeleteFileA(%s) result: %d", file_op->src_path.data(), success);

                    if (!success) {
                        debug_log(get_last_error_string().c_str());
                    }
                });
            }
            default:
                return false;
        }
    }
    catch (...) {
        return false;
    }
}

DWORD file_op_progress_callback(
    LARGE_INTEGER total_file_size,
    LARGE_INTEGER total_bytes_transferred,
    LARGE_INTEGER stream_size,
    LARGE_INTEGER stream_bytes_transferred,
    [[maybe_unused]] DWORD stream_num,
    DWORD callback_reason,
    [[maybe_unused]] HANDLE src_handle,
    [[maybe_unused]] HANDLE dest_handle,
    void *user_data) noexcept(true)
{
    auto &file_op = *((file_op_progress_callback_user_data *)user_data)->file_op;
    file_op.total_file_size.store((u64)total_file_size.QuadPart);
    file_op.total_bytes_transferred.store((u64)total_bytes_transferred.QuadPart);
    file_op.stream_size.store((u64)stream_size.QuadPart);
    file_op.stream_bytes_transferred.store((u64)stream_bytes_transferred.QuadPart);

    if (callback_reason == CALLBACK_CHUNK_FINISHED) {

    }
    else if (callback_reason == CALLBACK_STREAM_SWITCH) {

    }
    else {

    }

    return PROGRESS_CONTINUE;
}

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
    std::ofstream out("data/explorer_options.bin", std::ios::binary);
    if (!out) {
      return false;
    }

    static_assert(i8(1) == i8(true));
    static_assert(i8(0) == i8(false));

    out << i8(this->binary_size_system)
        << i8(this->show_cwd_len)
        << i8(this->show_debug_info)
        << i8(this->automatic_refresh)
        << i8(this->show_dotdot_dir)
        << i8(this->unix_directory_separator);

    return true;
  }
  catch (...) {
    return false;
  }
}

bool explorer_options::load_from_disk() noexcept(true)
{
  try {
    std::ifstream in("data/explorer_options.bin", std::ios::binary);
    if (!in) {
      return false;
    }

  static_assert(i8(1) == i8(true));
  static_assert(i8(0) == i8(false));

    in >> (i8 &)this->binary_size_system
       >> (i8 &)this->show_cwd_len
       >> (i8 &)this->show_debug_info
       >> (i8 &)this->automatic_refresh
       >> (i8 &)this->show_dotdot_dir
       >> (i8 &)this->unix_directory_separator;

    return true;
  }
  catch (...) {
    return false;
  }
}

bool windows_options::save_to_disk() const noexcept(true)
{
  try {
    std::ofstream out("data/windows_options.bin", std::ios::binary);
    if (!out) {
      return false;
    }

    static_assert(i8(1) == i8(true));
    static_assert(i8(0) == i8(false));

    out << i8(this->show_pinned)
        << i8(this->show_file_operations)
        << i8(this->show_explorer_0)
        << i8(this->show_explorer_1)
        << i8(this->show_explorer_2)
        << i8(this->show_explorer_3)
        << i8(this->show_analytics)
  #if !defined (NDEBUG)
        << i8(this->show_demo)
        << i8(this->show_debug_log)
  #endif
    ;

    return true;
  }
  catch (...) {
    return false;
  }
}

bool windows_options::load_from_disk() noexcept(true)
{
  try {
    std::ifstream in("data/windows_options.bin", std::ios::binary);
    if (!in) {
      return false;
    }

    static_assert(i8(1) == i8(true));
    static_assert(i8(0) == i8(false));

    in >> (i8 &)this->show_pinned
       >> (i8 &)this->show_file_operations
       >> (i8 &)this->show_explorer_0
       >> (i8 &)this->show_explorer_1
       >> (i8 &)this->show_explorer_2
       >> (i8 &)this->show_explorer_3
       >> (i8 &)this->show_analytics
  #if !defined (NDEBUG)
       >> (i8 &)this->show_demo
       >> (i8 &)this->show_debug_log
  #endif
    ;

    return true;
  }
  catch (...) {
    return false;
  }
}

char const *get_just_file_name(char const *std__source_location__file_path) noexcept(true)
{
    // MSVC does some cursed shit with std::source_location::file_name,
    // sometimes it returns the realpath, sometimes it returns a relative path.
    // hence we process it to extract just the "actual" file name.

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

bool query_directory_entries(std::vector<basic_dir_ent> &entries, path_t dir_path) noexcept(true)
{
    static std::string search_path{};
    {
        search_path.reserve(path_length(dir_path) + 2);
        search_path = dir_path.data();

        if (search_path.back() != '/' && search_path.back() != '\\') {
            search_path += '/';
        }
        search_path += '*';
    }

    WIN32_FIND_DATAA find_data;
    HANDLE find_handle = FindFirstFileA(search_path.data(), &find_data);

    auto find_handle_cleanup_routine = make_on_scope_exit([&find_handle] { FindClose(find_handle); });

    if (find_handle == INVALID_HANDLE_VALUE) {
        return false;
    }

    u32 id = 0;

    do {
        basic_dir_ent entry = {};
        entry.id = id;
        std::strncpy(entry.path.data(), find_data.cFileName, entry.path.size());
        entry.size = two_u32_to_one_u64(find_data.nFileSizeLow, find_data.nFileSizeHigh);
        entry.creation_time_raw = find_data.ftCreationTime;
        entry.last_write_time_raw = find_data.ftLastWriteTime;

        if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            entry.type = basic_dir_ent::kind::directory;
        }
        else if (path_ends_with(entry.path, ".lnk")) {
            entry.type = basic_dir_ent::kind::symlink;
        }
        else {
            entry.type = basic_dir_ent::kind::file;
        }

        if (path_equals_exactly(entry.path, ".")) {
            continue;
        }

        if (!path_equals_exactly(entry.path, ".") && !path_equals_exactly(entry.path, "..")) {
            entries.emplace_back(entry);
        }

        ++id;
    }
    while (FindNextFileA(find_handle, &find_data));

    return true;
}
