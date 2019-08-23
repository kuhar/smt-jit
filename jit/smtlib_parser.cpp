#include "smtlib_parser.hpp"

#include "llvm/ADT/DenseSet.h"
#include "llvm/Support/Debug.h"

#include <fstream>
#include <sstream>
#include <unordered_set>

namespace smt_jit {

ZSmtLibParser::ZSmtLibParser(llvm::StringRef fileName, Z3_context ctx)
    : m_zCtx(ctx) {
  std::ifstream file(fileName.str());
  if (file.bad()) {
    llvm::errs() << "[SmtLibParser] Could not open file: " << fileName << "\n";
    std::abort();
  }

  init(file);
}

ZSmtLibParser::ZSmtLibParser(std::istream &iss, Z3_context ctx) : m_zCtx(ctx) {
  init(iss);
}

static void collectAllDecls(const ZAst &a, ZSmtLibParser::FunDeclSet &seen,
                            ZSmtLibParser::AstSet &visited) {
  if (Z3_get_ast_kind(a.ctx(), a) != Z3_APP_AST)
    return;

  if (visited.count(a) > 0)
    return;
  visited.insert(a);

  Z3_app app = Z3_to_app(a.ctx(), a);
  Z3_func_decl _fdecl = Z3_get_app_decl(a.ctx(), app);
  ZFDecl fdecl{a.ctx(), _fdecl};

  if (seen.count(fdecl) > 0)
    return;

  if (Z3_get_decl_kind(a.ctx(), fdecl) == Z3_OP_UNINTERPRETED) {
    if (seen.count(fdecl) == 0) {
      seen.insert(fdecl);
    }
  }

  for (unsigned i = 0, e = Z3_get_app_num_args(a.ctx(), app); i != e; ++i)
    collectAllDecls(ZAst(a.ctx(), Z3_get_app_arg(a.ctx(), app, i)), seen,
                    visited);
}

void ZSmtLibParser::init(std::istream &iss) {
  std::string content{std::istreambuf_iterator<char>(iss),
                      std::istreambuf_iterator<char>()};

  Z3_ast_vector asts = Z3_parse_smtlib2_string(
      m_zCtx, content.c_str(), 0, nullptr, nullptr, 0, nullptr, nullptr);
  if (!asts) {
    llvm::errs() << "[SmtLibParser] Failed to parse\n";
    std::abort();
  }

  m_asts = ZAstVec(m_zCtx, asts);
  llvm::errs() << "Asts: " << m_asts << "\n";
  const unsigned numAsts = m_asts.size();

  m_decls = ZFDeclVec::mk(m_zCtx);

  ZSmtLibParser::AstSet visited;

  for (ZAst ast : m_asts) {
    llvm::errs() << "Ast: " << ast << "\n";
    collectAllDecls(ast, m_allDecls, visited);
  }

  for (const ZFDecl &decl : m_allDecls) {
    llvm::errs() << "Decl: " << decl << "\n";
    m_decls.push_back(decl);
  }

  llvm::errs() << "All decls: " << m_decls << "\n";
}

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
    else if (lineView.startswith("; Assignments")) {
      lineView = lineView.ltrim();
      lineView.consume_front("; Assignments ");
      m_kleeTime = lineView.str();
    }
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