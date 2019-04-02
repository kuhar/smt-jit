#include "bvlib.h"

#include <cassert>
#include <iostream>
#include <vector>

/*
  (set-logic QF_AUFBV )
  (declare-fun arg00 () (Array (_ BitVec 32) (_ BitVec 8) ) )
  (assert (=  (_ bv115 8) (select  arg00 (_ bv5 32) ) ) )
  (check-sat)
  (exit)
*/

int eval(bv_array *arg00) {
  bitvector c1 = bv_mk(8, 115);
  bitvector tmp1 = bv_mk(32, 5);
  bitvector s1 = bva_select(arg00, tmp1);
  int eq = bv_eq(s1, c1);
  return eq;
}

int main() {
  bv_init_context();

  char c = 0;

  while ((std::cin >> c) && c == ';') {
    while ((std::cin >> c) && c != ':')
      ;
    std::cin >> c;

    std::vector<bv_word> constants;

    while (c != ']') {
      bv_word tmp = 0;
      std::cin >> tmp >> c;
      constants.push_back(tmp);
    }

    unsigned len = constants.size();

    bv_array *arr = bva_mk_init(8, len, constants.data());
    std::cout << "input arr: ";
    bva_print(arr);
    std::cout << "\n\n";
    std::cout.flush();

    int v = eval(arr);
    std::cout << "eval: " << v << "\n";

    std::cin >> c;
  }

  bv_teardown_context();
  return 0;
}