#include <algorithm>
#include <cassert>
#include <fstream>
#include <thread>

#include "BS_thread_pool.hpp"
#include "common.hpp"
#include "path.hpp"

using namespace swan;

static std::deque<file_operation> s_file_ops_queue = {};
static std::vector<path_t> s_pins = {};
static BS::thread_pool s_thread_pool(1);
ImGuiTextBuffer debug_log_package::s_debug_buffer = {};

bool enqueue_file_op(
  file_operation::type op_type,
  path_t const &src_path,
  path_t const &dest_path,
  char dir_separator) noexcept(true)
{
  (void)dir_separator;

  // if (op_type == file_operation::type::copy) {
  //   s_thread_pool.push_task([file = file_path, dest = dest_path, dir_sep = dir_separator]() -> bool {
  //     path_t new_file_path = dest;
  //     if (!path_append(new_file_path, file.data(), dir_sep, true)) {
  //       // error
  //       return false;
  //     }
  //     return CopyFileExA(file.data(), new_file_path.data(), )
  //   });
  // }

  try {
    s_file_ops_queue.emplace_front(0, 0, 0, 0, time_point_t(), op_type, src_path, dest_path);
    return true;
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
    file_op.total_file_size = (u64)total_file_size.QuadPart;
    file_op.total_bytes_transferred = (u64)total_bytes_transferred.QuadPart;
    file_op.stream_size = (u64)stream_size.QuadPart;
    file_op.stream_bytes_transferred = (u64)stream_bytes_transferred.QuadPart;

    if (callback_reason == CALLBACK_CHUNK_FINISHED) {

    }
    else if (callback_reason == CALLBACK_STREAM_SWITCH) {

    }
    else {

    }

    return PROGRESS_CONTINUE;
}

std::deque<file_operation> const &get_file_ops_queue() noexcept(true)
{
    return s_file_ops_queue;
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
