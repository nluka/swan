#ifndef NLUKA_TERM_HPP
#define NLUKA_TERM_HPP

#include <cstdint>
#include <string>

/// Functions for doing fancy terminal stuff via ANSI escape sequences.
namespace term {

  void clear_screen();
  void clear_current_line();
  void clear_to_end_of_line();

  void hide_cursor();
  void unhide_cursor();
  void disable_cursor_blink();

  void move_cursor_to(size_t col, size_t row);
  void move_cursor_up(size_t lines);
  void move_cursor_down(size_t lines);
  void move_cursor_right(size_t cols);
  void move_cursor_left(size_t cols);
  void save_cursor_position();
  void restore_cursor_position();

  typedef uint64_t font_effects_t;
  static_assert(sizeof(font_effects_t) == 8);

  font_effects_t const BOLD          = (uint64_t(1) << 0);
  font_effects_t const UNDERLINE     = (uint64_t(1) << 1);
  font_effects_t const REVERSE_VIDEO = (uint64_t(1) << 2);
  font_effects_t const CROSSED_OUT   = (uint64_t(1) << 3);

  font_effects_t const FG_BLACK   = (uint64_t(1) << 8);
  font_effects_t const FG_RED     = (uint64_t(1) << 9);
  font_effects_t const FG_GREEN   = (uint64_t(1) << 10);
  font_effects_t const FG_YELLOW  = (uint64_t(1) << 11);
  font_effects_t const FG_BLUE    = (uint64_t(1) << 12);
  font_effects_t const FG_MAGENTA = (uint64_t(1) << 13);
  font_effects_t const FG_CYAN    = (uint64_t(1) << 14);
  font_effects_t const FG_WHITE   = (uint64_t(1) << 15);

  font_effects_t const FG_BRIGHT_BLACK   = (uint64_t(1) << 16);
  font_effects_t const FG_BRIGHT_RED     = (uint64_t(1) << 17);
  font_effects_t const FG_BRIGHT_GREEN   = (uint64_t(1) << 18);
  font_effects_t const FG_BRIGHT_YELLOW  = (uint64_t(1) << 19);
  font_effects_t const FG_BRIGHT_BLUE    = (uint64_t(1) << 20);
  font_effects_t const FG_BRIGHT_MAGENTA = (uint64_t(1) << 21);
  font_effects_t const FG_BRIGHT_CYAN    = (uint64_t(1) << 22);
  font_effects_t const FG_BRIGHT_WHITE   = (uint64_t(1) << 23);

  font_effects_t const BG_BLACK   = (uint64_t(1) << 24);
  font_effects_t const BG_RED     = (uint64_t(1) << 25);
  font_effects_t const BG_GREEN   = (uint64_t(1) << 26);
  font_effects_t const BG_YELLOW  = (uint64_t(1) << 27);
  font_effects_t const BG_BLUE    = (uint64_t(1) << 28);
  font_effects_t const BG_MAGENTA = (uint64_t(1) << 29);
  font_effects_t const BG_CYAN    = (uint64_t(1) << 30);
  font_effects_t const BG_WHITE   = (uint64_t(1) << 31);

  font_effects_t const BG_BRIGHT_BLACK   = (uint64_t(1) << 32);
  font_effects_t const BG_BRIGHT_RED     = (uint64_t(1) << 33);
  font_effects_t const BG_BRIGHT_GREEN   = (uint64_t(1) << 34);
  font_effects_t const BG_BRIGHT_YELLOW  = (uint64_t(1) << 35);
  font_effects_t const BG_BRIGHT_BLUE    = (uint64_t(1) << 36);
  font_effects_t const BG_BRIGHT_MAGENTA = (uint64_t(1) << 37);
  font_effects_t const BG_BRIGHT_CYAN    = (uint64_t(1) << 38);
  font_effects_t const BG_BRIGHT_WHITE   = (uint64_t(1) << 39);

  std::string &compute_font_effects_str(font_effects_t const effects, std::string &out);
  std::string const &set_font_effects(font_effects_t const effects);
  void reset_font_effects();

  /// Wrapper for `printf` enabling stylish printing.
  void printf(font_effects_t effects, char const *fmt, ...);

  // font_effects_t foreground_rgb(uint8_t r, uint8_t g, uint8_t b);
  // font_effects_t background_rgb(uint8_t r, uint8_t g, uint8_t b);

} // namespace term

#endif // NLUKA_TERM_HPP
