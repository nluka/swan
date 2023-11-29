#pragma once

template <typename Fun>
class ScopeGuard {
public:
    Fun m_fn;
    ScopeGuard(Fun&& fn) : m_fn(fn) {}
    ~ScopeGuard() { m_fn(); }
};

namespace detail {
    enum class ScopeGuardOnExit {};

    template <typename Fun>
    ScopeGuard<Fun> operator+(ScopeGuardOnExit, Fun&& fn) {
        return ScopeGuard<Fun>(std::forward<Fun>(fn));
    }
}

#define CONCATENATE_IMPL(s1, s2) s1##s2
#define CONCATENATE(s1, s2) CONCATENATE_IMPL(s1, s2)

#ifdef __COUNTER__
#define ANONYMOUS_VARIABLE(str) CONCATENATE(str, __COUNTER__)
#else
#define ANONYMOUS_VARIABLE(str) CONCATENATE(str, __LINE__)
#endif

#define SCOPE_EXIT \
auto ANONYMOUS_VARIABLE(SCOPE_EXIT_STATE) = ::detail::ScopeGuardOnExit() + [&]()
