#pragma once

#include "stdafx.hpp"
#include "data_types.hpp"

struct swan_path; // Microsoft IntelliSense generates nonsensical errors without this declaration, despite the compiler not complaining.

swan_path path_create(char const *data, u64 count = u64(-1)) noexcept;

u16 path_length(swan_path const &path) noexcept;

bool path_ends_with(swan_path const &path, char const *end) noexcept;

bool path_ends_with_one_of(swan_path const &path, char const *chars) noexcept;

bool path_is_empty(swan_path const &path) noexcept;

void path_clear(swan_path &path) noexcept;

void path_force_separator(swan_path &path, char dir_separator) noexcept;

char path_pop_back(swan_path &path) noexcept;

bool path_pop_back_if(swan_path &path, char if_ch) noexcept;
bool path_pop_back_if(swan_path &path, char const *chs) noexcept;

bool path_pop_back_if_not(swan_path &path, char if_not_ch) noexcept;

u64 path_append(
  swan_path &path,
  char const *str,
  char dir_separator = 0,
  bool prepend_slash = false,
  bool postpend_slash = false) noexcept;

bool path_loosely_same(swan_path const &p1, swan_path const &p2) noexcept;
bool path_loosely_same(swan_path const &p1, char const *p2, u64 p2_len = u64(-1)) noexcept;
bool path_loosely_same(char const *p1, swan_path const &p2, u64 p1_len = u64(-1)) noexcept;
bool path_loosely_same(char const *p1, char const *p2, u64 p1_len = u64(-1), u64 p2_len = u64(-1)) noexcept;

bool path_equals_exactly(swan_path const &p1, swan_path const &p2) noexcept;

bool path_equals_exactly(swan_path const &p1, char const *p2) noexcept;

swan_path path_squish_adjacent_separators(swan_path const &path) noexcept;

swan_path path_reconstruct_canonically(char const *path_utf8, char dir_sep_utf8) noexcept;
