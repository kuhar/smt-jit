#include "doctest.h"

#include "bvlib.h"

TEST_CASE("Test bv_mk") {
  bitvector v = bv_mk(8, 12);
  CHECK(v.width == 8);
  CHECK(v.occupied_width == 4);
  CHECK(v.bits.data == 12);

  v = bv_mk(64, 123);
  CHECK(v.width == 64);
  CHECK(v.occupied_width == 7);
  CHECK(v.bits.data == 123);

  v = bv_mk(65, 1234);
  CHECK(v.width == 65);
  CHECK(v.occupied_width == 11);
  CHECK(v.bits.data == 1234);

  v = bv_mk(129, 12345);
  CHECK(v.width == 129);
  CHECK(v.occupied_width == 14);
  CHECK(v.bits.data == 12345);
}

TEST_CASE("Test bv_add_simple1") {
  bitvector a = bv_mk(8, 12);
  bitvector b = bv_mk(8, 13);
  bitvector s = bv_add(a, b);
  CHECK(s.width == 8);
  CHECK(s.occupied_width == 5);
  CHECK(s.bits.data == 25);
}

TEST_CASE("Test bv_add_simple2") {
  bitvector a = bv_mk(32, 12345);
  bitvector b = bv_mk(32, 78787);
  CHECK(a.occupied_width == 14);
  CHECK(b.occupied_width == 17);

  bitvector s = bv_add(a, b);
  CHECK(s.width == 32);
  CHECK(s.occupied_width == 18);
  CHECK(s.bits.data == 91132);
}

TEST_CASE("Test bv_add_simple2") {
  bitvector a = bv_mk(32, 12344);
  bitvector b = bv_mk(32, 1);
  CHECK(a.occupied_width == 14);
  CHECK(b.occupied_width == 1);

  bitvector s = bv_add(a, b);
  CHECK(s.width == 32);
  CHECK(s.occupied_width == 15);
  CHECK(s.bits.data == 12345);
}

TEST_CASE("Test bv_add_overflow1") {
  bitvector a = bv_mk(8, 255);
  bitvector b = bv_mk(8, 1);
  CHECK(a.occupied_width == 8);
  CHECK(b.occupied_width == 1);

  bitvector s = bv_add(a, b);
  CHECK(s.width == 8);
  CHECK(s.occupied_width == 8);
  CHECK(s.bits.data == 0);
}

TEST_CASE("Test bv_add_overflow2") {
  bitvector a = bv_mk(8, 255);
  bitvector b = bv_mk(8, 2);
  CHECK(a.occupied_width == 8);
  CHECK(b.occupied_width == 2);

  bitvector s = bv_add(a, b);
  CHECK(s.width == 8);
  CHECK(s.occupied_width == 8);
  CHECK(s.bits.data == 1);
}

TEST_CASE("Test bv_mul_simple1") {
  bitvector a = bv_mk(8, 4);
  bitvector b = bv_mk(8, 13);
  CHECK(a.occupied_width == 3);
  CHECK(b.occupied_width == 4);

  bitvector s = bv_mul(a, b);
  CHECK(s.width == 8);
  CHECK(s.occupied_width == 7);
  CHECK(s.bits.data == 52);
}

TEST_CASE("Test bv_mul_simple2") {
  bitvector a = bv_mk(8, 16);
  bitvector b = bv_mk(8, 15);
  bitvector s = bv_mul(a, b);
  CHECK(s.width == 8);
  CHECK(s.occupied_width == 8);
  CHECK(s.bits.data == 240);
}

TEST_CASE("Test bv_mul_overflow2") {
  bitvector a = bv_mk(8, 128);
  bitvector b = bv_mk(8, 2);
  bitvector s = bv_mul(a, b);
  CHECK(s.width == 8);
  CHECK(s.occupied_width == 8);
  CHECK(s.bits.data == 0);
}

TEST_CASE("Test bv_mul_overflow2") {
  bitvector a = bv_mk(8, 99);
  bitvector b = bv_mk(8, 99);
  bitvector s = bv_mul(a, b);
  CHECK(s.width == 8);
  CHECK(s.occupied_width == 8);
  CHECK(s.bits.data == 73);
}

TEST_CASE("Test bv_and_simple1") {
  bitvector a = bv_mk(8, 4);
  bitvector b = bv_mk(8, 13);
  CHECK(a.occupied_width == 3);
  CHECK(b.occupied_width == 4);

  bitvector s = bv_and(a, b);
  CHECK(s.width == 8);
  CHECK(s.occupied_width == 3);
  CHECK(s.bits.data == 4);
}

TEST_CASE("Test bv_and_simple2") {
  bitvector a = bv_mk(8, 7);
  bitvector b = bv_mk(8, 15);
  CHECK(a.occupied_width == 3);
  CHECK(b.occupied_width == 4);

  bitvector s = bv_and(a, b);
  CHECK(s.width == 8);
  CHECK(s.occupied_width == 3);
  CHECK(s.bits.data == 7);
}

TEST_CASE("Test bv_or_simple1") {
  bitvector a = bv_mk(8, 4);
  bitvector b = bv_mk(8, 13);
  CHECK(a.occupied_width == 3);
  CHECK(b.occupied_width == 4);

  bitvector s = bv_or(a, b);
  CHECK(s.width == 8);
  CHECK(s.occupied_width == 4);
  CHECK(s.bits.data == 13);
}

TEST_CASE("Test bv_and_simple2") {
  bitvector a = bv_mk(8, 7);
  bitvector b = bv_mk(8, 8);
  CHECK(a.occupied_width == 3);
  CHECK(b.occupied_width == 4);

  bitvector s = bv_or(a, b);
  CHECK(s.width == 8);
  CHECK(s.occupied_width == 4);
  CHECK(s.bits.data == 15);
}

TEST_CASE("Test bv_eq_simple1") {
  bitvector a = bv_mk(8, 7);
  bitvector b = bv_mk(8, 8);
  CHECK(a.occupied_width == 3);
  CHECK(b.occupied_width == 4);

  bitvector s = bv_eq(a, b);
  CHECK(s.width == 1);
  CHECK(s.occupied_width == 1);
  CHECK(s.bits.data == 0);
}

TEST_CASE("Test bv_eq_simple2") {
  bitvector a = bv_mk(8, 7);
  bitvector b = bv_mk(8, 7);
  bitvector s = bv_eq(a, b);
  CHECK(s.width == 1);
  CHECK(s.occupied_width == 1);
  CHECK(s.bits.data == 1);
}

TEST_CASE("Test bv_concat1") {
  bitvector a = bv_mk(2, 3);
  bitvector b = bv_mk(2, 2);
  bitvector s = bv_concat(a, b);
  CHECK(s.width == 4);
  CHECK(s.occupied_width == 4);
  CHECK(s.bits.data == 11);
}

TEST_CASE("Test bv_concat2") {
  bitvector a = bv_mk(2, 3);
  bitvector b = bv_mk(3, 1);
  bitvector s = bv_concat(a, b);
  CHECK(s.width == 5);
  CHECK(s.occupied_width == 3);
  CHECK(s.bits.data == 7);
}

TEST_CASE("Test bv_concat3") {
  bitvector a = bv_mk(2, 3);
  bitvector b = bv_mk(3, 0);
  bitvector s = bv_concat(a, b);
  CHECK(s.width == 5);
  CHECK(s.occupied_width == 2);
  CHECK(s.bits.data == 3);
}

TEST_CASE("Test bv_concat4") {
  bitvector a = bv_mk(4, 3);
  bitvector b = bv_mk(2, 2);
  bitvector s = bv_concat(a, b);
  CHECK(s.width == 6);
  CHECK(s.occupied_width == 6);
  CHECK(s.bits.data == 35);
}

TEST_CASE("Test bv_extract_single_bit1") {
  bitvector a = bv_mk(8, 1);
  bitvector s = bv_extract(a, 0, 0);
  CHECK(s.width == 1);
  CHECK(s.occupied_width == 1);
  CHECK(s.bits.data == 1);

  bitvector b = bv_mk(8, 0);
  s = bv_extract(b, 0, 0);
  CHECK(s.width == 1);
  CHECK(s.occupied_width == 0);
  CHECK(s.bits.data == 0);
}

TEST_CASE("Test bv_extract_bits1") {
  bitvector a = bv_mk(8, 6);
  bitvector s = bv_extract(a, 0, 1);
  CHECK(s.width == 2);
  CHECK(s.occupied_width == 2);
  CHECK(s.bits.data == 2);

  bitvector b = bv_mk(8, 5);
  s = bv_extract(b, 1, 2);
  CHECK(s.width == 2);
  CHECK(s.occupied_width == 2);
  CHECK(s.bits.data == 2);

  bitvector c = bv_mk(8, 90);
  s = bv_extract(c, 2, 4);
  CHECK(s.width == 3);
  CHECK(s.occupied_width == 3);
  CHECK(s.bits.data == 6);
}

TEST_CASE("Test bv_extract_bits2") {
  bitvector a = bv_mk(8, 3);
  bitvector s = bv_extract(a, 7, 8);
  CHECK(s.width == 2);
  CHECK(s.occupied_width == 0);
  CHECK(s.bits.data == 0);
}

TEST_CASE("Test bv_extract_whole") {
  bitvector a = bv_mk(8, 181);
  bitvector s = bv_extract(a, 0, 7);
  CHECK(s.width == 8);
  CHECK(s.occupied_width == 8);
  CHECK(s.bits.data == 181);
}