#ifndef SWAN_PATH_HPP
#define SWAN_PATH_HPP

#include <array>

#include <windows.h>

#include "primitives.hpp"

namespace swan {

  typedef std::array<char, MAX_PATH> path_t;

  path_t path_create(char const *data) noexcept(true);

  u16 path_length(path_t const &path) noexcept(true);

  bool path_ends_with(path_t const &path, char const *end) noexcept(true);

  bool path_ends_with_one_of(path_t const &path, char const *chars) noexcept(true);

  bool path_is_empty(path_t const &path) noexcept(true);

  void path_clear(path_t &path) noexcept(true);

  void path_force_separator(path_t &path, char dir_separator) noexcept(true);

  char path_pop_back(path_t &path) noexcept(true);

  bool path_pop_back_if(path_t &path, char if_ch) noexcept(true);

  bool path_pop_back_if_not(path_t &path, char if_not_ch) noexcept(true);

  u64 path_append(
    swan::path_t &path,
    char const *str,
    char dir_separator = 0,
    bool prepend_slash = false,
    bool postpend_slash = false) noexcept(true);

  bool path_loosely_same(path_t const &p1, path_t const &p2) noexcept(true);

  swan::path_t path_squish_adjacent_separators(path_t const &path) noexcept(true);

} // namespace swan

#endif // SWAN_PATH_HPP
