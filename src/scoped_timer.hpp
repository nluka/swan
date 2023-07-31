#ifndef NLUKA_SCOPEDTIMER_HPP
#define NLUKA_SCOPEDTIMER_HPP

#include <chrono>
#include <cstdint>
// #include <ostream>
// #include <string_view>

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
    scoped_timer(double *elapsed_time_out)
      : m_elapsed_time_out{elapsed_time_out}
      , m_start{std::chrono::steady_clock::now()}
    {
        assert(elapsed_time_out != nullptr);
    }

    scoped_timer(scoped_timer const &) = delete; // copy constructor
    scoped_timer &operator=(scoped_timer const &) = delete; // copy assignment
    scoped_timer(scoped_timer &&) noexcept = delete; // move constructor
    scoped_timer &operator=(scoped_timer &&) noexcept = delete; // move assignment

    ~scoped_timer() {
        auto const end = std::chrono::steady_clock::now();
        auto const elapsed_nanos = end - m_start;
        auto const elapsed_time_in_units = (double)elapsed_nanos.count() / (double)TimerUnit;

        *m_elapsed_time_out = elapsed_time_in_units;
    }

private:
    double *const m_elapsed_time_out;
    std::chrono::time_point<std::chrono::steady_clock> const m_start;
};

#endif // NLUKA_SCOPEDTIMER_HPP
