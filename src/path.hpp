#pragma once

#include "stdafx.hpp"

typedef std::array<char, ((MAX_PATH - 1) * 4) + 1> swan_path_t;
// typedef std::span<char> swan_path_t;

[[nodiscard]] swan_path_t path_create(char const *data) noexcept;

[[nodiscard]] u16 path_length(swan_path_t const &path) noexcept;

bool path_ends_with(swan_path_t const &path, char const *end) noexcept;

bool path_ends_with_one_of(swan_path_t const &path, char const *chars) noexcept;

bool path_is_empty(swan_path_t const &path) noexcept;

void path_clear(swan_path_t &path) noexcept;

void path_force_separator(swan_path_t &path, char dir_separator) noexcept;

char path_pop_back(swan_path_t &path) noexcept;

bool path_pop_back_if(swan_path_t &path, char if_ch) noexcept;

bool path_pop_back_if_not(swan_path_t &path, char if_not_ch) noexcept;

[[nodiscard]] u64 path_append(
  swan_path_t &path,
  char const *str,
  char dir_separator = 0,
  bool prepend_slash = false,
  bool postpend_slash = false) noexcept;

bool path_loosely_same(swan_path_t const &p1, swan_path_t const &p2) noexcept;

bool path_equals_exactly(swan_path_t const &p1, swan_path_t const &p2) noexcept;

bool path_equals_exactly(swan_path_t const &p1, char const *p2) noexcept;

[[nodiscard]] swan_path_t path_squish_adjacent_separators(swan_path_t const &path) noexcept;
