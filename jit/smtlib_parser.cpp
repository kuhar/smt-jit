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
    llvm::StringRef lineView(line);

    if (lineView.ltrim().startswith("; { ")) {
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

      llvm::errs() << "Reminder : " << c << "\n";

      m_assignments.emplace_back(std::move(assignment));
    }
  }
}

} // namespace smt_jit