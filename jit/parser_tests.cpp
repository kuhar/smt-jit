#include "doctest.h"

#include "smtlib_parser.hpp"

#include <sstream>

using namespace smt_jit;

TEST_CASE("Test single_assignment_empty") {
  std::string txt = R"(
    ; Assignments
    ; { }
  )";

  std::istringstream iss(txt);
  smt_jit::SmtLibParser parser(iss);
  CHECK(parser.numAssignments() == 1);

  auto assignments = parser.assignments();
  CHECK(assignments.size() == 1);
  const Assignment &a = assignments.front();
  a.dump();
  llvm::errs() << "\n";
  CHECK(a.numVariables() == 0);
}

TEST_CASE("Test single_assignment") {
  std::string txt = R"(
    ; Assignments
    ; { "arg00": [3, 0, 0, 0, 0, 0, 1, 0] }
  )";

  std::istringstream iss(txt);
  smt_jit::SmtLibParser parser(iss);
  CHECK(parser.numAssignments() == 1);
  auto assignments = parser.assignments();

  CHECK(assignments.size() == 1);
  const Assignment &a = assignments.front();
  a.dump();
  llvm::errs() << "\n";

  CHECK(a.numVariables() == 1);
  CHECK(a.hasVariable("arg00"));
  CHECK(!a.hasVariable("arg01"));

  std::vector<AssignmentValTy> expectedValues = {3, 0, 0, 0, 0, 0, 1, 0};
  auto &arg00Assignment = a.getValue("arg00");
  CHECK(expectedValues == arg00Assignment);
}

TEST_CASE("Test single_empty_assignment") {
  std::string txt = R"(
    ; Assignments
    ; { "arg00": [] }
  )";

  std::istringstream iss(txt);
  smt_jit::SmtLibParser parser(iss);
  CHECK(parser.numAssignments() == 1);
  auto assignments = parser.assignments();

  CHECK(assignments.size() == 1);
  const Assignment &a = assignments.front();
  a.dump();
  llvm::errs() << "\n";

  CHECK(a.numVariables() == 1);
  CHECK(a.hasVariable("arg00"));
  CHECK(!a.hasVariable("arg01"));

  auto &arg00Assignment = a.getValue("arg00");
  CHECK(arg00Assignment.empty());
}

TEST_CASE("Test single_assignment_two") {
  std::string txt = R"(
    ; Assignments
    ; { "a": [1, 2, 3], "b": [4, 5] }
  )";

  std::istringstream iss(txt);
  smt_jit::SmtLibParser parser(iss);
  CHECK(parser.numAssignments() == 1);
  auto assignments = parser.assignments();

  CHECK(assignments.size() == 1);
  const Assignment &a = assignments.front();
  a.dump();
  llvm::errs() << "\n";

  CHECK(a.numVariables() == 2);
  CHECK(a.hasVariable("a"));
  CHECK(a.hasVariable("b"));

  auto &aAssignment = a.getValue("a");
  CHECK(aAssignment == std::vector<AssignmentValTy>{1, 2, 3});
  auto &bAssignment = a.getValue("b");
  CHECK(bAssignment == std::vector<AssignmentValTy>{4, 5});
}

TEST_CASE("Test two_assignment_two") {
  std::string txt = R"(
    ; Assignments
    ; { "a": [1, 2, 3], "b": [4, 5] }
    ; { "c": [6, 7], "b": [8] }
  )";

  std::istringstream iss(txt);
  smt_jit::SmtLibParser parser(iss);
  CHECK(parser.numAssignments() == 2);
  auto assignments = parser.assignments();

  CHECK(assignments.size() == 2);
  const Assignment &a0 = assignments.front();
  a0.dump();
  llvm::errs() << "\n";

  const Assignment &a1 = assignments.back();
  a1.dump();
  llvm::errs() << "\n";

  CHECK(a0.numVariables() == 2);
  CHECK(a0.hasVariable("a"));
  CHECK(a0.hasVariable("b"));

  auto &a0aAssignment = a0.getValue("a");
  CHECK(a0aAssignment == std::vector<AssignmentValTy>{1, 2, 3});
  auto &a0bAssignment = a0.getValue("b");
  CHECK(a0bAssignment == std::vector<AssignmentValTy>{4, 5});

  CHECK(a1.numVariables() == 2);
  CHECK(a1.hasVariable("c"));
  CHECK(a1.hasVariable("b"));

  auto &a1cAssignment = a1.getValue("c");
  CHECK(a1cAssignment == std::vector<AssignmentValTy>{6, 7});
  auto &a1bAssignment = a1.getValue("b");
  CHECK(a1bAssignment == std::vector<AssignmentValTy>{8});
}

TEST_CASE("Test two_assignment_three") {
  std::string txt = R"(
    ; Assignments
    ; { "a": [1, 2, 3], "b": [4, 5], "c": [] }
    ; { "d": [6, 7], "e": [8] }
  )";

  std::istringstream iss(txt);
  smt_jit::SmtLibParser parser(iss);
  CHECK(parser.numAssignments() == 2);
  auto assignments = parser.assignments();

  CHECK(assignments.size() == 2);
  const Assignment &a0 = assignments.front();
  a0.dump();
  llvm::errs() << "\n";

  const Assignment &a1 = assignments.back();
  a1.dump();
  llvm::errs() << "\n";

  CHECK(a0.numVariables() == 3);
  CHECK(a0.hasVariable("a"));
  CHECK(a0.hasVariable("b"));
  CHECK(a0.hasVariable("c"));

  auto &a0aAssignment = a0.getValue("a");
  CHECK(a0aAssignment == std::vector<AssignmentValTy>{1, 2, 3});
  auto &a0bAssignment = a0.getValue("b");
  CHECK(a0bAssignment == std::vector<AssignmentValTy>{4, 5});
  auto &a0cAssignment = a0.getValue("c");
  CHECK(a0cAssignment.empty());

  CHECK(a1.numVariables() == 2);
  CHECK(a1.hasVariable("d"));
  CHECK(a1.hasVariable("e"));

  auto &a1dAssignment = a1.getValue("d");
  CHECK(a1dAssignment == std::vector<AssignmentValTy>{6, 7});
  auto &a1eAssignment = a1.getValue("e");
  CHECK(a1eAssignment == std::vector<AssignmentValTy>{8});
}

TEST_CASE("Test single_array1") {
  std::string txt = R"(
    (declare-fun arg00 () (Array (_ BitVec 32) (_ BitVec 8) ) )
  )";

  std::istringstream iss(txt);
  smt_jit::SmtLibParser parser(iss);
  CHECK(parser.numAssignments() == 0);
  CHECK(parser.numAssertions() == 0);
  CHECK(parser.numArrays() == 1);

  auto arrays = parser.arrays();
  CHECK(arrays.size() == 1);
  const ArrayInfo &array = arrays.front();
  CHECK(array.name == "arg00");
  CHECK(array.element_width == 8);
  CHECK(array.is_bitvector);
}

TEST_CASE("Test single_array2") {
  std::string txt = R"(
    (declare-fun arr () (Array (_ BitVec 32) (_ BitVec 63) ) )
  )";

  std::istringstream iss(txt);
  smt_jit::SmtLibParser parser(iss);
  CHECK(parser.numAssignments() == 0);
  CHECK(parser.numAssertions() == 0);
  CHECK(parser.numArrays() == 1);

  auto arrays = parser.arrays();
  CHECK(arrays.size() == 1);
  const ArrayInfo &array = arrays.front();
  CHECK(array.name == "arr");
  CHECK(array.element_width == 63);
  CHECK(array.is_bitvector);
}

TEST_CASE("Test two_arrays") {
  std::string txt = R"(
    (declare-fun arg00 () (Array (_ BitVec 32) (_ BitVec 8) ) )
    (declare-fun const_arr () (Array (_ BitVec 32) (_ BitVec 32) ) )
  )";

  std::istringstream iss(txt);
  smt_jit::SmtLibParser parser(iss);
  CHECK(parser.numAssignments() == 0);
  CHECK(parser.numAssertions() == 0);

  CHECK(parser.numArrays() == 2);

  auto arrays = parser.arrays();
  CHECK(arrays.size() == 2);
  const ArrayInfo &array0 = arrays.front();
  CHECK(array0.name == "arg00");
  CHECK(array0.element_width == 8);
  CHECK(array0.is_bitvector);

  const ArrayInfo &array1 = arrays.back();
  CHECK(array1.name == "const_arr");
  CHECK(array1.element_width == 32);
  CHECK(array1.is_bitvector);
}
