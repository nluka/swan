#include "stdafx.hpp"
#include "libs/ntest.hpp"
#include "common_functions.hpp"

std::optional<ntest::report_result> run_tests(std::filesystem::path const &output_path,
                                              void (*assertion_callback)(ntest::assertion const &, bool)) noexcept
try {
    ntest::config::set_max_arr_preview_len(3);
    ntest::config::set_max_str_preview_len(10);

    {
        std::filesystem::create_directories(output_path);
        [[maybe_unused]] auto const res = ntest::init(output_path);
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
        swan_path p = path_create("Text.");
        ntest::assert_cstr("Text.", p.data());
        path_clear(p);
        ntest::assert_cstr("", p.data());
    }
    #endif

    // path_length;
    #if 1
    {
        {
            swan_path p = {};
            ntest::assert_uint64(0, path_length(p));
        }
        {
            swan_path p = path_create("Text.");
            ntest::assert_cstr("Text.", p.data());
            ntest::assert_uint64(5, path_length(p));
        }
    }
    #endif

    // path_ends_with;
    #if 1
    {
        {
            swan_path p = {};
            ntest::assert_bool(false, path_ends_with(p, ""));
            ntest::assert_bool(false, path_ends_with(p, "text"));
        }
        {
            swan_path p = path_create("Text.");
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
            swan_path p = {};
            ntest::assert_bool(false, path_ends_with_one_of(p, "abc"));
        }
        {
            swan_path p = path_create("Text");
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
            swan_path p = {};

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
            swan_path p = {};

            // std::string temp(swan_path_t::static_capacity, 'x');
            std::string temp(p.max_size() - 1, 'x');

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
            swan_path p = {};
            ntest::assert_bool(true, path_is_empty(p));
        }
        {
            swan_path p = path_create("text");
            ntest::assert_bool(false, path_is_empty(p));
        }
    }
    #endif

    // directory_exists
    #if 1
    {
        ntest::assert_bool(false, directory_exists("not_a_directory"));
    #if DEBUG_MODE
        ntest::assert_bool(true, directory_exists("src"));
    #endif
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
            swan_path p1 = path_create("1/2/3");
            swan_path p2 = path_create("1/2/3");
            ntest::assert_bool(true, path_loosely_same(p1, p2));
            ntest::assert_bool(true, path_loosely_same(p2, p1));
        }
        {
            swan_path p1 = path_create("1/2/3");
            swan_path p2 = path_create("1/2/3/");
            ntest::assert_bool(true, path_loosely_same(p1, p2));
            ntest::assert_bool(true, path_loosely_same(p2, p1));
        }
        {
            swan_path p1 = path_create("C:\\code\\");
            swan_path p2 = path_create("C:\\code");
            ntest::assert_bool(true, path_loosely_same(p1, p2));
            ntest::assert_bool(true, path_loosely_same(p2, p1));
        }
        {
            swan_path p1 = path_create("C:/code/");
            swan_path p2 = path_create("C:/code/");
            ntest::assert_bool(true, path_loosely_same(p1, p2));
            ntest::assert_bool(true, path_loosely_same(p2, p1));
        }
        {
            swan_path p1 = path_create("C:/code///");
            swan_path p2 = path_create("C:/code/");
            ntest::assert_bool(true, path_loosely_same(p1, p2));
            ntest::assert_bool(true, path_loosely_same(p2, p1));
        }
        {
            swan_path p1 = path_create("C:/code///");
            swan_path p2 = path_create("C:/code//");
            ntest::assert_bool(true, path_loosely_same(p1, p2));
            ntest::assert_bool(true, path_loosely_same(p2, p1));
        }
        {
            swan_path p1 = path_create("C:/code///");
            swan_path p2 = path_create("C:/Code//");
            ntest::assert_bool(true, path_loosely_same(p1, p2));
            ntest::assert_bool(true, path_loosely_same(p2, p1));
        }

        {
            swan_path p1 = path_create("C:/code///");
            swan_path p2 = path_create("C:/cod");
            ntest::assert_bool(false, path_loosely_same(p1, p2));
            ntest::assert_bool(false, path_loosely_same(p2, p1));
        }
        {
            swan_path p1 = path_create("C:\\code");
            swan_path p2 = path_create("C:\\");
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

    // bulk_rename_compile_pattern, bulk_rename_transform;
    #if 1
    {
        // bool squish_adjacent_spaces = false;
        swan_path after = {};

        auto assert_failed_compile = [](char const *pattern, char const *expected_error, std::source_location sloc = std::source_location::current()) noexcept {
            auto const [success, compiled, err_msg] = bulk_rename_compile_pattern(pattern, false);
            ntest::assert_bool(false, success, sloc);
            ntest::assert_cstr(expected_error, err_msg.data(), ntest::default_str_opts(), sloc);
            ntest::assert_stdvec({}, compiled.ops, sloc);
        };

        assert_failed_compile("", "empty pattern");
        assert_failed_compile("asdf\t", "illegal filename character 9 at position 4");
        assert_failed_compile("data<<", "unexpected '<' at position 5, unclosed '<' at position 4");
        assert_failed_compile("<", "unclosed '<' at position 0");
        assert_failed_compile(">", "unexpected '>' at position 0 - no preceding '<' found");
        assert_failed_compile("<>", "empty expression starting at position 0");
        assert_failed_compile("data_<bogus>", "unknown expression starting at position 5");
        assert_failed_compile("<,>", "unknown expression starting at position 0");
        assert_failed_compile("<0>", "unknown expression starting at position 0");
        assert_failed_compile("<  >", "unknown expression starting at position 0");
        assert_failed_compile("<123456,>", "unknown expression starting at position 0");
        assert_failed_compile("<1,0>", "slice expression starting at position 0 is malformed, first is greater than last");

        struct compile_and_transform_test_args
        {
            char const *name;
            char const *ext;
            char const *pattern;
            u64 size;
            s32 counter;
            bool squish_adjacent_spaces;
            char const *expected_after;
        };

        auto assert_successful_compile_and_transform = [](compile_and_transform_test_args args, std::source_location sloc = std::source_location::current()) noexcept {
            auto const [success_compile, compiled, err_msg_compile] = bulk_rename_compile_pattern(args.pattern, args.squish_adjacent_spaces);
            ntest::assert_bool(true, success_compile, sloc);
            ntest::assert_cstr("", err_msg_compile.data(), ntest::default_str_opts(), sloc);
            ntest::assert_bool(args.squish_adjacent_spaces, compiled.squish_adjacent_spaces, sloc);

            swan_path after = {};

            auto [success_transform, err_msg_transform] = bulk_rename_transform(compiled, after, args.name, args.ext, args.counter, args.size);
            ntest::assert_bool(true, success_transform, sloc);
            ntest::assert_cstr(args.expected_after, after.data(), ntest::default_str_opts(), sloc);
            ntest::assert_cstr("", err_msg_transform.data(), ntest::default_str_opts(), sloc);
        };

        assert_successful_compile_and_transform({ .name="before", .ext="txt",
                                                  .pattern="<counter>", .size={}, .counter=0, .squish_adjacent_spaces={},
                                                  .expected_after="0" });

        assert_successful_compile_and_transform({ .name="before", .ext="txt",
                                                  .pattern="<counter>", .size={}, .counter=100, .squish_adjacent_spaces={},
                                                  .expected_after="100" });

        assert_successful_compile_and_transform({ .name="BEFORE", .ext="txt",
                                                  .pattern="something_<name>_bla", .size={}, .counter={}, .squish_adjacent_spaces={},
                                                  .expected_after="something_BEFORE_bla" });

        assert_successful_compile_and_transform({ .name="BEFORE", .ext="TXT",
                                                  .pattern="<name>  <bytes>.<ext>", .size=42, .counter={}, .squish_adjacent_spaces=false,
                                                  .expected_after="BEFORE  42.TXT" });

        assert_successful_compile_and_transform({ .name="BEFORE", .ext="TXT",
                                                  .pattern="<name>  <bytes>.<ext>", .size=42, .counter={}, .squish_adjacent_spaces=true,
                                                  .expected_after="BEFORE 42.TXT" });

        assert_successful_compile_and_transform({ .name="before", .ext="txt",
                                                  .pattern="29.  Gladiator Boss", .size={}, .counter={}, .squish_adjacent_spaces=false,
                                                  .expected_after="29.  Gladiator Boss" });

        assert_successful_compile_and_transform({ .name="before", .ext="txt",
                                                  .pattern="29.  Gladiator Boss", .size={}, .counter={}, .squish_adjacent_spaces=true,
                                                  .expected_after="29. Gladiator Boss" });

        assert_successful_compile_and_transform({ .name="before", .ext="txt",
                                                  .pattern="<0,>", .size={}, .counter={}, .squish_adjacent_spaces={},
                                                  .expected_after="before.txt" });

        assert_successful_compile_and_transform({ .name="before", .ext="txt",
                                                  .pattern="<,3>", .size={}, .counter={}, .squish_adjacent_spaces={},
                                                  .expected_after="befo" });

        assert_successful_compile_and_transform({ .name="before", .ext="txt",
                                                  .pattern="<0,0>", .size={}, .counter={}, .squish_adjacent_spaces={},
                                                  .expected_after="b" });

        assert_successful_compile_and_transform({ .name="before", .ext="txt",
                                                  .pattern="<0,5>", .size={}, .counter={}, .squish_adjacent_spaces={},
                                                  .expected_after="before" });

        assert_successful_compile_and_transform({ .name="before", .ext="txt",
                                                  .pattern="<0,6>", .size={}, .counter={}, .squish_adjacent_spaces={},
                                                  .expected_after="before." });

        assert_successful_compile_and_transform({ .name="before", .ext="txt",
                                                  .pattern="<0,7>", .size={}, .counter={}, .squish_adjacent_spaces={},
                                                  .expected_after="before.t" });

        assert_successful_compile_and_transform({ .name="before", .ext="txt",
                                                  .pattern="<0,9>", .size={}, .counter={}, .squish_adjacent_spaces={},
                                                  .expected_after="before.txt" });

        assert_successful_compile_and_transform({ .name="before", .ext="txt",
                                                  .pattern="<9,9>", .size={}, .counter={}, .squish_adjacent_spaces={},
                                                  .expected_after="t" });

        assert_successful_compile_and_transform({ .name="Some Long File Name", .ext="txt",
                                                  .pattern="<5,8>  <15,18>_<counter>_<bytes>.<ext>", .size=42, .counter=21, .squish_adjacent_spaces=true,
                                                  .expected_after="Long Name_21_42.txt" });

        assert_successful_compile_and_transform({ .name="01-01-2023 Report Name", .ext="docx",
                                                  .pattern="<0,10> asdf", .size={}, .counter={}, .squish_adjacent_spaces=true,
                                                  .expected_after="01-01-2023 asdf" });

        auto assert_successful_compile_but_failed_transform = [](compile_and_transform_test_args args, std::source_location sloc = std::source_location::current()) noexcept {
            // using args.expected_after for expected error

            auto const [success_compile, compiled, err_msg_compile] = bulk_rename_compile_pattern(args.pattern, args.squish_adjacent_spaces);
            ntest::assert_bool(true, success_compile, sloc);
            ntest::assert_cstr("", err_msg_compile.data(), ntest::default_str_opts(), sloc);
            ntest::assert_bool(args.squish_adjacent_spaces, compiled.squish_adjacent_spaces, sloc);

            swan_path after = {};

            auto [success_transform, err_msg_transform] = bulk_rename_transform(compiled, after, args.name, args.ext, args.counter, args.size);
            ntest::assert_bool(false, success_transform, sloc);
            ntest::assert_cstr(args.expected_after, err_msg_transform.data(), ntest::default_str_opts(), sloc);
        };

        assert_successful_compile_but_failed_transform({ .name="before", .ext="txt",
                                                         .pattern="<0,10>", .size={}, .counter={}, .squish_adjacent_spaces={},
                                                         .expected_after="slice goes out of bounds" });

        assert_successful_compile_but_failed_transform({ .name="before", .ext="txt",
                                                         .pattern="<10,10>", .size={}, .counter={}, .squish_adjacent_spaces={},
                                                         .expected_after="slice goes out of bounds" });

        assert_successful_compile_but_failed_transform({ .name="before", .ext="txt",
                                                         .pattern="<5,10>", .size={}, .counter={}, .squish_adjacent_spaces={},
                                                         .expected_after="slice goes out of bounds" });

        assert_successful_compile_but_failed_transform({ .name="before", .ext="txt",
                                                         .pattern="<10,20>", .size={}, .counter={}, .squish_adjacent_spaces={},
                                                         .expected_after="slice goes out of bounds" });
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

    // bulk_rename_find_collisions;
    #if 1
    {
        using ent_kind = basic_dirent::kind;

        auto create_basic_dirent = [](char const *path, ent_kind type) -> basic_dirent {
            basic_dirent ent = {};
            ent.type = type;
            ent.path = path_create(path);
            return ent;
        };

        {
            std::vector<explorer_window::dirent> dest = {};
            std::vector<bulk_rename_op> input_renames = {};
            std::vector<bulk_rename_collision> expected_collisions = {};

            auto actual = bulk_rename_find_collisions(dest, input_renames);

            ntest::assert_stdvec(expected_collisions, actual.collisions);
        }

        {
            std::vector<explorer_window::dirent> dest = {
                { .basic = create_basic_dirent("file1.cpp", ent_kind::file), .is_filtered_out = 0, .is_selected = 0 },
                { .basic = create_basic_dirent("file2.cpp", ent_kind::file), .is_filtered_out = 0, .is_selected = 0 },
                { .basic = create_basic_dirent("file3.cpp", ent_kind::file), .is_filtered_out = 0, .is_selected = 0 },
                { .basic = create_basic_dirent("file4.cpp", ent_kind::file), .is_filtered_out = 0, .is_selected = 0 },
            };
            std::vector<bulk_rename_op> input_renames = {
                // none
            };
            std::vector<bulk_rename_collision> expected_collisions = {
                // none
            };

            auto actual = bulk_rename_find_collisions(dest, input_renames);

            ntest::assert_stdvec(expected_collisions, actual.collisions);
        }

        {
            std::vector<explorer_window::dirent> dest = {
                { .basic = create_basic_dirent("file0", ent_kind::file), .is_filtered_out = 0, .is_selected = 1 },
                { .basic = create_basic_dirent("file1", ent_kind::file), .is_filtered_out = 0, .is_selected = 1 },
                { .basic = create_basic_dirent("file2", ent_kind::file), .is_filtered_out = 0, .is_selected = 1 },
                { .basic = create_basic_dirent("file3", ent_kind::file), .is_filtered_out = 0, .is_selected = 1 },
            };
            std::vector<bulk_rename_op> input_renames = {
                { .before = &dest[0].basic, .after = path_create("file1") },
                { .before = &dest[1].basic, .after = path_create("file2") },
                { .before = &dest[2].basic, .after = path_create("file3") },
                { .before = &dest[3].basic, .after = path_create("file4") },
            };
            std::vector<bulk_rename_collision> expected_collisions = {
                // none
            };

            auto actual = bulk_rename_find_collisions(dest, input_renames);

            ntest::assert_stdvec(expected_collisions, actual.collisions);
        }

        {
            std::vector<explorer_window::dirent> dest = {
                { .basic = create_basic_dirent("file0", ent_kind::file), .is_filtered_out = 0, .is_selected = 1 },
                { .basic = create_basic_dirent("file1", ent_kind::file), .is_filtered_out = 0, .is_selected = 1 },
                { .basic = create_basic_dirent("file2", ent_kind::file), .is_filtered_out = 0, .is_selected = 1 },
                { .basic = create_basic_dirent("file3", ent_kind::file), .is_filtered_out = 0, .is_selected = 0 },
            };
            std::vector<bulk_rename_op> input_renames = {
                { .before = &dest[0].basic, .after = path_create("file1") },
                { .before = &dest[1].basic, .after = path_create("file2") },
                { .before = &dest[2].basic, .after = path_create("file3") },
                //! NOTE: this will get sorted in bulk_rename_find_collisions, order to check against will be:
                // file3
                // file2
                // file1
            };
            std::vector<bulk_rename_collision> expected_collisions = {
                { .dest_dirent = &dest[3].basic, .first_rename_pair_idx = 0, .last_rename_pair_idx = 0 },
            };

            auto actual = bulk_rename_find_collisions(dest, input_renames);

            ntest::assert_stdvec(expected_collisions, actual.collisions);
        }

        {
            std::vector<explorer_window::dirent> dest = {
                { .basic = create_basic_dirent("file0", ent_kind::file), .is_filtered_out = 0, .is_selected = 1 },
                { .basic = create_basic_dirent("file1", ent_kind::file), .is_filtered_out = 0, .is_selected = 1 },
                { .basic = create_basic_dirent("file2", ent_kind::file), .is_filtered_out = 0, .is_selected = 1 },
                { .basic = create_basic_dirent("file3", ent_kind::file), .is_filtered_out = 0, .is_selected = 0 },
            };
            std::vector<bulk_rename_op> input_renames = {
                { .before = &dest[0].basic, .after = path_create("file3") },
                { .before = &dest[1].basic, .after = path_create("file3") },
                { .before = &dest[2].basic, .after = path_create("file3") },
            };
            std::vector<bulk_rename_collision> expected_collisions = {
                { .dest_dirent = &dest[3].basic, .first_rename_pair_idx = 0, .last_rename_pair_idx = 2 },
            };

            auto actual = bulk_rename_find_collisions(dest, input_renames);

            ntest::assert_stdvec(expected_collisions, actual.collisions);
        }

        {
            std::vector<explorer_window::dirent> dest = {
                { .basic = create_basic_dirent(".cpp", ent_kind::file), .is_filtered_out = 0, .is_selected = 1 },
                { .basic = create_basic_dirent(".cpp", ent_kind::file), .is_filtered_out = 0, .is_selected = 1 },
                { .basic = create_basic_dirent(".cpp", ent_kind::file), .is_filtered_out = 0, .is_selected = 1 },
                { .basic = create_basic_dirent(".cpp", ent_kind::file), .is_filtered_out = 0, .is_selected = 0 },
            };
            std::vector<bulk_rename_op> input_renames = {
                { .before = &dest[0].basic, .after = path_create("file5") },
                { .before = &dest[1].basic, .after = path_create("file5") },
                { .before = &dest[2].basic, .after = path_create("file5") },
            };
            std::vector<bulk_rename_collision> expected_collisions = {
                { .dest_dirent = nullptr, .first_rename_pair_idx = 0, .last_rename_pair_idx = 2 },
            };

            auto actual = bulk_rename_find_collisions(dest, input_renames);

            ntest::assert_stdvec(expected_collisions, actual.collisions);
        }

        {
            std::vector<explorer_window::dirent> dest = {
                { .basic = create_basic_dirent("file0", ent_kind::file), .is_filtered_out = 0, .is_selected = 1 },
                { .basic = create_basic_dirent("file1", ent_kind::file), .is_filtered_out = 0, .is_selected = 1 },
                { .basic = create_basic_dirent("file2", ent_kind::file), .is_filtered_out = 0, .is_selected = 1 },
                { .basic = create_basic_dirent("file3", ent_kind::file), .is_filtered_out = 0, .is_selected = 0 },
                { .basic = create_basic_dirent("file4", ent_kind::file), .is_filtered_out = 0, .is_selected = 0 },
                { .basic = create_basic_dirent("file5", ent_kind::file), .is_filtered_out = 0, .is_selected = 0 },
            };
            std::vector<bulk_rename_op> input_renames = {
                { .before = &dest[0].basic, .after = path_create("file3") },
                { .before = &dest[1].basic, .after = path_create("file4") },
                { .before = &dest[2].basic, .after = path_create("file5") },
                //! NOTE: this will get sorted in bulk_rename_find_collisions, order to check against will be:
                // file5
                // file4
                // file3
            };
            std::vector<bulk_rename_collision> expected_collisions = {
                { .dest_dirent = &dest[5].basic, .first_rename_pair_idx = 0, .last_rename_pair_idx = 0 },
                { .dest_dirent = &dest[4].basic, .first_rename_pair_idx = 1, .last_rename_pair_idx = 1 },
                { .dest_dirent = &dest[3].basic, .first_rename_pair_idx = 2, .last_rename_pair_idx = 2 },
            };

            auto actual = bulk_rename_find_collisions(dest, input_renames);

            ntest::assert_stdvec(expected_collisions, actual.collisions);
        }

        {
            std::vector<explorer_window::dirent> dest = {
                { .basic = create_basic_dirent("file0", ent_kind::file), .is_filtered_out = 0, .is_selected = 1 },
                { .basic = create_basic_dirent("file1", ent_kind::file), .is_filtered_out = 0, .is_selected = 1 },
                { .basic = create_basic_dirent("file2", ent_kind::file), .is_filtered_out = 0, .is_selected = 0 },
            };
            std::vector<bulk_rename_op> input_renames = {
                { .before = &dest[0].basic, .after = path_create("file2") },
                { .before = &dest[1].basic, .after = path_create("file2") },
            };
            std::vector<bulk_rename_collision> expected_collisions = {
                { .dest_dirent = &dest[2].basic, .first_rename_pair_idx = 0, .last_rename_pair_idx = 1 },
            };

            auto actual = bulk_rename_find_collisions(dest, input_renames);

            ntest::assert_stdvec(expected_collisions, actual.collisions);
        }

        {
            std::vector<explorer_window::dirent> dest = {
                { .basic = create_basic_dirent("file0", ent_kind::file), .is_filtered_out = 0, .is_selected = 1 },
                { .basic = create_basic_dirent("file1", ent_kind::file), .is_filtered_out = 0, .is_selected = 0 },
                { .basic = create_basic_dirent("file2", ent_kind::file), .is_filtered_out = 0, .is_selected = 1 },
                { .basic = create_basic_dirent("file3", ent_kind::file), .is_filtered_out = 0, .is_selected = 0 },
                { .basic = create_basic_dirent("file4", ent_kind::file), .is_filtered_out = 0, .is_selected = 1 },
            };
            std::vector<bulk_rename_op> input_renames = {
                { .before = &dest[0].basic, .after = path_create("file1") },
                { .before = &dest[2].basic, .after = path_create("fileX") },
                { .before = &dest[4].basic, .after = path_create("file3") },
                //! NOTE: this will get sorted in bulk_rename_find_collisions, order to check against will be:
                // fileX
                // file3
                // file1
            };
            std::vector<bulk_rename_collision> expected_collisions = {
                { .dest_dirent = &dest[3].basic, .first_rename_pair_idx = 1, .last_rename_pair_idx = 1 },
                { .dest_dirent = &dest[1].basic, .first_rename_pair_idx = 2, .last_rename_pair_idx = 2 },
            };

            auto actual = bulk_rename_find_collisions(dest, input_renames);

            ntest::assert_stdvec(expected_collisions, actual.collisions);
        }

        {
            std::vector<explorer_window::dirent> dest = {
                { .basic = create_basic_dirent("00b4dP", ent_kind::file), .is_filtered_out = 0, .is_selected = 1 },
                { .basic = create_basic_dirent("00BbVr", ent_kind::file), .is_filtered_out = 0, .is_selected = 1 },
                { .basic = create_basic_dirent("01MWKO", ent_kind::file), .is_filtered_out = 0, .is_selected = 1 },
                { .basic = create_basic_dirent("01Sw7U", ent_kind::file), .is_filtered_out = 0, .is_selected = 1 },
                { .basic = create_basic_dirent("02MugF", ent_kind::file), .is_filtered_out = 0, .is_selected = 1 },
            };
            std::vector<bulk_rename_op> input_renames = {
                { .before = &dest[0].basic, .after = path_create("0b") },
                { .before = &dest[1].basic, .after = path_create("0b") },
                { .before = &dest[2].basic, .after = path_create("0b") },
                { .before = &dest[3].basic, .after = path_create("0b") },
                { .before = &dest[4].basic, .after = path_create("0b") },
                //! NOTE: this will get sorted in bulk_rename_find_collisions
            };
            std::vector<bulk_rename_collision> expected_collisions = {
                { .dest_dirent = nullptr, .first_rename_pair_idx = 0, .last_rename_pair_idx = 4 },
            };

            auto actual = bulk_rename_find_collisions(dest, input_renames);

            ntest::assert_stdvec(expected_collisions, actual.collisions);
        }

        {
            std::vector<explorer_window::dirent> dest = {
                { .basic = create_basic_dirent("bla_bla", ent_kind::file), .is_filtered_out = 0, .is_selected = 1 },
                { .basic = create_basic_dirent("test1",   ent_kind::file), .is_filtered_out = 0, .is_selected = 1 },
                { .basic = create_basic_dirent("test2",   ent_kind::file), .is_filtered_out = 0, .is_selected = 0 },
            };
            std::vector<bulk_rename_op> input_renames = {
                { .before = &dest[0].basic, .after = path_create("bla_2") },
                { .before = &dest[1].basic, .after = path_create("test2") },
                //! NOTE: this will get sorted in bulk_rename_find_collisions
            };
            std::vector<bulk_rename_collision> expected_collisions = {
                { .dest_dirent = &dest[2].basic, .first_rename_pair_idx = 0, .last_rename_pair_idx = 0 },
            };

            auto actual = bulk_rename_find_collisions(dest, input_renames);

            ntest::assert_stdvec(expected_collisions, actual.collisions);
            ntest::assert_cstr(path_create("test1").data(), actual.sorted_renames[0].before->path.data());
            ntest::assert_cstr(path_create("bla_bla").data(), actual.sorted_renames[1].before->path.data());
        }
    }
    #endif

    // file_name_extension_splitter;
    #if 1
    {
        {
            char name[] = "src/swan.cpp";
            file_name_extension_splitter sut(name);
            ntest::assert_cstr("swan", sut.name);
            ntest::assert_cstr("cpp", sut.ext);
        }
        {
            char name[] = "C:/code/swan/src/explorer_window.cpp";
            file_name_extension_splitter sut(name);
            ntest::assert_cstr("explorer_window", sut.name);
            ntest::assert_cstr("cpp", sut.ext);
        }
        {
            char name[] = "a.b.c";
            file_name_extension_splitter sut(name);
            ntest::assert_cstr("a.b", sut.name);
            ntest::assert_cstr("c", sut.ext);
        }
    }
    #endif

    // one_of
    #if 1
    {
        ntest::assert_bool(true, one_of(21, { 0, 21, 22 }));
        ntest::assert_bool(false, one_of(21, { 0, 1, 2 }));
    }
    #endif

    //
    #if 1
    {

    }
    #endif

    return ntest::generate_report("swan_tests", output_path, assertion_callback);
}
catch ([[maybe_unused]] std::exception const &err) {
    return std::nullopt;
}
catch (...) {
    return std::nullopt;
}
