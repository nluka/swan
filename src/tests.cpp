#include <iostream>
#include <cstring>

#include <shlwapi.h>

#include "term.hpp"
#include "term.cpp"

#include "ntest.hpp"
#include "ntest.cpp"

#include "primitives.hpp"

#include "path.hpp"
#include "path.cpp"

#include "util.hpp"
#include "util.cpp"

#include "bulk_rename.cpp"

using swan::path_t;

i32 main()
{
  using namespace term;
  using term::printf;

  std::printf("~~~~~~~~~~\n");
  std::printf("swan_tests\n");
  std::printf("~~~~~~~~~~\n");
  printf(FG_BRIGHT_CYAN, "std::filesystem::current_path() = [%s]\n", std::filesystem::current_path().string().c_str());

  ntest::config::set_max_arr_preview_len(3);
  ntest::config::set_max_str_preview_len(10);

  using swan::path_create;

  // ntest init
  {
    auto const res = ntest::init();
    u64 const total = res.num_files_removed + res.num_files_failed_to_remove;

    if (total > 0)
      printf(FG_MAGENTA, "ntest: ");
    if (res.num_files_removed)
      printf(FG_MAGENTA, "%zu residual files removed ", res.num_files_removed);
    if (res.num_files_failed_to_remove > 0)
      printf(FG_YELLOW, "(%zu residual files failed to be removed)", res.num_files_failed_to_remove);
    if (total > 0)
      std::printf("\n");
  }

  // flip_bool
  #if 1
  {
    bool b = true;

    flip_bool(b);
    ntest::assert_bool(false, b);

    flip_bool(b);
    ntest::assert_bool(true, b);

    flip_bool(b);
    ntest::assert_bool(false, b);
  }
  #endif

  // StrStrA
  #if 1
  {
    {
      char const *haystack = "This is some text.";
      char const *needle = "some";
      char const *expected = haystack + 8;
      ntest::assert_cstr(expected, StrStrA(haystack, needle));
    }
    {
      char const *haystack = "This is some text.";
      char const *needle = "some";
      char const *expected = haystack + 8;
      ntest::assert_cstr(expected, StrStrA(haystack, needle));
    }
    {
      char const *haystack = "";
      char const *needle = "Some";
      char const *expected = nullptr;
      ntest::assert_cstr(expected, StrStrA(haystack, needle));
    }
    {
      char const *haystack = "This is some text.";
      char const *needle = "";
      char const *expected = nullptr;
      ntest::assert_cstr(expected, StrStrA(haystack, needle));
    }
  }
  #endif

  // StrStrIA
  #if 1
  {
    {
      char const *haystack = "This is some text.";
      char const *needle = "some";
      char const *expected = haystack + 8;
      ntest::assert_cstr(expected, StrStrIA(haystack, needle));
    }
    {
      char const *haystack = "This is some text.";
      char const *needle = "Some";
      char const *expected = haystack + 8;
      ntest::assert_cstr(expected, StrStrIA(haystack, needle));
    }
    {
      char const *haystack = "This is some text.";
      char const *needle = "grease";
      char const *expected = nullptr;
      ntest::assert_cstr(expected, StrStrIA(haystack, needle));
    }
    {
      char const *haystack = "";
      char const *needle = "Some";
      char const *expected = nullptr;
      ntest::assert_cstr(expected, StrStrIA(haystack, needle));
    }
    {
      char const *haystack = "This is some text.";
      char const *needle = "";
      char const *expected = nullptr;
      ntest::assert_cstr(expected, StrStrIA(haystack, needle));
    }
  }
  #endif

  using swan::path_clear;
  #if 1
  {
    path_t p = path_create("Text.");
    ntest::assert_cstr("Text.", p.data());
    path_clear(p);
    ntest::assert_cstr("", p.data());
  }
  #endif

  using swan::path_length;
  #if 1
  {
    {
      path_t p = {};
      ntest::assert_uint64(0, path_length(p));
    }
    {
      path_t p = path_create("Text.");
      ntest::assert_cstr("Text.", p.data());
      ntest::assert_uint64(5, path_length(p));
    }
  }
  #endif

  using swan::path_ends_with;
  #if 1
  {
    {
      path_t p = {};
      ntest::assert_bool(false, path_ends_with(p, ""));
      ntest::assert_bool(false, path_ends_with(p, "text"));
    }
    {
      path_t p = path_create("Text.");
      ntest::assert_bool(false, path_ends_with(p, "text."));
      ntest::assert_bool(true, path_ends_with(p, "."));
      ntest::assert_bool(true, path_ends_with(p, "xt."));
      ntest::assert_bool(true, path_ends_with(p, "Text."));
      ntest::assert_bool(false, path_ends_with(p, "Text.Text."));
    }
  }
  #endif

  using swan::path_ends_with_one_of;
  #if 1
  {
    {
      path_t p = {};
      ntest::assert_bool(false, path_ends_with_one_of(p, "abc"));
    }
    {
      path_t p = path_create("Text");
      ntest::assert_bool(false, path_ends_with_one_of(p, "abc"));
      ntest::assert_bool(false, path_ends_with_one_of(p, "T"));
      ntest::assert_bool(true, path_ends_with_one_of(p, "t"));
      ntest::assert_bool(true, path_ends_with_one_of(p, "tt"));
      ntest::assert_bool(true, path_ends_with_one_of(p, "abct"));
      ntest::assert_bool(true, path_ends_with_one_of(p, "tabc"));
    }
  }
  #endif

  using swan::path_append;
  #if 1
  {
    {
      path_t p = {};

      ntest::assert_uint64(1, path_append(p, "C:"));
      ntest::assert_cstr("C:", p.data());

      ntest::assert_uint64(1, path_append(p, "code\\", '\\', true));
      ntest::assert_cstr("C:\\code\\", p.data());

      ntest::assert_uint64(1, path_append(p, "swan", '\\', true));
      ntest::assert_cstr("C:\\code\\swan", p.data());

      ntest::assert_uint64(1, path_append(p, ".md"));
      ntest::assert_cstr("C:\\code\\swan.md", p.data());
    }
    {
      path_t p = {};

      std::string temp(p.size() - 1, 'x');

      ntest::assert_uint64(1, path_append(p, temp.c_str()));
      ntest::assert_cstr(temp.c_str(), p.data());

      p = {};

      ntest::assert_uint64(0, path_append(p, temp.c_str(), '\\', true));
      ntest::assert_cstr("", p.data());

      p = {};

      temp.pop_back();
      ntest::assert_uint64(1, path_append(p, temp.c_str()));
      ntest::assert_cstr(temp.c_str(), p.data());
      // p is now one away from max capacity

      ntest::assert_uint64(1, path_append(p, "x"));
      temp.push_back('x');
      ntest::assert_cstr(temp.c_str(), p.data());
      // p is now at max capacity, next append should fail

      ntest::assert_uint64(0, path_append(p, "x"));
      ntest::assert_cstr(temp.c_str(), p.data()); // ensure state has not changed on failure
    }
  }
  #endif

  using swan::path_is_empty;
  #if 1
  {
    {
      path_t p = {};
      ntest::assert_bool(true, path_is_empty(p));
    }
    {
      path_t p = path_create("text");
      ntest::assert_bool(false, path_is_empty(p));
    }
  }
  #endif

  // directory_exists
  #if 1
  {
    ntest::assert_bool(false, directory_exists("not_a_directory"));
    ntest::assert_bool(true, directory_exists("src"));
  }
  #endif

  // two_u32_to_one_u64
  #if 1
  {
    u32 hi = 0xFF'F0'0F'FF;
    u32 lo = 0xFF'F1'1F'FF;
    u64 combined = two_u32_to_one_u64(lo, hi);
    ntest::assert_uint64(0xff'f0'0f'ff'ff'f1'1f'ff, combined);
  }
  #endif

  using swan::path_loosely_same;
  #if 1
  {
    {
      path_t p1 = path_create("1/2/3");
      path_t p2 = path_create("1/2/3");
      ntest::assert_bool(true, path_loosely_same(p1, p2));
      ntest::assert_bool(true, path_loosely_same(p2, p1));
    }
    {
      path_t p1 = path_create("1/2/3");
      path_t p2 = path_create("1/2/3/");
      ntest::assert_bool(true, path_loosely_same(p1, p2));
      ntest::assert_bool(true, path_loosely_same(p2, p1));
    }
    {
      path_t p1 = path_create("C:\\code\\");
      path_t p2 = path_create("C:\\code");
      ntest::assert_bool(true, path_loosely_same(p1, p2));
      ntest::assert_bool(true, path_loosely_same(p2, p1));
    }
    {
      path_t p1 = path_create("C:\\code");
      path_t p2 = path_create("C:\\");
      ntest::assert_bool(false, path_loosely_same(p1, p2));
      ntest::assert_bool(false, path_loosely_same(p2, p1));
    }
  }
  #endif

  using swan::path_squish_adjacent_separators;
  #if 1
  {
    ntest::assert_cstr("1/2/3", path_squish_adjacent_separators(path_create("1/2/3")).data());
    ntest::assert_cstr("1/2/3", path_squish_adjacent_separators(path_create("1//2//3")).data());
    ntest::assert_cstr("1/2/3/", path_squish_adjacent_separators(path_create("1//2//3//")).data());

    ntest::assert_cstr("1\\2\\3\\", path_squish_adjacent_separators(path_create("1\\2\\3\\")).data());
    ntest::assert_cstr("1\\2\\3\\", path_squish_adjacent_separators(path_create("1\\\\\\2\\3\\\\\\")).data());
  }
  #endif

  using bulk_rename::apply_pattern;
  #if 1
  {
    path_t after = {};

    // success == false
    {
      path_t before = path_create("before");
      auto [success, err_msg] = apply_pattern(before, after, "", 0, 0);
      ntest::assert_bool(false, success);
      ntest::assert_cstr("empty pattern", err_msg.data());
      ntest::assert_cstr("", after.data());
    }
    {
      path_t before = path_create("before");
      auto [success, err_msg] = apply_pattern(before, after, "<<", 0, 0);
      ntest::assert_bool(false, success);
      ntest::assert_cstr("unexpected '<' at position 1, unclosed '<' at position 0", err_msg.data());
      ntest::assert_cstr("", after.data());
    }
    {
      path_t before = path_create("before");
      auto [success, err_msg] = apply_pattern(before, after, "data<<", 0, 0);
      ntest::assert_bool(false, success);
      ntest::assert_cstr("unexpected '<' at position 5, unclosed '<' at position 4", err_msg.data());
      ntest::assert_cstr("", after.data());
    }
    {
      path_t before = path_create("before");
      auto [success, err_msg] = apply_pattern(before, after, ">", 0, 0);
      ntest::assert_bool(false, success);
      ntest::assert_cstr("unexpected '>' at position 0 - no preceding '<' found", err_msg.data());
      ntest::assert_cstr("", after.data());
    }
    {
      path_t before = path_create("before");
      auto [success, err_msg] = apply_pattern(before, after, "<>", 0, 0);
      ntest::assert_bool(false, success);
      ntest::assert_cstr("empty expression starting at position 0", err_msg.data());
      ntest::assert_cstr("", after.data());
    }
    {
      path_t before = path_create("before");
      auto [success, err_msg] = apply_pattern(before, after, "data_<bogus>", 0, 0);
      ntest::assert_bool(false, success);
      ntest::assert_cstr("unknown expression starting at position 5", err_msg.data());
      ntest::assert_cstr("", after.data());
    }
    // success == true
    {
      path_t before = path_create("before");
      auto [success, err_msg] = apply_pattern(before, after, "<counter>", 0, 0);
      ntest::assert_bool(true, success);
      ntest::assert_cstr("", err_msg.data());
      ntest::assert_cstr("0", after.data());
    }
    {
      path_t before = path_create("before");
      auto [success, err_msg] = apply_pattern(before, after, "<CoUnTeR>", 100, 0);
      ntest::assert_bool(true, success);
      ntest::assert_cstr("", err_msg.data());
      ntest::assert_cstr("100", after.data());
    }
    {
      path_t before = path_create("before");
      auto [success, err_msg] = apply_pattern(before, after, "something_<before>_bla", 0, 0);
      ntest::assert_bool(true, success);
      ntest::assert_cstr("", err_msg.data());
      ntest::assert_cstr("something_before_bla", after.data());
    }
    {
      path_t before = path_create("before");
      auto [success, err_msg] = apply_pattern(before, after, "<bytes>", 0, 42);
      ntest::assert_bool(true, success);
      ntest::assert_cstr("", err_msg.data());
      ntest::assert_cstr("42", after.data());
    }
  }
  #endif

  //
  #if 1
  {

  }
  #endif

  // ntest report output
  {
    auto const res = ntest::generate_report("swan_tests", [](ntest::assertion const &a, bool const passed) {
      if (!passed)
        printf(FG_RED, "failed: %s:%zu\n", a.loc.file_name(), a.loc.line());
    });

    if ((res.num_fails + res.num_passes) == 0) {
      printf(FG_BRIGHT_YELLOW, "No tests defined");
    } else {
      if (res.num_fails > 0) {
        printf(FG_BRIGHT_RED,   "%zu failed", res.num_fails);
        std::printf(" | ");
        printf(FG_BRIGHT_GREEN, "%zu passed", res.num_passes);
      } else
        printf(FG_BRIGHT_GREEN, "All %zu tests passed", res.num_passes);
    }
    std::printf("\n\n");

    return 0;
  }
}
