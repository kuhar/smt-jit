#pragma once

namespace smt_jit {
namespace detail {
template <typename F> class OnScopeExitImpl {
  F m_fn;

public:
  OnScopeExitImpl(F fn) : m_fn(fn) {}
  ~OnScopeExitImpl() { m_fn(); }
};
} // namespace detail

template <typename F> detail::OnScopeExitImpl<F> OnScopeExit(F fn) {
  return {fn};
}
} // namespace smt_jit
