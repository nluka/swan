#ifndef NLUKA_ONSCOPEEXIT_HPP
#define NLUKA_ONSCOPEEXIT_HPP

template <typename Func>
class on_scope_exit
{
  public:
    on_scope_exit() = delete;

    on_scope_exit(Func fn) : m_fn(fn) {}

    ~on_scope_exit() {
      m_fn();
    }

  private:
    Func const m_fn;
};

template <typename Func>
on_scope_exit<Func> make_on_scope_exit(Func const &fn) {
  return on_scope_exit<Func>(fn);
}

#endif // NLUKA_ONSCOPEEXIT_HPP
