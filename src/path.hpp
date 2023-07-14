#ifndef SWAN_PATH_HPP
#define SWAN_PATH_HPP

#include <array>

#include <windows.h>

#include "primitives.hpp"

namespace swan {

  typedef std::array<char, MAX_PATH> path_t;

  u64 path_length(path_t const &path) noexcept(true);

  bool path_ends_with(path_t const &path, char const *end) noexcept(true);

  bool path_ends_with_one_of(path_t const &path, char const *chars) noexcept(true);

  bool path_is_empty(path_t const &path) noexcept(true);

  void path_clear(path_t &path) noexcept(true);

  void path_force_separator(path_t &path, char dir_separator) noexcept(true);

  char path_pop_back(path_t &path) noexcept(true);

  bool path_pop_back_if(path_t &path, char if_ch) noexcept(true);

  bool path_pop_back_if_not(path_t &path, char if_not_ch) noexcept(true);

  enum class path_append_result : i32
  {
      nil = -1,
      success = 0,
      exceeds_max_path,
  };

  path_append_result path_append(
    swan::path_t &path,
    char const *str,
    char dir_separator = 0,
    bool prepend_slash = false,
    bool postpend_slash = false) noexcept(true);

} // namespace swan

#endif // SWAN_PATH_HPP
