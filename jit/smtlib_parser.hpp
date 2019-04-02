#pragma once

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/raw_ostream.h"

#include <cassert>
#include <iosfwd>
#include <string>
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

  size_t numVariables() const { return m_variables.size(); }

  void dump(llvm::raw_ostream &os = llvm::errs()) const;
};

class SmtLibParser {
  std::vector<Assignment> m_assignments;
  llvm::SmallVector<std::string, 2> m_arrayNames;
  std::vector<std::string> m_assertions;

public:
  SmtLibParser(llvm::StringRef fileName);
  SmtLibParser(std::istream& iss);

  const std::vector<Assignment> &getAllAssignments() const {
    return m_assignments;
  }

  size_t numAssignments() const { return m_assignments.size(); }

private:
  void init(std::istream& iss);
};

} // namespace smt_jit
