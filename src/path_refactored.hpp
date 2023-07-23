#ifndef SWAN_PATH_HPP
#define SWAN_PATH_HPP

#include <cassert>
#include <array>

#include <shlwapi.h>

#include "primitives.hpp"

struct path
{
  private:
    std::array<char, MAX_PATH> m_content = {};
    u16 m_len = 0;

  public:

    /// @return A const pointer to the path's character data.
    [[nodiscard]] char const *c_str() const noexcept(true)
    {
      return m_content.data();
    }

    /// @return A mutable pointer to the path's character data.
    [[nodiscard]] char *data() noexcept(true)
    {
      return m_content.data();
    }

    /// @return The current length of the path, excluding NUL.
    [[nodiscard]] u16 size() const noexcept(true)
    {
      return m_len;
    }

    void set_size(u16 size) noexcept(true)
    {
      m_len = size;
    }

    /// @return The maximum number of characters (excluding NUL) the path can fit.
    [[nodiscard]] u16 max_size() const noexcept(true)
    {
      return u16(m_content.max_size() - 1);
    }

    /// @brief Creates a path object.
    /// @param data Starting data. If null or empty, path begins blank.
    /// @param dir_separator The desired directory separator. Non-conforming separators in `data` will be converted.
    /// @return The created path object.
    static [[nodiscard]] path create(char const *data, char dir_separator) noexcept(true)
    {
      path p = {};

      if (data == nullptr) {
        return p;
      }

      u16 data_len = (u16)strnlen(data, UINT16_MAX);

      if (data_len >= p.m_content.max_size()) {
          return p;
      }

      strcpy(p.m_content.data(), data);

      p.m_len = data_len;

      p.convert_separators(dir_separator);

      return p;
    }

    [[nodiscard]] path create_copy() const noexcept(true)
    {
      path copy;
      strncpy(copy.m_content.data(), m_content.data(), m_len);
      copy.m_len = m_len;
      return copy;
    }

    /// @brief Converts all directory separators to a desired one.
    /// @param dir_separator the desired directory separator.
    void convert_separators(char dir_separator) noexcept(true)
    {
      assert(dir_separator == '\\' || dir_separator == '/');

      for (u64 i = 0; i < m_len; ++i) {
          if (strchr("\\/", m_content[i])) {
              m_content[i] = dir_separator;
          }
      }
    }

    /// @brief Query whether the path is empty or not.
    /// @return True if empty, false if not empty.
    [[nodiscard]] bool is_empty() const noexcept(true)
    {
      return m_len == 0;
    }

    /// @brief Truncates the path into an empty string, i.e. "".
    void clear() noexcept(true)
    {
      m_content[0] = '\0';
      m_len = 0;
    }

    /// @return The last character if path not empty, NUL character if path empty.
    char back() const noexcept(true)
    {
      return m_len == 0 ? '\0' : m_content[m_len - 1];
    }

    /// @param chars The string of characters to test last character of path against.
    /// @return True if path's last character is any of `chars`, false otherwise.
    [[nodiscard]] bool ends_with_any_of(char const *chars) const noexcept(true)
    {
      if (chars == nullptr || strlen(chars) == 0) {
        return false;
      }

      if (m_len == 0) {
          return false;
      }
      else {
          char last_ch = this->back();
          return strchr(chars, last_ch);
      }
    }

    /// @param end The string to test the end against.
    /// @return True if the path ends with `end` (case and separator sensitive comparison), false otherwise.
    [[nodiscard]] bool ends_with_exactly(char const *end) noexcept(true)
    {
      if (end == nullptr || m_len == 0) {
        return false;
      }

      u64 end_len = strlen(end);

      if (end_len > m_len) {
        return false;
      }
      else {
        return strcmp(m_content.data() + (m_len - end_len), end) == 0;
      }
    }

    /// @brief Tries to append `str` to the path.
    /// @param str The string to append.
    /// @param dir_separator The desired directory separator. Any existing separators in path or `str` are converted.
    /// @param prepend_sep If true, `dir_separator` will be prepended before `str`, if there isn't one already at the end of the path.
    /// @param postpend_sep If true, `dir_separator` will be appended after `str`, if there isn't one already at the end of `str`.
    /// @return The number of characters appended. 0 indicates failure, non-zero guarantees complete and successful append.
    [[nodiscard]] u16 append(
      char const *str,
      char dir_separator,
      bool prepend_sep = false,
      bool postpend_sep = false) noexcept(true)
    {
      if (str == nullptr) {
        return 0;
      }

      u64 str_len = strlen(str);

      if (str_len == 0) {
        return 0;
      }

      u16 space_left = this->max_size() - m_len;

      static_assert(u8(false) == 0);
      static_assert(u8(true) == 1);
      u64 desired_append_len = str_len + u8(prepend_sep) + u8(postpend_sep);

      bool can_fit = space_left >= desired_append_len;

      if (!can_fit) {
        return 0;
      }

      char const separator_buf[] = { dir_separator, '\0' };

      if (prepend_sep && !this->back() == dir_separator) {
        (void) strncat(m_content.data(), separator_buf, 1);
      }

      (void) strncat(m_content.data(), str, str_len);

      if (postpend_sep) {
        (void) strncat(m_content.data(), separator_buf, 1);
      }

      return (u16)desired_append_len;
    }

    /// @brief Pops the last character from end of path. Does nothing if path empty.
    /// @return The character popped, or NUL if path empty.
    char pop_back() noexcept(true)
    {
      if (m_len > 0) {
        char what_got_popped = this->back();
        m_content[m_len - 1] = '\0';
        return what_got_popped;
      }
      else {
        return '\0';
      }
    }

    /// @brief Pops the last character from end of path if == `test_ch`. Does nothing if path empty.
    /// @return True if the character was popped, false if last character != `test_ch` or path was empty.
    bool pop_back_if(char test_ch) noexcept(true)
    {
      if (m_len > 0 && (this->back() == test_ch)) {
        (void) this->pop_back();
        return true;
      } else {
        return false;
      }
    }

    /// @brief Pops the last character from end of path if != `test_ch`. Does nothing if path empty.
    /// @return True if the character was popped, false if last character == `test_ch` or path was empty.
    bool pop_back_if_not(char test_ch) noexcept(true)
    {
      if (m_len > 0 && (this->back() != test_ch)) {
        (void) this->pop_back();
        return true;
      } else {
        return false;
      }
    }

    /// @brief Loosely compares this path to `other`.
    /// Case insensitive.
    /// Trailing-separator agnostic.
    /// @return True if paths are loosely equal, false otherwise.
    bool loosely_equals(path const &other) const noexcept(true)
    {
      u16 this_len = m_len;
      u16 other_len = other.size();
      i32 len_diff = +(this_len - other_len);

      if (len_diff <= 1) {
        return StrCmpNIA(
          this->m_content.data(),
          other.m_content.data(),
          min(this_len, other_len)
        ) == 0;
      }
      else {
        return false;
      }
    }
};

#endif // SWAN_PATH_HPP
