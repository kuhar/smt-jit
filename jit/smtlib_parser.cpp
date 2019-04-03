#include "smtlib_parser.hpp"

#include "llvm/Support/Debug.h"

#include <fstream>
#include <sstream>

namespace smt_jit {

void Assignment::dump(llvm::raw_ostream &os) const {
  const size_t vars = numVariables();
  size_t i = 0;

  for (auto &NameValues : m_variables) {
    os << NameValues.first() << ": [";

    const size_t numVals = NameValues.second.size();
    size_t j = 0;
    for (const AssignmentValTy val : NameValues.second) {
      os << val;
      ++j;
      if (j != numVals)
        os << ", ";
    }
    os << "]";

    ++i;
    if (i != vars)
      os << ", ";
  }
}

SmtLibParser::SmtLibParser(llvm::StringRef fileName) {
  std::ifstream file(fileName.str());
  if (file.bad()) {
    llvm::errs() << "[SmtLibParser] Could not open file: " << fileName << "\n";
    std::abort();
  }

  init(file);
}

SmtLibParser::SmtLibParser(std::istream &iss) { init(iss); }

void SmtLibParser::init(std::istream &iss) {
  std::string line;
  while (getline(iss, line)) {
    llvm::StringRef lineView = llvm::StringRef(line).ltrim();

    if (lineView.startswith("; { "))
      parseAssignment(line);
    else if (lineView.startswith("(declare-fun"))
      parseArrayDecl(line);
    else if (lineView.startswith("(assert"))
      parseAssertion(line);
  }
}

void SmtLibParser::parseAssignment(const std::string &line) {
  Assignment assignment;
  std::istringstream iss(line);
  char c = 0;

  iss >> c;
  assert(c == ';');
  iss >> c;
  assert(c == '{');

  do {
    iss >> c;

    if (c != '"') {
      assert(c == '}');
      break;
    } else {
      assert(c == '"');
    }

    std::string varName;
    while ((iss >> c) && c != '"')
      varName.push_back(c);

    assert(c == '"');

    iss >> c;
    assert(c == ':');
    iss >> c;
    assert(c == '[');

    smt_jit::AssignmentValTy val = 0;
    smt_jit::Assignment::AssignmentVector nums;

    while (iss >> val) {
      nums.push_back(val);
      iss >> c;
      assert(c == ',' || c == ']');

      if (c == ']')
        break;
    }

    assignment.addAssignment(varName, nums);

    if (!nums.empty())
      assert(c == ']');

    iss >> c;
  } while (c == ',');

  m_assignments.emplace_back(std::move(assignment));
}

void SmtLibParser::parseArrayDecl(const std::string &line) {
  // Sample array declaration:
  //   (declare-fun arg00 () (Array (_ BitVec 32) (_ BitVec 8)))
  //                  ^                       ^             ^
  //              array name             addressing    element width
  //
  using namespace sexpresso;
  std::string err;
  Sexp sexp = sexpresso::parse(line, err);

  assert(sexp.isSexp());
  Sexp &unwrapped = sexp.getChild(0);
  assert(unwrapped.isSexp());
  assert(unwrapped.getHead().isString());

  const std::string head = unwrapped.getHead().getString();
  assert(head == "declare-fun");

  assert(unwrapped.childCount() == 4);
  Sexp &name = unwrapped.getChild(1);
  assert(name.isString());
  const std::string arrayName = name.getString();

  Sexp &args = unwrapped.getChild(2);
  assert(args.isSexp());
  assert(args.childCount() == 0);

  Sexp &ret = unwrapped.getChild(3);
  assert(ret.isSexp());
  assert(ret.childCount() == 3);

  Sexp &arraySort = ret.getHead();
  assert(arraySort.isString());
  assert(arraySort.getString() == "Array");

  Sexp &indexing = ret.getChild(1);
  assert(indexing.isSexp());
  assert(indexing.childCount() == 3);
  assert(indexing.getChild(0).isString());
  assert(indexing.getChild(1).isString());
  assert(indexing.getChild(2).isNumber());
  assert(indexing.getChild(2).toNumber() == 32ll);

  Sexp &elementTy = ret.getChild(2);
  assert(elementTy.isSexp());
  assert(elementTy.childCount() == 3);
  assert(elementTy.getHead().isString());
  assert(elementTy.getHead().getString() == "_");
  assert(elementTy.getChild(1).isString());
  assert(elementTy.getChild(1).getString() == "BitVec");
  assert(elementTy.getChild(2).isNumber());

  const long long num = elementTy.getChild(2).toNumber();
  assert(num > 0);
  const unsigned elemWidth = (unsigned) num;

  ArrayInfo ai = {elemWidth, true, arrayName};
  m_arrays.push_back(ai);
}

void SmtLibParser::parseAssertion(const std::string &line) {
  using namespace sexpresso;
  std::string err;
  Sexp sexp = sexpresso::parse(line, err);

  Sexp &unwrapped = sexp.getChild(0);
  assert(unwrapped.isSexp());
  assert(unwrapped.getHead().isString());
  assert(unwrapped.getHead().getString() == "assert");

  m_assertions.push_back(unwrapped);
}

} // namespace smt_jit