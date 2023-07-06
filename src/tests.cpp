#include <iostream>
#include <cstring>

#include "primitives.hpp"
#include "ntest.hpp"
#include "term.hpp"

#include "term.cpp"
#include "ntest.cpp"
#include "util.cpp"

#include <shlwapi.h>

i32 main()
{
  using namespace term;
  using term::printf;

  std::printf("std::filesystem::current_path() = %s\n", std::filesystem::current_path().string().c_str());

  ntest::config::set_max_arr_preview_len(1);
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
