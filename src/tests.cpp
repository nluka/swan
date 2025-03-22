#include "stdafx.hpp"
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
            swan_path p1 = path_create("E:\\dev\\");
            swan_path p2 = path_create("E:\\dev");
            ntest::assert_bool(true, path_loosely_same(p1, p2));
            ntest::assert_bool(true, path_loosely_same(p2, p1));
        }
        {
            swan_path p1 = path_create("E:/dev/");
            swan_path p2 = path_create("E:/dev/");
            ntest::assert_bool(true, path_loosely_same(p1, p2));
            ntest::assert_bool(true, path_loosely_same(p2, p1));
        }
        {
            swan_path p1 = path_create("E:/dev///");
            swan_path p2 = path_create("E:/dev/");
            ntest::assert_bool(true, path_loosely_same(p1, p2));
            ntest::assert_bool(true, path_loosely_same(p2, p1));
        }
        {
            swan_path p1 = path_create("E:/dev///");
            swan_path p2 = path_create("E:/dev//");
            ntest::assert_bool(true, path_loosely_same(p1, p2));
            ntest::assert_bool(true, path_loosely_same(p2, p1));
        }
        {
            swan_path p1 = path_create("E:/dev///");
            swan_path p2 = path_create("E:/dev//");
            ntest::assert_bool(true, path_loosely_same(p1, p2));
            ntest::assert_bool(true, path_loosely_same(p2, p1));
        }

        {
            swan_path p1 = path_create("E:/dev///");
            swan_path p2 = path_create("E:/de");
            ntest::assert_bool(false, path_loosely_same(p1, p2));
            ntest::assert_bool(false, path_loosely_same(p2, p1));
        }
        {
            swan_path p1 = path_create("E:\\dev");
            swan_path p2 = path_create("E:\\");
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

    // temp_filename_extension_splitter;
    #if 1
    {
        {
            char name[] = "src/swan.cpp";
            temp_filename_extension_splitter sut(name);
            ntest::assert_cstr("swan", sut.name);
            ntest::assert_cstr("cpp", sut.ext);
        }
        {
            char name[] = "E:/dev/swan/src/explorer_window.cpp";
            temp_filename_extension_splitter sut(name);
            ntest::assert_cstr("explorer_window", sut.name);
            ntest::assert_cstr("cpp", sut.ext);
        }
        {
            char name[] = "a.b.c";
            temp_filename_extension_splitter sut(name);
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

    // rename (C stdlib) - confirmed that it doesn't work with Unicode
    #if 0
    {
        auto path1 = output_path / "Юникод 1";
        auto path2 = output_path / "Юникод 2";
        auto path1_str = path1.string();
        auto path2_str = path2.string();

        ntest::assert_bool(true, std::filesystem::create_directory(path1));

        if (ntest::assert_int32(0, rename(path1_str.c_str(), path2_str.c_str()))) {
            if (ntest::assert_bool(true, std::filesystem::exists(path2_str.c_str()))) {
                ntest::assert_int32(0, rename(path2_str.c_str(), path2_str.c_str()));
                ntest::assert_bool(true, std::filesystem::exists(path1));
            }
        }
    }
    #endif

    // bulk_rename_transform::execute, bulk_rename_transform::revert
    #if 1
    {
        bulk_rename_transform transform(basic_dirent::kind::directory, "Юникод-before", "Юникод-after");
        transform.stat.store(bulk_rename_transform::status::ready);

        std::wstring working_dir = output_path.wstring();
        std::replace(working_dir.begin(), working_dir.end(), L'/', L'\\');
        if (!working_dir.ends_with(L'\\')) working_dir += L'\\';

        std::wstring before, after;

        if (ntest::assert_stdstr( "", transform.execute(working_dir.c_str(), before, after) )) {            // execute [before]->[after]
            if (ntest::assert_bool( true, std::filesystem::exists(after.c_str()) )) {                       // check [after] exists
                transform.stat.store(bulk_rename_transform::status::execute_success);
                if (ntest::assert_stdstr( "", transform.revert(working_dir.c_str(), before, after) )) {     // revert [before]<-[after]
                    ntest::assert_bool( true, std::filesystem::exists(before.c_str()) );                    // check [before] exists
                }
            }
        }
    }
    #endif

    //
    #if 1
    {

    }
    #endif

    return ntest::generate_report("swan_tests", output_path, assertion_callback);
}
catch (std::exception const &) {
    // print_debug_msg("FAILED catch(std::exception) %s", except.what());
    return std::nullopt;
}
catch (...) {
    // print_debug_msg("FAILED catch(...)");
    return std::nullopt;
}
