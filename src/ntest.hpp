#ifndef NLUKA_NTEST_HPP
#define NLUKA_NTEST_HPP

#include <array>
#include <concepts>
#include <fstream>
#include <functional>
#include <source_location>
#include <sstream>
#include <string>
#include <type_traits>
#include <filesystem>

namespace ntest {

struct assertion
{
  // for passed assertions: "type\0expected"
  // for failed assertions: "type\0expected\0actual\0"
  std::string serialized_vals;
  std::source_location loc;

  struct serialized
  {
    char const *type;
    char const *expected;
    char const *actual;
  };

  serialized extract_serialized_values(bool passed) const;
};

namespace config {

  void set_max_str_preview_len(size_t);

  void set_max_arr_preview_len(size_t);

  void set_show_column_numbers(bool);

} // namespace config

namespace concepts {

  template <typename Ty>
  concept comparable_neq = requires(Ty a, Ty b)
  {
    a != b;
  };

  template <typename Ty>
  concept printable = requires(std::ostream os, Ty obj)
  {
    os << obj;
  };

  template <typename Ty>
  concept derives_from_std_exception = requires(Ty) {
    std::derived_from<std::exception, Ty>;
  };

} // namespace concepts

namespace internal {

  template <typename Ty, size_t Length>
  consteval
  size_t lengthof(Ty (&)[Length])
  {
    return Length;
  }

  typedef std::vector<std::pair<char, char>> special_chars_table_t;

  special_chars_table_t const &special_chars_serial_file();
  special_chars_table_t const &special_chars_markdown_preview();

  std::string escape(
    std::string const &string_to_escape,
    special_chars_table_t const &special_chars);

  char const *preview_style();

  size_t max_str_preview_len();

  size_t max_arr_preview_len();

  std::string make_serialized_file_path(std::source_location const &, char const *extension);

  void throw_if_file_not_open(std::fstream const &, char const *path);

  void register_passed_assertion(std::stringstream const &, std::source_location const &);

  void register_failed_assertion(std::stringstream const &, std::source_location const &);

  template <typename Ty>
  requires std::integral<Ty>
  auto normalize_integral_type(auto const val)
  {
    if constexpr (std::is_unsigned_v<Ty>)
      return static_cast<uintmax_t>(val);
    else
      return static_cast<intmax_t>(val);
  }

  template <typename Ty>
  requires concepts::printable<Ty>
  void write_arr_to_file(
    std::string const &path,
    Ty const *const arr,
    size_t const size)
  {
    std::fstream file(path, std::ios::out);
    ntest::internal::throw_if_file_not_open(file, path.c_str());

    std::stringstream element{};

    for (size_t i = 0; i < size; ++i)
    {
      if constexpr (std::is_integral_v<Ty>)
      {
        // some integrals like uint8_t are treated strangely by ostream insertion,
        // so casting must be done
        if constexpr (std::is_unsigned_v<Ty>)
          element << static_cast<uintmax_t>(arr[i]);
        else
          element << static_cast<intmax_t>(arr[i]);
      }
      else
      {
        element << arr[i];
      }

      file << ntest::internal::escape(element.str(), ntest::internal::special_chars_serial_file());
      file << '\n';

      element.clear();
      element.str(std::string());
    }
  }

  template <typename Ty>
  requires concepts::comparable_neq<Ty>
  bool arr_eq(
    Ty const *const a1,
    size_t const a1_size,
    Ty const *const a2,
    size_t const a2_size)
  {
    if (a1_size != a2_size)
      return false;

    for (size_t i = 0; i < a1_size; ++i)
      if (a1[i] != a2[i])
        return false;

    return true;
  }

  template <typename Ty>
  requires concepts::printable<Ty>
  void serialize_arr_preview(
    Ty const *const arr,
    size_t const size,
    std::stringstream &ss)
  {
    std::stringstream preview{};

    preview << "sz=" << size;

    if (size == 0)
      return;

    preview << " __[__ ";

    size_t const max_len = std::min(size, ntest::internal::max_arr_preview_len());
    for (size_t i = 0; i < max_len; ++i)
    {
      preview << "<span style='" << internal::preview_style() << "' title='index " << i << "'>";
      if constexpr (std::is_integral_v<Ty>)
      {
        // some integrals like uint8_t are treated strangely by ostream insertion,
        // so casting must be done
        if constexpr (std::is_unsigned_v<Ty>)
          preview << static_cast<uintmax_t>(arr[i]);
        else
          preview << static_cast<intmax_t>(arr[i]);
      }
      else
      {
        preview << arr[i];
      }
      preview << "</span>, ";
    }

    if (size > max_len)
    {
      preview << " *... " << (size - max_len) << " more* ";
    }
    preview << "__]__";

    ss << ntest::internal::escape(preview.str(), ntest::internal::special_chars_markdown_preview());
  }

  std::string beautify_typeid_name(char const *name);

} // namespace internal

bool assert_bool(
  bool expected,
  bool actual,
  std::source_location loc = std::source_location::current()
);

bool assert_int8(
  int8_t expected,
  int8_t actual,
  std::source_location loc = std::source_location::current()
);

bool assert_uint8(
  uint8_t expected,
  uint8_t actual,
  std::source_location loc = std::source_location::current()
);

bool assert_int16(
  int16_t expected,
  int16_t actual,
  std::source_location loc = std::source_location::current()
);

bool assert_uint16(
  uint16_t expected,
  uint16_t actual,
  std::source_location loc = std::source_location::current()
);

bool assert_int32(
  int32_t expected,
  int32_t actual,
  std::source_location loc = std::source_location::current()
);

bool assert_uint32(
  uint32_t expected,
  uint32_t actual,
  std::source_location loc = std::source_location::current()
);

bool assert_int64(
  int64_t expected,
  int64_t actual,
  std::source_location loc = std::source_location::current()
);

bool assert_uint64(
  uint64_t expected,
  uint64_t actual,
  std::source_location loc = std::source_location::current()
);

struct str_opts
{
  bool escape_special_chars;
};

str_opts default_str_opts();

bool assert_cstr(
  char const *expected,
  char const *actual,
  str_opts const &options = default_str_opts(),
  std::source_location loc = std::source_location::current()
);

bool assert_cstr(
  char const *expected,
  size_t expected_len,
  char const *actual,
  size_t actual_len,
  str_opts const &options = default_str_opts(),
  std::source_location loc = std::source_location::current()
);

bool assert_stdstr(
  std::string const &expected,
  std::string const &actual,
  str_opts const &options = default_str_opts(),
  std::source_location loc = std::source_location::current()
);

template <typename Ty>
requires concepts::comparable_neq<Ty> && concepts::printable<Ty>
bool assert_arr(
  Ty const *const expected,
  size_t const expected_size,
  Ty const *const actual,
  size_t const actual_size,
  std::source_location const loc = std::source_location::current())
{
  if (expected_size > 0 && expected == nullptr) {
    std::stringstream err{};
    err
      << "ntest::assert_arr - 'expected' array size was > 0 but data was nullptr, at "
      << loc.file_name() << ':' << loc.line() << "," << loc.column();
    throw std::runtime_error(err.str());
  }
  if (actual_size > 0 && actual == nullptr) {
    std::stringstream err{};
    err
      << "ntest::assert_arr - 'actual' array size was > 0 but data was nullptr, at "
      << loc.file_name() << ':' << loc.line() << "," << loc.column();
    throw std::runtime_error(err.str());
  }

  bool const passed = ntest::internal::arr_eq(expected, expected_size, actual, actual_size);

  std::stringstream serialized_vals{};
  serialized_vals << ntest::internal::beautify_typeid_name(typeid(Ty).name()) << " []" << '\0';

  if (passed)
  {
    ntest::internal::serialize_arr_preview(expected, expected_size, serialized_vals);
    serialized_vals << '\0';
    ntest::internal::register_passed_assertion(serialized_vals, loc);
  }
  else // failed
  {
    std::string const
      expected_path = internal::make_serialized_file_path(loc, "expected"),
      actual_path = internal::make_serialized_file_path(loc, "actual");

    ntest::internal::write_arr_to_file(expected_path, expected, expected_size);
    ntest::internal::write_arr_to_file(actual_path, actual, actual_size);

    serialized_vals
      << '[' << expected_path << "](" << expected_path << ')' << '\0'
      << '[' << actual_path << "](" << actual_path << ')' << '\0';

    ntest::internal::register_failed_assertion(std::move(serialized_vals), loc);
  }

  return passed;
}

template <typename Ty>
requires concepts::comparable_neq<Ty> && concepts::printable<Ty>
bool assert_stdvec(
  std::vector<Ty> const &expected,
  std::vector<Ty> const &actual,
  std::source_location const loc = std::source_location::current())
{
  bool const passed = ntest::internal::arr_eq(
    expected.data(), expected.size(), actual.data(), actual.size());

  std::stringstream serialized_vals{};
  serialized_vals
    << "std::vector\\<"
    << ntest::internal::beautify_typeid_name(typeid(Ty).name())
    << "\\>" << '\0';

  if (passed)
  {
    ntest::internal::serialize_arr_preview(expected.data(), expected.size(), serialized_vals);
    serialized_vals << '\0';
    ntest::internal::register_passed_assertion(serialized_vals, loc);
  }
  else // failed
  {
    std::string const
      expected_path = internal::make_serialized_file_path(loc, "expected"),
      actual_path = internal::make_serialized_file_path(loc, "actual");

    ntest::internal::write_arr_to_file(expected_path, expected.data(), expected.size());
    ntest::internal::write_arr_to_file(actual_path, actual.data(), actual.size());

    serialized_vals
      << '[' << expected_path << "](" << expected_path << ')' << '\0'
      << '[' << actual_path << "](" << actual_path << ')' << '\0';

    ntest::internal::register_failed_assertion(std::move(serialized_vals), loc);
  }

  return passed;
}

template <typename Ty, size_t Size>
requires concepts::comparable_neq<Ty> && concepts::printable<Ty>
bool assert_stdarr(
  std::array<Ty, Size> const &expected,
  std::array<Ty, Size> const &actual,
  std::source_location const loc = std::source_location::current())
{
  bool const passed = ntest::internal::arr_eq(
    expected.data(), expected.size(), actual.data(), actual.size());

  std::stringstream serialized_vals{};
  serialized_vals
    << "std::array\\<"
    << ntest::internal::beautify_typeid_name(typeid(Ty).name())
    << ", " << Size << "\\>" << '\0';

  if (passed)
  {
    ntest::internal::serialize_arr_preview(expected.data(), expected.size(), serialized_vals);
    serialized_vals << '\0';
    ntest::internal::register_passed_assertion(serialized_vals, loc);
  }
  else // failed
  {
    std::string const
      expected_path = internal::make_serialized_file_path(loc, "expected"),
      actual_path = internal::make_serialized_file_path(loc, "actual");

    ntest::internal::write_arr_to_file(expected_path, expected.data(), expected.size());
    ntest::internal::write_arr_to_file(actual_path, actual.data(), actual.size());

    serialized_vals
      << '[' << expected_path << "](" << expected_path << ')' << '\0'
      << '[' << actual_path << "](" << actual_path << ')' << '\0';

    ntest::internal::register_failed_assertion(std::move(serialized_vals), loc);
  }

  return passed;
}

struct text_file_opts
{
  bool canonicalize_newlines;
};

text_file_opts default_text_file_opts();

bool assert_text_file(
  char const *expected_path,
  char const *actual_path,
  text_file_opts const &options = default_text_file_opts(),
  std::source_location loc = std::source_location::current()
);

bool assert_text_file(
  std::string const &expected_path,
  std::string const &actual_path,
  text_file_opts const &options = default_text_file_opts(),
  std::source_location loc = std::source_location::current()
);

bool assert_text_file(
  std::filesystem::path const &expected,
  std::filesystem::path const &actual,
  text_file_opts const &options = default_text_file_opts(),
  std::source_location loc = std::source_location::current()
);

bool assert_binary_file(
  char const *expected_path,
  char const *actual_path,
  std::source_location loc = std::source_location::current()
);

bool assert_binary_file(
  std::string const &expected_path,
  std::string const &actual_path,
  std::source_location loc = std::source_location::current()
);

bool assert_binary_file(
  std::filesystem::path const &expected,
  std::filesystem::path const &actual,
  std::source_location loc = std::source_location::current()
);

/*
  Asserts that a certain type of exception if thrown by `code_snippet`.
  If the correct exception type is thrown, returns the .what() string of the thrown exception.
  If the wrong exception type is thrown or no exception is thrown, an empty string is returned.
  `ExceptTy` must be derived from std::exception.
*/
template <typename ExceptTy>
requires concepts::derives_from_std_exception<ExceptTy>
[[maybe_unused]] std::string assert_throws(
  std::function<void (void)> const &code_snippet,
  std::source_location const loc = std::source_location::current())
{
  bool threw_correct_except = false;
  bool threw_incorrect_except = false;
  std::string what_str = "";

  try
  {
    code_snippet();
  }
  catch (ExceptTy const &except)
  {
    threw_correct_except = true;
    what_str = except.what();
  }
  catch (...)
  {
    threw_incorrect_except = true;
  }

  std::stringstream serialized_vals{};
  serialized_vals << ntest::internal::beautify_typeid_name(typeid(ExceptTy).name()) << '\0';

  if (threw_correct_except) // passed
  {
    serialized_vals << "to be thrown" << '\0';
    ntest::internal::register_passed_assertion(serialized_vals, loc);
  }
  else if (threw_incorrect_except) // failed
  {
    serialized_vals << "to be thrown" << '\0' << "different exception type was thrown" << '\0';
    ntest::internal::register_failed_assertion(serialized_vals, loc);
  }
  else // failed, didn't throw anything
  {
    serialized_vals << "to be thrown" << '\0' << "nothing was thrown" << '\0';
    ntest::internal::register_failed_assertion(serialized_vals, loc);
  }

  return what_str;
}

struct init_result
{
  size_t num_files_removed;
  size_t num_files_failed_to_remove;
};

init_result init(bool remove_residual_files = true);

struct report_result
{
  size_t num_passes;
  size_t num_fails;
};

report_result generate_report(
  char const *name,
  void (*assertion_callback)(assertion const &, bool) = nullptr);

size_t pass_count();

size_t fail_count();

} // namespace ntest

#endif // NLUKA_NTEST_HPP
