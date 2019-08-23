#pragma once

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/raw_ostream.h"

#include "sexpresso.hpp"

#include "z3_utils.hpp"

#include <cassert>
#include <iosfwd>
#include <string>
#include <unordered_set>
#include <vector>

namespace smt_jit {

using AssignmentValTy = unsigned long long;

class Assignment {
public:
  using AssignmentVector = std::vector<AssignmentValTy>;

private:
  llvm::StringMap<AssignmentVector> m_variables;

public:
  void addAssignment(std::string varName, AssignmentVector values) {
    m_variables.insert({std::move(varName), std::move(values)});
  }

  bool hasVariable(llvm::StringRef varName) const {
    return m_variables.count(varName) > 0;
  }

  const AssignmentVector &getValue(llvm::StringRef varName) const {
    auto it = m_variables.find(varName);
    assert(it != m_variables.end() && "Wrong variable name?");
    return it->second;
  }

  AssignmentVector &getValue(llvm::StringRef varName) {
    auto it = m_variables.find(varName);
    assert(it != m_variables.end() && "Wrong variable name?");
    return it->second;
  }

  size_t numVariables() const { return m_variables.size(); }

  void dump(llvm::raw_ostream &os = llvm::errs()) const;
};

struct ArrayInfo {
  unsigned element_width;
  bool is_bitvector;
  std::string name;
};

class ZSmtLibParser {
public:
  using FunDeclSet = std::unordered_set<ZFDecl>;
  using AstSet = std::unordered_set<ZAst>;

private:
  Z3_context m_zCtx = nullptr;
  ZAstVec m_asts;
  ZFDeclVec m_decls;

  FunDeclSet m_allDecls;
  FunDeclSet m_constantDecls;
  FunDeclSet m_arrayDecls;

  void init(std::istream &iss);

public:
  ZSmtLibParser(llvm::StringRef fileName, Z3_context ctx);
  ZSmtLibParser(std::istream &iss, Z3_context);

  llvm::ArrayRef<Assignment> assignments() const;
  llvm::MutableArrayRef<Assignment> assignments();

  llvm::ArrayRef<ArrayInfo> arrays() const;

  llvm::MutableArrayRef<sexpresso::Sexp> assertions();
  llvm::ArrayRef<sexpresso::Sexp> assertions() const;

  size_t numAssignments() const;
  size_t numArrays() const;
  size_t numAssertions() const;
};

class SmtLibParser {
  std::vector<Assignment> m_assignments;
  llvm::SmallVector<ArrayInfo, 2> m_arrays;
  std::vector<sexpresso::Sexp> m_assertions;
  std::string m_kleeTime;

public:
  SmtLibParser(llvm::StringRef fileName);
  SmtLibParser(std::istream &iss);

  llvm::ArrayRef<Assignment> assignments() const { return m_assignments; }
  llvm::MutableArrayRef<Assignment> assignments() { return m_assignments; }

  llvm::ArrayRef<ArrayInfo> arrays() const { return m_arrays; }

  llvm::MutableArrayRef<sexpresso::Sexp> assertions() { return m_assertions; }
  llvm::ArrayRef<sexpresso::Sexp> assertions() const { return m_assertions; }

  size_t numAssignments() const { return m_assignments.size(); }
  size_t numArrays() const { return m_arrays.size(); }
  size_t numAssertions() const { return m_assertions.size(); }

  llvm::StringRef getKleeTime() const { return m_kleeTime; }

private:
  void init(std::istream &iss);
  void parseAssignment(const std::string &line);
  void parseArrayDecl(const std::string &line);
  void parseAssertion(const std::string &line);
};

} // namespace smt_jit
