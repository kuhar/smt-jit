#pragma once

#include "z3.h"

#include "llvm/Support/raw_ostream.h"

#include <cassert>
#include <utility>

namespace smt_jit {

namespace detail {

template <typename ZTy> class ZRefBase {
public:
  using Z3Ty = ZTy;

protected:
  void *m_ptr;
  void *m_ctx;

  ZRefBase(Z3_context ctx, Z3Ty ptr) : m_ptr(ptr), m_ctx(ctx) {}

  Z3_context ctxImpl() const { return (Z3_context)m_ctx; }
  ZTy getImpl() const { return (ZTy)m_ptr; }

  template <typename Ty> Ty asT() const;
  template <> ZTy asT<ZTy>() const { return getImpl(); }

  template <typename T> friend class ZRef;
  
public:
  size_t hash() const { return std::hash<void *>{}(m_ptr); }
};

/// ZRefImpl must be derived from ZRefBase.
template <typename ZRefImpl> class ZRef : public ZRefImpl {
public:
  using ZTy = typename ZRefImpl::Z3Ty;

  ZRef(Z3_context ctx, ZTy val) : ZRefImpl(ctx, val) { inc(); }

  ZRef() : ZRefImpl(nullptr, nullptr) {}

  template <typename... Args> static ZRef mk(const Args &... args) {
    ZRef ref;
    ref.finalizeMk(args...);
    ref.inc();
    return ref;
  }

  ZRef(const ZRef &other) : ZRef(other.ctx(), other.get()) {}
  ZRef &operator=(const ZRef &other) {
    dec();
    this->m_ptr = other.m_ptr;
    this->m_ctx = other.m_ctx;
    inc();
    return *this;
  }

  ZRef(ZRef &&other) : ZRefImpl(other.ctx(), other.get()) {
    other.m_ptr = nullptr;
    other.m_ctx = nullptr;
  }
  ZRef &operator=(ZRef &&other) {
    dec();
    this->m_ptr = other.m_ptr;
    this->m_ctx = other.m_ctx;
    other.m_ptr = nullptr;
    other.m_ctx = nullptr;

    return *this;
  }

  ~ZRef() { dec(); }

  Z3_context ctx() const { return this->ctxImpl(); }
  ZTy get() const { return this->getImpl(); }

  operator ZTy() const { return get(); }
  explicit operator bool() const { return this->m_ptr; }

  bool operator<(const ZRef &other) const { return this->m_ptr < other.m_ptr; }
  bool operator==(const ZRef &other) const {
    return this->m_ptr == other.m_ptr;
  }
  bool operator!=(const ZRef &other) const {
    return this->m_ptr != other.m_ptr;
  }

  Z3_string toZStr() const { return this->toZStrImpl(); }
  std::string toStr() const { return {toZStr()}; }
  friend llvm::raw_ostream &operator<<(llvm::raw_ostream &os, const ZRef &ref) {
    return os << ref.toZStr();
  }
  llvm::raw_ostream &dump(llvm::raw_ostream &os = llvm::errs()) const {
    return os << toZStr() << "\n";
  }

  template <typename ZRefTy> ZRefTy as() const {
    return {ctx(), this->template asT<typename ZRefTy::ZTy>()};
  }
  template <> ZRef as<ZRef>() const { return *this; }

private:
  void inc() {
    if (this->m_ptr) {
      assert(this->m_ctx);
      this->incRefImpl();
    }
  }

  void dec() {
    if (this->m_ptr) {
      assert(this->m_ctx);
      this->decRefImpl();
    }
  }
};

class ZAstImpl : public ZRefBase<Z3_ast> {
protected:
  ZAstImpl(Z3_context ctx, Z3_ast a) : ZRefBase(ctx, a) {}
  template <typename Ty> Ty asT() const;
  template <> Z3_func_decl asT<Z3_func_decl>() const {
    assert(Z3_get_ast_kind(this->ctxImpl(), this->getImpl()) ==
           Z3_ast_kind::Z3_FUNC_DECL_AST);
    return Z3_to_func_decl(this->ctxImpl(), this->getImpl());
  }

  void incRefImpl() const { Z3_inc_ref(this->ctxImpl(), this->getImpl()); }
  void decRefImpl() const { Z3_dec_ref(this->ctxImpl(), this->getImpl()); }
  Z3_string toZStrImpl() const {
    return Z3_ast_to_string(this->ctxImpl(), this->getImpl());
  }
};

class ZFDeclImpl : public ZRefBase<Z3_func_decl> {
protected:
  ZFDeclImpl(Z3_context ctx, Z3_func_decl decl) : ZRefBase(ctx, decl) {}
  template <typename Ty> Ty asT() const;
  template <> Z3_ast asT<Z3_ast>() const {
    return Z3_func_decl_to_ast(this->ctxImpl(), this->getImpl());
  }

  void incRefImpl() const { Z3_inc_ref(this->ctxImpl(), asT<Z3_ast>()); }
  void decRefImpl() const { Z3_dec_ref(this->ctxImpl(), asT<Z3_ast>()); }
  Z3_string toZStrImpl() const {
    return Z3_func_decl_to_string(this->ctxImpl(), this->getImpl());
  }
};

template <typename ElemRefTy>
class ZGenericAstVecImpl : public ZRefBase<Z3_ast_vector> {
protected:
  ZGenericAstVecImpl(Z3_context ctx, Z3_ast_vector v) : ZRefBase(ctx, v) {}
  void incRefImpl() const {
    Z3_ast_vector_inc_ref(this->ctxImpl(), this->getImpl());
  }
  void decRefImpl() const {
    Z3_ast_vector_dec_ref(this->ctxImpl(), this->getImpl());
  }
  Z3_string toZStrImpl() const {
    return Z3_ast_vector_to_string(this->ctxImpl(), this->getImpl());
  }
  void finalizeMk(Z3_context ctx) {
    assert(ctx);
    this->m_ctx = ctx;
    this->m_ptr = Z3_mk_ast_vector(ctx);
  }

private:
  using AstRef = detail::ZRef<detail::ZAstImpl>;

public:
  size_t size() const {
    return Z3_ast_vector_size(this->ctxImpl(), this->getImpl());
  }

  bool empty() const { return size() == 0; }

  ElemRefTy operator[](size_t i) const {
    assert(i < size());
    Z3_ast ast = Z3_ast_vector_get(this->ctxImpl(), this->getImpl(), i);
    AstRef zast{this->ctxImpl(), ast};
    return zast.as<ElemRefTy>();
  }

  void set(size_t i, ElemRefTy val) {
    assert(i < size());
    AstRef ast = val.template as<AstRef>();
    Z3_ast_vector_set(this->ctxImpl(), this->getImpl(), i, ast);
  }

  void push_back(ElemRefTy val) {
    AstRef ast = val.template as<AstRef>();
    Z3_ast_vector_push(this->ctxImpl(), this->getImpl(), ast);
  }

  void resize(size_t sz) {
    Z3_ast_vector_resize(this->ctxImpl(), this->getImpl(), sz);
  }

  class Iterator {
    const ZGenericAstVecImpl &m_vec;
    unsigned m_idx = 0;

  public:
    using iterator_category = std::bidirectional_iterator_tag;
    using value_type = ElemRefTy;
    using difference_type = ptrdiff_t;
    using pointer = ElemRefTy;
    using reference = ElemRefTy;

    Iterator(const ZGenericAstVecImpl &vec) : m_vec(vec) {}
    Iterator(const Iterator &) = default;
    Iterator &operator=(const Iterator &) = default;

    static Iterator mkBegin(const ZGenericAstVecImpl &vec) { return {vec}; }
    static Iterator mkEnd(const ZGenericAstVecImpl &vec) {
      Iterator it{vec};
      it.m_idx = vec.size();
      return it;
    }

    Iterator &operator++() {
      ++m_idx;
      assert(m_idx <= m_vec.size());
      return *this;
    }
    Iterator &operator--() {
      assert(m_idx > 0);
      assert(m_idx <= m_vec.size());
      --m_idx;
      return *this;
    }

    reference operator*() const { return m_vec[m_idx]; }
    pointer operator->() const { return m_vec[m_idx]; }

    difference_type operator-(const Iterator &other) const {
      assert(&m_vec == &other.m_vec);
      assert(m_idx >= other.m_idx);
      return static_cast<ptrdiff_t>(m_idx - other.m_idx);
    }

    bool operator==(const Iterator &other) const {
      assert(&m_vec == &other.m_vec);
      return m_idx == other.m_idx;
    }
    bool operator!=(const Iterator &other) const {
      assert(&m_vec == &other.m_vec);
      return m_idx != other.m_idx;
    }
  };

  using iterator = Iterator;
  iterator begin() const { return Iterator::mkBegin(*this); }
  iterator end() const { return Iterator::mkEnd(*this); }
};

} // namespace detail

using ZAst = detail::ZRef<detail::ZAstImpl>;
using ZFDecl = detail::ZRef<detail::ZFDeclImpl>;

using ZAstVec = detail::ZRef<detail::ZGenericAstVecImpl<ZAst>>;
using ZFDeclVec = detail::ZRef<detail::ZGenericAstVecImpl<ZFDecl>>;

} // namespace smt_jit

namespace std {
template <> struct hash<smt_jit::ZAst> {
  size_t operator()(const smt_jit::ZAst &ast) const { return ast.hash(); }
};

template <> struct hash<smt_jit::ZFDecl> {
  size_t operator()(const smt_jit::ZFDecl &decl) const { return decl.hash(); }
};

} // namespace std
