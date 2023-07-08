#include <iostream>
#include <cstring>

#include <shlwapi.h>

#include "term.hpp"
#include "term.cpp"

#include "ntest.hpp"
#include "ntest.cpp"

#include "primitives.cpp"
#include "path.cpp"
#include "util.cpp"

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

  // path_clear
  #if 1
  {
    path_t p = {};
    std::strcpy(p.data(), "Text.");
    ntest::assert_cstr("Text.", p.data());
    path_clear(p);
    ntest::assert_cstr("", p.data());
  }
  #endif

  // path_length
  #if 1
  {
    path_t p = {};
    ntest::assert_uint64(0, path_length(p));

    std::strcpy(p.data(), "Text.");
    ntest::assert_cstr("Text.", p.data());
    ntest::assert_uint64(5, path_length(p));
  }
  #endif

  // path_ends_with
  #if 1
  {
    path_t p = {};

    ntest::assert_bool(false, path_ends_with(p, ""));
    ntest::assert_bool(false, path_ends_with(p, "text"));

    std::strcpy(p.data(), "Text.");
    ntest::assert_bool(false, path_ends_with(p, "text."));
    ntest::assert_bool(true, path_ends_with(p, "."));
    ntest::assert_bool(true, path_ends_with(p, "xt."));
    ntest::assert_bool(true, path_ends_with(p, "Text."));
    ntest::assert_bool(false, path_ends_with(p, "Text.Text."));
  }
  #endif

  // path_ends_with_one_of
  #if 1
  {
    path_t p = {};

    ntest::assert_bool(false, path_ends_with_one_of(p, "abc"));

    std::strcpy(p.data(), "Text");
    ntest::assert_bool(false, path_ends_with_one_of(p, "abc"));
    ntest::assert_bool(false, path_ends_with_one_of(p, "T"));
    ntest::assert_bool(true, path_ends_with_one_of(p, "t"));
    ntest::assert_bool(true, path_ends_with_one_of(p, "tt"));
    ntest::assert_bool(true, path_ends_with_one_of(p, "abct"));
    ntest::assert_bool(true, path_ends_with_one_of(p, "tabc"));
  }
  #endif

  // path_append
  #if 1
  {
    {
      path_t p = {};
      auto result = path_append_result::nil;

      result = path_append(p, "C:");
      ntest::assert_int32((i32)path_append_result::success, (i32)result);
      ntest::assert_cstr("C:", p.data());

      result = path_append(p, "code\\", true);
      ntest::assert_int32((i32)path_append_result::success, (i32)result);
      ntest::assert_cstr("C:\\code\\", p.data());

      result = path_append(p, "swan", true);
      ntest::assert_int32((i32)path_append_result::success, (i32)result);
      ntest::assert_cstr("C:\\code\\swan", p.data());

      result = path_append(p, ".md");
      ntest::assert_int32((i32)path_append_result::success, (i32)result);
      ntest::assert_cstr("C:\\code\\swan.md", p.data());
    }
    {
      path_t p = {};
      auto result = path_append_result::nil;

      std::string temp(p.size() - 1, 'x');

      result = path_append(p, temp.c_str());
      ntest::assert_int32((i32)path_append_result::success, (i32)result);
      ntest::assert_cstr(temp.c_str(), p.data());

      p = {};

      result = path_append(p, temp.c_str(), true);
      ntest::assert_int32((i32)path_append_result::exceeds_max_path, (i32)result);
      ntest::assert_cstr("", p.data());

      p = {};

      temp.pop_back();
      result = path_append(p, temp.c_str());
      ntest::assert_int32((i32)path_append_result::success, (i32)result);
      ntest::assert_cstr(temp.c_str(), p.data());
      // p is now one away from max capacity

      result = path_append(p, "x");
      ntest::assert_int32((i32)path_append_result::success, (i32)result);
      temp.push_back('x');
      ntest::assert_cstr(temp.c_str(), p.data());
      // p is now at max capacity, next append should fail

      result = path_append(p, "x");
      ntest::assert_int32((i32)path_append_result::exceeds_max_path, (i32)result);
      ntest::assert_cstr(temp.c_str(), p.data()); // ensure state has not changed on failure
    }
  }
  #endif

  // path_is_empty
  #if 1
  {
    path_t p = {};
    ntest::assert_bool(true, path_is_empty(p));
    std::strcpy(p.data(), "text");
    ntest::assert_bool(false, path_is_empty(p));
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
