#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
#include <cassert>
#include <stdexcept>

#include "term.hpp"

// super useful references:
// https://stackoverflow.com/questions/4842424/list-of-ansi-color-escape-sequences
// https://www2.math.upenn.edu/~kazdan/210/computer/ansi.html

void term::clear_screen() {
  std::printf("\033[2J");
}

void term::clear_current_line() {
  std::printf("\33[2K\r");
}

void term::clear_to_end_of_line() {
  std::printf("\033[K");
}

void term::hide_cursor() {
  std::printf("\33[?25l");
}

void term::unhide_cursor() {
  std::printf("\33[?25h");
}

void term::disable_cursor_blink() {
  std::printf("\33[?12l");
}

void term::move_cursor_to(size_t const row, size_t const col) {
  std::printf("\33[%zu;%zuH", row, col);
}

void term::move_cursor_up(size_t const n) {
  std::printf("\33[%zuA", n);
}

void term::move_cursor_down(size_t const n) {
  std::printf("\33[%zuB", n);
}

void term::move_cursor_right(size_t const n) {
  std::printf("\33[%zuC", n);
}

void term::move_cursor_left(size_t const n) {
  std::printf("\33[%zuD", n);
}

void term::save_cursor_position() {
  std::printf("\33[s");
}

void term::restore_cursor_position() {
  std::printf("\33[u");
}

std::string &term::compute_font_effects_str(font_effects_t const effects, std::string &out) {
  out.clear();

  auto const append_code_if_effect_set = [&](
    font_effects_t const effect_to_check,
    char const *code
  ) -> size_t {
    if (effects & effect_to_check) {
      out += code;
      out += ';';
      return 1;
    } else {
      return 0;
    }
  };

  {
    size_t num_set = 0;

    num_set += append_code_if_effect_set(BOLD,          "1");
    num_set += append_code_if_effect_set(UNDERLINE,     "4");
    num_set += append_code_if_effect_set(REVERSE_VIDEO, "7");
    num_set += append_code_if_effect_set(CROSSED_OUT,   "9");
  }

  {
    size_t num_set = 0;

    num_set += append_code_if_effect_set(FG_BLACK,   "30");
    num_set += append_code_if_effect_set(FG_RED,     "31");
    num_set += append_code_if_effect_set(FG_GREEN,   "32");
    num_set += append_code_if_effect_set(FG_YELLOW,  "33");
    num_set += append_code_if_effect_set(FG_BLUE,    "34");
    num_set += append_code_if_effect_set(FG_MAGENTA, "35");
    num_set += append_code_if_effect_set(FG_CYAN,    "36");
    num_set += append_code_if_effect_set(FG_WHITE,   "37");

    num_set += append_code_if_effect_set(FG_BRIGHT_BLACK,   "90");
    num_set += append_code_if_effect_set(FG_BRIGHT_RED,     "91");
    num_set += append_code_if_effect_set(FG_BRIGHT_GREEN,   "92");
    num_set += append_code_if_effect_set(FG_BRIGHT_YELLOW,  "93");
    num_set += append_code_if_effect_set(FG_BRIGHT_BLUE,    "94");
    num_set += append_code_if_effect_set(FG_BRIGHT_MAGENTA, "95");
    num_set += append_code_if_effect_set(FG_BRIGHT_CYAN,    "96");
    num_set += append_code_if_effect_set(FG_BRIGHT_WHITE,   "97");

    if (num_set > 1) {
      throw std::runtime_error("multiple foreground colors set, only one can be set");
    }
  }

  {
    size_t num_set = 0;

    num_set += append_code_if_effect_set(BG_BLACK,   "40");
    num_set += append_code_if_effect_set(BG_RED,     "41");
    num_set += append_code_if_effect_set(BG_GREEN,   "42");
    num_set += append_code_if_effect_set(BG_YELLOW,  "43");
    num_set += append_code_if_effect_set(BG_BLUE,    "44");
    num_set += append_code_if_effect_set(BG_MAGENTA, "45");
    num_set += append_code_if_effect_set(BG_CYAN,    "46");
    num_set += append_code_if_effect_set(BG_WHITE,   "47");

    num_set += append_code_if_effect_set(BG_BRIGHT_BLACK,   "100");
    num_set += append_code_if_effect_set(BG_BRIGHT_RED,     "101");
    num_set += append_code_if_effect_set(BG_BRIGHT_GREEN,   "102");
    num_set += append_code_if_effect_set(BG_BRIGHT_YELLOW,  "103");
    num_set += append_code_if_effect_set(BG_BRIGHT_BLUE,    "104");
    num_set += append_code_if_effect_set(BG_BRIGHT_MAGENTA, "105");
    num_set += append_code_if_effect_set(BG_BRIGHT_CYAN,    "106");
    num_set += append_code_if_effect_set(BG_BRIGHT_WHITE,   "107");

    if (num_set > 1) {
      throw std::runtime_error("multiple background colors set, only one can be set");
    }
  }

  if (out.length() > 0)
    out.pop_back(); // remove trailing ;

  return out;
}

std::string const &term::set_font_effects(font_effects_t const effects) {
  static std::string fes(100, '\0');

  compute_font_effects_str(effects, fes);
  std::printf("\033[%sm", fes.c_str());

  return fes;
}

void term::reset_font_effects() {
  std::printf("\033[0m");
}

void term::printf(font_effects_t const effects, char const *fmt, ...) {
  term::set_font_effects(effects);

  va_list args;
  va_start(args, fmt);
  vprintf(fmt, args);
  va_end(args);

  term::reset_font_effects();
}

// And if I ever get around to implementing RGB colors:

// struct rgb {
//   uint8_t r;
//   uint8_t g;
//   uint8_t b;
// };

// term::font_effects_t term::foreground_rgb(uint8_t const r, uint8_t const g, uint8_t const b) {
//   uint8_t data[8]{};

//   data[0] = r;
//   data[1] = g;
//   data[2] = b;

//   font_effects_t retval;
//   std::memcpy(&retval, data, 8);
//   return retval;
// }

// term::font_effects_t term::background_rgb(uint8_t const r, uint8_t const g, uint8_t const b) {
//   uint8_t data[8]{};

//   data[3] = r;
//   data[4] = g;
//   data[5] = b;

//   font_effects_t retval;
//   std::memcpy(&retval, data, 8);
//   return retval;
// }

#if 0
  uint8_t const *const effects_data = reinterpret_cast<uint8_t const *>(&effects);

  // foreground
  {
    uint8_t const r = static_cast<uint8_t>(effects_data[0]);
    uint8_t const g = static_cast<uint8_t>(effects_data[1]);
    uint8_t const b = static_cast<uint8_t>(effects_data[2]);
    char buffer[17];
    std::sprintf(buffer, "38;2;%d;%d;%d", r, g, b);
    out += buffer;
  }

  // background
  {
    uint8_t const r = static_cast<uint8_t>(effects_data[3]);
    uint8_t const g = static_cast<uint8_t>(effects_data[4]);
    uint8_t const b = static_cast<uint8_t>(effects_data[5]);
    char buffer[17];
    std::sprintf(buffer, "48;2;%d;%d;%d", r, g, b);
    out += buffer;
  }
#endif
