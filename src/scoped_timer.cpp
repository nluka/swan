#ifndef NLUKA_SCOPEDTIMER_HPP
#define NLUKA_SCOPEDTIMER_HPP

#include <chrono>
#include <cstdint>
#include <ostream>
#include <string_view>

namespace timer_unit
{
  typedef uint32_t value_type;

  value_type constexpr
    SECONDS      = 1'000'000'000,
    MILLISECONDS = 1'000'000,
    MICROSECONDS = 1'000,
    NANOSECONDS  = 1;
};

template <timer_unit::value_type TimerUnit>
class scoped_timer
{
  public:
    scoped_timer(char const *const label, double *elapsed_time_out = nullptr, std::ostream *os = nullptr)
    : m_label{label ? label : "unnamed timer"},
      m_os{os},
      m_elapsed_time_out{elapsed_time_out},
      m_start{std::chrono::steady_clock::now()}
    {}

    scoped_timer(scoped_timer const &) = delete; // copy constructor
    scoped_timer &operator=(scoped_timer const &) = delete; // copy assignment
    scoped_timer(scoped_timer &&) noexcept = delete; // move constructor
    scoped_timer &operator=(scoped_timer &&) noexcept = delete; // move assignment

    ~scoped_timer() {
      auto const end = std::chrono::steady_clock::now();
      auto const elapsed_nanos = end - m_start;
      auto const elapsed_time_in_units = (double)elapsed_nanos.count() / (double)TimerUnit;

      if (m_elapsed_time_out) {
        *m_elapsed_time_out = elapsed_time_in_units;
      }

      if (m_os) {
        char const *unit_cstr;
        switch (TimerUnit) {
          case timer_unit::SECONDS:      unit_cstr = "s"; break;
          case timer_unit::MILLISECONDS: unit_cstr = "ms"; break;
          case timer_unit::MICROSECONDS: unit_cstr = "us"; break;
          case timer_unit::NANOSECONDS:  unit_cstr = "ns"; break;
          default:                              unit_cstr = nullptr; break;
        }

        *m_os << m_label << " took " << elapsed_time_in_units << ' ' << unit_cstr << '\n';
      }
    }

  private:
    std::string_view const m_label;
    std::ostream *m_os;
    double *m_elapsed_time_out;
    std::chrono::time_point<std::chrono::steady_clock> const m_start;
};

#endif // NLUKA_SCOPEDTIMER_HPP
