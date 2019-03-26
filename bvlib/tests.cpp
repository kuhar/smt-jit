#include "doctest.h"

#include "bvlib.h"

TEST_CASE("Test bv_mk") {
  bitvector v = bv_mk(8, 12);
  CHECK(v.width == 8);
  CHECK(v.first == 12);
  CHECK(v.rest.data == 0);

  v = bv_mk(64, 123);
  CHECK(v.width == 64);
  CHECK(v.first == 123);
  CHECK(v.rest.data == 0);

  v = bv_mk(65, 1234);
  CHECK(v.width == 65);
  CHECK(v.first == 1234);
  CHECK(v.rest.data == 0);

  v = bv_mk(129, 12345);
  CHECK(v.width == 129);
  CHECK(v.first == 12345);
  CHECK(v.rest.data != 0);
}
