#define NO_GUI

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

s32 main()
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

  // path_clear;
  #if 1
  {
    swan_path_t p = path_create("Text.");
    ntest::assert_cstr("Text.", p.data());
    path_clear(p);
    ntest::assert_cstr("", p.data());
  }
  #endif

  // path_length;
  #if 1
  {
    {
      swan_path_t p = {};
      ntest::assert_uint64(0, path_length(p));
    }
    {
      swan_path_t p = path_create("Text.");
      ntest::assert_cstr("Text.", p.data());
      ntest::assert_uint64(5, path_length(p));
    }
  }
  #endif

  // path_ends_with;
  #if 1
  {
    {
      swan_path_t p = {};
      ntest::assert_bool(false, path_ends_with(p, ""));
      ntest::assert_bool(false, path_ends_with(p, "text"));
    }
    {
      swan_path_t p = path_create("Text.");
      ntest::assert_bool(false, path_ends_with(p, "text."));
      ntest::assert_bool(true, path_ends_with(p, "."));
      ntest::assert_bool(true, path_ends_with(p, "xt."));
      ntest::assert_bool(true, path_ends_with(p, "Text."));
      ntest::assert_bool(false, path_ends_with(p, "Text.Text."));
    }
  }
  #endif

  // path_ends_with_one_of;
  #if 1
  {
    {
      swan_path_t p = {};
      ntest::assert_bool(false, path_ends_with_one_of(p, "abc"));
    }
    {
      swan_path_t p = path_create("Text");
      ntest::assert_bool(false, path_ends_with_one_of(p, "abc"));
      ntest::assert_bool(false, path_ends_with_one_of(p, "T"));
      ntest::assert_bool(true, path_ends_with_one_of(p, "t"));
      ntest::assert_bool(true, path_ends_with_one_of(p, "tt"));
      ntest::assert_bool(true, path_ends_with_one_of(p, "abct"));
      ntest::assert_bool(true, path_ends_with_one_of(p, "tabc"));
    }
  }
  #endif

  // path_append;
  #if 1
  {
    {
      swan_path_t p = {};

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
      swan_path_t p = {};

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

  // path_is_empty;
  #if 1
  {
    {
      swan_path_t p = {};
      ntest::assert_bool(true, path_is_empty(p));
    }
    {
      swan_path_t p = path_create("text");
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

  // path_loosely_same;
  #if 1
  {
    {
      swan_path_t p1 = path_create("1/2/3");
      swan_path_t p2 = path_create("1/2/3");
      ntest::assert_bool(true, path_loosely_same(p1, p2));
      ntest::assert_bool(true, path_loosely_same(p2, p1));
    }
    {
      swan_path_t p1 = path_create("1/2/3");
      swan_path_t p2 = path_create("1/2/3/");
      ntest::assert_bool(true, path_loosely_same(p1, p2));
      ntest::assert_bool(true, path_loosely_same(p2, p1));
    }
    {
      swan_path_t p1 = path_create("C:\\code\\");
      swan_path_t p2 = path_create("C:\\code");
      ntest::assert_bool(true, path_loosely_same(p1, p2));
      ntest::assert_bool(true, path_loosely_same(p2, p1));
    }
    {
      swan_path_t p1 = path_create("C:\\code");
      swan_path_t p2 = path_create("C:\\");
      ntest::assert_bool(false, path_loosely_same(p1, p2));
      ntest::assert_bool(false, path_loosely_same(p2, p1));
    }
  }
  #endif

  // path_squish_adjacent_separators;
  #if 1
  {
    ntest::assert_cstr("1/2/3", path_squish_adjacent_separators(path_create("1/2/3")).data());
    ntest::assert_cstr("1/2/3", path_squish_adjacent_separators(path_create("1//2//3")).data());
    ntest::assert_cstr("1/2/3/", path_squish_adjacent_separators(path_create("1//2//3//")).data());

    ntest::assert_cstr("1\\2\\3\\", path_squish_adjacent_separators(path_create("1\\2\\3\\")).data());
    ntest::assert_cstr("1\\2\\3\\", path_squish_adjacent_separators(path_create("1\\\\\\2\\3\\\\\\")).data());
  }
  #endif

  // bulk_rename_transform;
  #if 1
  {
    std::array<char, 1025> after = {};

    // success == false
    {
      auto [success, err_msg] = bulk_rename_transform("before", "ext", after, "", 0, 0, false);
      ntest::assert_bool(false, success);
      ntest::assert_cstr("empty pattern", err_msg.data());
      ntest::assert_cstr("", after.data());
    }
    {
      auto [success, err_msg] = bulk_rename_transform("before", "ext", after, "<<", 0, 0, false);
      ntest::assert_bool(false, success);
      ntest::assert_cstr("unexpected '<' at position 1, unclosed '<' at position 0", err_msg.data());
      ntest::assert_cstr("", after.data());
    }
    {
      auto [success, err_msg] = bulk_rename_transform("before", "ext", after, "data<<", 0, 0, false);
      ntest::assert_bool(false, success);
      ntest::assert_cstr("unexpected '<' at position 5, unclosed '<' at position 4", err_msg.data());
      ntest::assert_cstr("", after.data());
    }
    {
      auto [success, err_msg] = bulk_rename_transform("before", "ext", after, ">", 0, 0, false);
      ntest::assert_bool(false, success);
      ntest::assert_cstr("unexpected '>' at position 0 - no preceding '<' found", err_msg.data());
      ntest::assert_cstr("", after.data());
    }
    {
      auto [success, err_msg] = bulk_rename_transform("before", "ext", after, "<>", 0, 0, false);
      ntest::assert_bool(false, success);
      ntest::assert_cstr("empty expression starting at position 0", err_msg.data());
      ntest::assert_cstr("", after.data());
    }
    {
      auto [success, err_msg] = bulk_rename_transform("before", "ext", after, "data_<bogus>", 0, 0, false);
      ntest::assert_bool(false, success);
      ntest::assert_cstr("unknown expression starting at position 5", err_msg.data());
      ntest::assert_cstr("", after.data());
    }
    // success == true
    {
      auto [success, err_msg] = bulk_rename_transform("before", "ext", after, "<counter>", 0, 0, false);
      ntest::assert_bool(true, success);
      ntest::assert_cstr("", err_msg.data());
      ntest::assert_cstr("0", after.data());
    }
    {
      auto [success, err_msg] = bulk_rename_transform("before", "ext", after, "<CoUnTeR>", 100, 0, false);
      ntest::assert_bool(true, success);
      ntest::assert_cstr("", err_msg.data());
      ntest::assert_cstr("100", after.data());
    }
    {
      auto [success, err_msg] = bulk_rename_transform("before", "ext", after, "something_<name>_bla", 0, 0, false);
      ntest::assert_bool(true, success);
      ntest::assert_cstr("", err_msg.data());
      ntest::assert_cstr("something_before_bla", after.data());
    }
    {
      auto [success, err_msg] = bulk_rename_transform("before", "ext", after, "<name>  <bytes>.<ext>", 0, 42, false);
      ntest::assert_bool(true, success);
      ntest::assert_cstr("", err_msg.data());
      ntest::assert_cstr("before  42.ext", after.data());
    }
    {
      auto [success, err_msg] = bulk_rename_transform("29.  Gladiator Boss", "mp3", after, "<name>.<ext>", 0, 42, false);
      ntest::assert_bool(true, success);
      ntest::assert_cstr("", err_msg.data());
      ntest::assert_cstr("29.  Gladiator Boss.mp3", after.data());
    }
    {
      auto [success, err_msg] = bulk_rename_transform("before", "ext", after, "<name>  <bytes>.<ext>", 0, 42, true);
      ntest::assert_bool(true, success);
      ntest::assert_cstr("", err_msg.data());
      ntest::assert_cstr("before 42.ext", after.data());
    }
    {
      auto [success, err_msg] = bulk_rename_transform("29.  Gladiator Boss", "mp3", after, "<name>.<ext>", 0, 42, true);
      ntest::assert_bool(true, success);
      ntest::assert_cstr("", err_msg.data());
      ntest::assert_cstr("29. Gladiator Boss.mp3", after.data());
    }
  }
  #endif

  // bulk_rename_find_collisions_1
  #if 0
  {
    using ent_kind = basic_dir_ent::kind;

    auto create_basic_dirent = [](char const *path, ent_kind type) -> basic_dir_ent {
      return {
        0, // size
        {}, // creation_time_raw
        {}, // last_write_time_raw
        {}, // id
        type,
        path_create(path)
      };
    };

    bool selected = true;
    bool not_selected = false;

    {
      std::vector<explorer_window::dir_ent> dest = {};
      std::vector<bulk_rename_op> input_renames = {};
      std::vector<bulk_rename::collision_1> expected = {};

      auto actual = find_collisions_1(dest, input_renames);

      ntest::assert_stdvec(expected, actual);
    }

    {
      std::vector<explorer_window::dir_ent> dest = {
        { create_basic_dirent("file1.cpp", ent_kind::file), false, not_selected },
        { create_basic_dirent("file2.cpp", ent_kind::file), false, not_selected },
        { create_basic_dirent("file3.cpp", ent_kind::file), false, not_selected },
        { create_basic_dirent("file4.cpp", ent_kind::file), false, not_selected },
      };
      std::vector<bulk_rename_op> input_renames = {};
      std::vector<bulk_rename::collision_1> expected = {};

      auto actual = find_collisions_1(dest, input_renames);

      ntest::assert_stdvec(expected, actual);
    }

    {
      std::vector<explorer_window::dir_ent> dest = {
        { create_basic_dirent("file1.cpp", ent_kind::file), false, selected },
        { create_basic_dirent("file2.cpp", ent_kind::file), false, selected },
        { create_basic_dirent("file3.cpp", ent_kind::file), false, selected },
        { create_basic_dirent("file4.cpp", ent_kind::file), false, selected },
      };
      std::vector<bulk_rename_op> input_renames = {
        { &dest[0].basic /* file1.cpp */, path_create("file2.cpp") },
        { &dest[1].basic /* file2.cpp */, path_create("file3.cpp") },
        { &dest[2].basic /* file3.cpp */, path_create("file4.cpp") },
        { &dest[3].basic /* file4.cpp */, path_create("file5.cpp") },
      };
      std::vector<bulk_rename::collision_1> expected = {};

      auto actual = find_collisions_1(dest, input_renames);

      ntest::assert_stdvec(expected, actual);
    }

    {
      std::vector<explorer_window::dir_ent> dest = {
        { create_basic_dirent("file1.cpp", ent_kind::file), false, selected },
        { create_basic_dirent("file2.cpp", ent_kind::file), false, selected },
        { create_basic_dirent("file3.cpp", ent_kind::file), false, selected },
        { create_basic_dirent("file4.cpp", ent_kind::file), false, not_selected },
      };
      std::vector<bulk_rename_op> input_renames = {
        { &dest[0].basic /* file1.cpp */, path_create("file2.cpp") },
        { &dest[1].basic /* file2.cpp */, path_create("file3.cpp") },
        { &dest[2].basic /* file3.cpp */, path_create("file4.cpp") },
      };
      std::vector<bulk_rename::collision_1> expected = {
        { &dest[3].basic /* file4.cpp */, input_renames[2] /* file3.cpp -> file4.cpp */ },
      };

      auto actual = find_collisions_1(dest, input_renames);

      ntest::assert_stdvec(expected, actual);
    }
  }
  #endif

  // bulk_rename_sort_renames_dup_elem_sequences_after_non_dups;
  #if 1
  {
    {
      std::vector<bulk_rename_op> input_renames = {};
      sort_renames_dup_elem_sequences_after_non_dups(input_renames);
      ntest::assert_stdvec({}, input_renames);
    }

    {
      std::vector<bulk_rename_op> input_renames = {
        { nullptr, path_create("file1") },
      };
      sort_renames_dup_elem_sequences_after_non_dups(input_renames);
      ntest::assert_stdvec({
        { nullptr, path_create("file1") },
      }, input_renames);
    }

    {
      std::vector<bulk_rename_op> input_renames = {
        { nullptr, path_create("file2") },
        { nullptr, path_create("file2") },
      };
      sort_renames_dup_elem_sequences_after_non_dups(input_renames);
      ntest::assert_stdvec({
        { nullptr, path_create("file2") },
        { nullptr, path_create("file2") },
      }, input_renames);
    }

    {
      std::vector<bulk_rename_op> input_renames = {
        { nullptr, path_create("file3") },
        { nullptr, path_create("file3") },
        { nullptr, path_create("file3") },
      };
      sort_renames_dup_elem_sequences_after_non_dups(input_renames);
      ntest::assert_stdvec({
        { nullptr, path_create("file3") },
        { nullptr, path_create("file3") },
        { nullptr, path_create("file3") },
      }, input_renames);
    }

    {
      std::vector<bulk_rename_op> input_renames = {
        { nullptr, path_create("file1") },
        { nullptr, path_create("file2") },
      };
      sort_renames_dup_elem_sequences_after_non_dups(input_renames);
      ntest::assert_stdvec({
        { nullptr, path_create("file2") },
        { nullptr, path_create("file1") },
      }, input_renames);
    }

    {
      std::vector<bulk_rename_op> input_renames = {
        { nullptr, path_create("file1") },
        { nullptr, path_create("file2") },
        { nullptr, path_create("file3") },
      };
      sort_renames_dup_elem_sequences_after_non_dups(input_renames);
      ntest::assert_stdvec({
        { nullptr, path_create("file3") },
        { nullptr, path_create("file2") },
        { nullptr, path_create("file1") },
      }, input_renames);
    }

    {
      std::vector<bulk_rename_op> input_renames = {
        { nullptr, path_create("file4") },
        { nullptr, path_create("file5") },
        { nullptr, path_create("file6") },
      };
      sort_renames_dup_elem_sequences_after_non_dups(input_renames);
      ntest::assert_stdvec({
        { nullptr, path_create("file6") },
        { nullptr, path_create("file5") },
        { nullptr, path_create("file4") },
      }, input_renames);
    }

    {
      std::vector<bulk_rename_op> input_renames = {
        { nullptr, path_create("file1") },
        { nullptr, path_create("file2") },
        { nullptr, path_create("file3") },
        { nullptr, path_create("file4") },
      };
      sort_renames_dup_elem_sequences_after_non_dups(input_renames);
      ntest::assert_stdvec({
        { nullptr, path_create("file4") },
        { nullptr, path_create("file3") },
        { nullptr, path_create("file2") },
        { nullptr, path_create("file1") },
      }, input_renames);
    }

    {
      std::vector<bulk_rename_op> input_renames = {
        { nullptr, path_create("apple") },
        { nullptr, path_create("banana") },
        { nullptr, path_create("cherry") },
        { nullptr, path_create("apple") },
        { nullptr, path_create("banana") },
        { nullptr, path_create("date") },
      };
      sort_renames_dup_elem_sequences_after_non_dups(input_renames);
      ntest::assert_stdvec({
        { nullptr, path_create("date") },
        { nullptr, path_create("cherry") },
        { nullptr, path_create("banana") },
        { nullptr, path_create("banana") },
        { nullptr, path_create("apple") },
        { nullptr, path_create("apple") },
      }, input_renames);
    }
  }
  #endif

  // bulk_rename_find_collisions_2;
  #if 1
  {
    using ent_kind = basic_dirent::kind;

    auto create_basic_dirent = [](char const *path, ent_kind type) -> basic_dirent {
      return {
        0, // size
        {}, // creation_time_raw
        {}, // last_write_time_raw
        {}, // id
        type,
        path_create(path)
      };
    };

    bool selected = true;
    bool not_selected = false;

    {
      std::vector<explorer_window::dirent> dest = {};
      std::vector<bulk_rename_op> input_renames = {};
      std::vector<bulk_rename_collision> expected_collisions = {};

      auto actual_collisions = bulk_rename_find_collisions(dest, input_renames);

      ntest::assert_stdvec(expected_collisions, actual_collisions);
    }

    {
      std::vector<explorer_window::dirent> dest = {
        { create_basic_dirent("file1.cpp", ent_kind::file), false, not_selected },
        { create_basic_dirent("file2.cpp", ent_kind::file), false, not_selected },
        { create_basic_dirent("file3.cpp", ent_kind::file), false, not_selected },
        { create_basic_dirent("file4.cpp", ent_kind::file), false, not_selected },
      };
      std::vector<bulk_rename_op> input_renames = {
        // none
      };
      std::vector<bulk_rename_collision> expected_collisions = {
        // none
      };

      auto actual_collisions = bulk_rename_find_collisions(dest, input_renames);

      ntest::assert_stdvec(expected_collisions, actual_collisions);
    }

    {
      std::vector<explorer_window::dirent> dest = {
        { create_basic_dirent("file0", ent_kind::file), false, selected },
        { create_basic_dirent("file1", ent_kind::file), false, selected },
        { create_basic_dirent("file2", ent_kind::file), false, selected },
        { create_basic_dirent("file3", ent_kind::file), false, selected },
      };
      std::vector<bulk_rename_op> input_renames = {
        { &dest[0].basic, path_create("file1") },
        { &dest[1].basic, path_create("file2") },
        { &dest[2].basic, path_create("file3") },
        { &dest[3].basic, path_create("file4") },
      };
      std::vector<bulk_rename_collision> expected_collisions = {
        // none
      };

      auto actual_collisions = bulk_rename_find_collisions(dest, input_renames);

      ntest::assert_stdvec(expected_collisions, actual_collisions);
    }

    {
      std::vector<explorer_window::dirent> dest = {
        { create_basic_dirent("file0", ent_kind::file), false, selected },
        { create_basic_dirent("file1", ent_kind::file), false, selected },
        { create_basic_dirent("file2", ent_kind::file), false, selected },
        { create_basic_dirent("file3", ent_kind::file), false, not_selected },
      };
      std::vector<bulk_rename_op> input_renames = {
        { &dest[0].basic, path_create("file1") },
        { &dest[1].basic, path_create("file2") },
        { &dest[2].basic, path_create("file3") },
        //! NOTE: this will get sorted in bulk_rename_find_collisions_2, order to check against will be:
        // file3
        // file2
        // file1
      };
      std::vector<bulk_rename_collision> expected_collisions = {
        { &dest[3].basic, 0, 0 },
      };

      auto actual_collisions = bulk_rename_find_collisions(dest, input_renames);

      ntest::assert_stdvec(expected_collisions, actual_collisions);
    }

    {
      std::vector<explorer_window::dirent> dest = {
        { create_basic_dirent("file0", ent_kind::file), false, selected },
        { create_basic_dirent("file1", ent_kind::file), false, selected },
        { create_basic_dirent("file2", ent_kind::file), false, selected },
        { create_basic_dirent("file3", ent_kind::file), false, not_selected },
      };
      std::vector<bulk_rename_op> input_renames = {
        { &dest[0].basic, path_create("file3") },
        { &dest[1].basic, path_create("file3") },
        { &dest[2].basic, path_create("file3") },
      };
      std::vector<bulk_rename_collision> expected_collisions = {
        { &dest[3].basic, 0, 2 },
      };

      auto actual_collisions = bulk_rename_find_collisions(dest, input_renames);

      ntest::assert_stdvec(expected_collisions, actual_collisions);
    }

    {
      std::vector<explorer_window::dirent> dest = {
        { create_basic_dirent(".cpp", ent_kind::file), false, selected },
        { create_basic_dirent(".cpp", ent_kind::file), false, selected },
        { create_basic_dirent(".cpp", ent_kind::file), false, selected },
        { create_basic_dirent(".cpp", ent_kind::file), false, not_selected },
      };
      std::vector<bulk_rename_op> input_renames = {
        { &dest[0].basic, path_create("file5") },
        { &dest[1].basic, path_create("file5") },
        { &dest[2].basic, path_create("file5") },
      };
      std::vector<bulk_rename_collision> expected_collisions = {
        { nullptr, 0, 2 },
      };

      auto actual_collisions = bulk_rename_find_collisions(dest, input_renames);

      ntest::assert_stdvec(expected_collisions, actual_collisions);
    }

    {
      std::vector<explorer_window::dirent> dest = {
        { create_basic_dirent("file0", ent_kind::file), false, selected },
        { create_basic_dirent("file1", ent_kind::file), false, selected },
        { create_basic_dirent("file2", ent_kind::file), false, selected },
        { create_basic_dirent("file3", ent_kind::file), false, not_selected },
        { create_basic_dirent("file4", ent_kind::file), false, not_selected },
        { create_basic_dirent("file5", ent_kind::file), false, not_selected },
      };
      std::vector<bulk_rename_op> input_renames = {
        { &dest[0].basic, path_create("file3") },
        { &dest[1].basic, path_create("file4") },
        { &dest[2].basic, path_create("file5") },
        //! NOTE: this will get sorted in bulk_rename_find_collisions_2, order to check against will be:
        // file5
        // file4
        // file3
      };
      std::vector<bulk_rename_collision> expected_collisions = {
        { &dest[5].basic, 0, 0 },
        { &dest[4].basic, 1, 1 },
        { &dest[3].basic, 2, 2 },
      };

      auto actual_collisions = bulk_rename_find_collisions(dest, input_renames);

      ntest::assert_stdvec(expected_collisions, actual_collisions);
    }

    {
      std::vector<explorer_window::dirent> dest = {
        { create_basic_dirent("file0", ent_kind::file), false, selected },
        { create_basic_dirent("file1", ent_kind::file), false, selected },
        { create_basic_dirent("file2", ent_kind::file), false, not_selected },
      };
      std::vector<bulk_rename_op> input_renames = {
        { &dest[0].basic, path_create("file2") },
        { &dest[1].basic, path_create("file2") },
      };
      std::vector<bulk_rename_collision> expected_collisions = {
        { &dest[2].basic, 0, 1 },
      };

      auto actual_collisions = bulk_rename_find_collisions(dest, input_renames);

      ntest::assert_stdvec(expected_collisions, actual_collisions);
    }

    {
      std::vector<explorer_window::dirent> dest = {
        { create_basic_dirent("file0", ent_kind::file), false, selected },
        { create_basic_dirent("file1", ent_kind::file), false, not_selected },
        { create_basic_dirent("file2", ent_kind::file), false, selected },
        { create_basic_dirent("file3", ent_kind::file), false, not_selected },
        { create_basic_dirent("file4", ent_kind::file), false, selected },
      };
      std::vector<bulk_rename_op> input_renames = {
        { &dest[0].basic, path_create("file1") },
        { &dest[2].basic, path_create("fileX") },
        { &dest[4].basic, path_create("file3") },
        //! NOTE: this will get sorted in bulk_rename_find_collisions_2, order to check against will be:
        // fileX
        // file3
        // file1
      };
      std::vector<bulk_rename_collision> expected_collisions = {
        { &dest[3].basic, 1, 1 },
        { &dest[1].basic, 2, 2 },
      };

      auto actual_collisions = bulk_rename_find_collisions(dest, input_renames);

      ntest::assert_stdvec(expected_collisions, actual_collisions);
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
