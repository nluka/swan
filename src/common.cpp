#include <algorithm>
#include <cassert>
#include <fstream>

#include "common.hpp"
#include "path.hpp"

using namespace swan;

static std::vector<path_t> s_pins = {};

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
  u64 last_idx = s_pins.size() - 1;

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
    std::ofstream out("pins.txt");
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
    std::ifstream in("pins.txt");
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
    std::ofstream out("explorer_options.bin", std::ios::binary);
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
    std::ifstream in("explorer_options.bin", std::ios::binary);
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
    std::ofstream out("windows_options.bin", std::ios::binary);
    if (!out) {
      return false;
    }

    static_assert(i8(1) == i8(true));
    static_assert(i8(0) == i8(false));

    out << i8(this->show_pinned)
        << i8(this->show_explorer_0)
        << i8(this->show_explorer_1)
        << i8(this->show_explorer_2)
        << i8(this->show_explorer_3)
        << i8(this->show_analytics)
  #if !defined (NDEBUG)
        << i8(this->show_demo)
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
    std::ifstream in("windows_options.bin", std::ios::binary);
    if (!in) {
      return false;
    }

    static_assert(i8(1) == i8(true));
    static_assert(i8(0) == i8(false));

    in >> (i8 &)this->show_pinned
       >> (i8 &)this->show_explorer_0
       >> (i8 &)this->show_explorer_1
       >> (i8 &)this->show_explorer_2
       >> (i8 &)this->show_explorer_3
       >> (i8 &)this->show_analytics
  #if !defined (NDEBUG)
      >> (i8 &)this->show_demo
  #endif
    ;

    return true;
  }
  catch (...) {
    return false;
  }
}
