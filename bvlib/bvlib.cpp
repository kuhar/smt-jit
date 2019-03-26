#include "bvlib.h"
#include <cstdio>
#include <cstdlib>

#if BVLIB_ENABLE_ASSERTIONS == 1
#include <cassert>
#define BVLIB_ASSERT(...) assert(__VA_ARGS__)
#else
#define BVLIB_ASSERT(...) (void)0
#endif

namespace {
using bv_double_word = __uint128_t;

constexpr bv_word BVWordMax = (bv_word)~0;
constexpr bv_width BVWordBytes = sizeof(bv_word);
constexpr bv_width BVWordPtrBytes = sizeof(bv_word *);
static_assert(BVWordPtrBytes >= BVWordBytes, "Word size too small");
static_assert(sizeof(bitvector) == 2 * BVWordBytes, "Assumptions changed?");

constexpr bv_width BVWordBits = BVWordBytes * 8;

constexpr bv_width numWordsNeeded(bv_width width) {
  return (width - 1) / BVWordBits + 1;
}

constexpr bv_width numBitsNeeded(bv_word n) {
  bv_width res = 1;
  while (n >>= 1)
    ++res;
  return res;
}

constexpr bv_width min(bv_width a, bv_width b) { return a < b ? a : b; }
constexpr bv_width max(bv_width a, bv_width b) { return a < b ? b : a; }

constexpr bv_word maskOverflow(bv_word n, bv_width width) {
  const bv_width shiftAmount = BVWordBits - width;
  return (n << shiftAmount) >> shiftAmount;
}

} // namespace

extern "C" {

bitvector bv_true() { return {1, 1, {1}}; }
bitvector bv_false() { return {1, 1, {0}}; }
bitvector bv_bool(int b) { return {1, 1, {bv_word(b == 0 ? 0 : 1)}}; }

bitvector bv_mk(bv_width width, bv_word n) {
  if (width < BVWordBits)
    BVLIB_ASSERT(n <= (1ull << width) && "BV literal does not fit the width");
  BVLIB_ASSERT(n <= BVWordMax);
  (void)BVWordMax;

  const bv_width bitsNeeded = numBitsNeeded(n);
  BVLIB_ASSERT(numWordsNeeded(bitsNeeded) == 1);
  bitvector res = {width, bitsNeeded, {bv_word{n}}};
  return res;
}

bitvector bv_add(bitvector a, bitvector b) {
  BVLIB_ASSERT(a.width == b.width);
  BVLIB_ASSERT(a.occupied_width <= a.width);
  BVLIB_ASSERT(b.occupied_width <= b.width);

  const bv_width bitsNeeded =
      min(max(a.occupied_width, b.occupied_width) + 1, a.width);
  const bv_width wordsNeeded = numWordsNeeded(bitsNeeded);
  if (wordsNeeded > 1)
    BVLIB_ASSERT(false);

  bv_word bits = maskOverflow(a.bits.data + b.bits.data, a.width);
  bitvector res = {a.width, bitsNeeded, {bits}};
  return res;
}

bitvector bv_mul(bitvector a, bitvector b) {
  BVLIB_ASSERT(a.width == b.width);
  BVLIB_ASSERT(a.occupied_width <= a.width);
  BVLIB_ASSERT(b.occupied_width <= b.width);

  const bv_width bitsNeeded = min(a.occupied_width + b.occupied_width, a.width);
  const bv_width wordsNeeded = numWordsNeeded(bitsNeeded);
  if (wordsNeeded > 1)
    BVLIB_ASSERT(false);

  bv_word bits = maskOverflow(a.bits.data * b.bits.data, a.width);
  bitvector res = {a.width, bitsNeeded, {bits}};
  return res;
}

bitvector bv_and(bitvector a, bitvector b) {
  BVLIB_ASSERT(a.width == b.width);
  BVLIB_ASSERT(a.occupied_width <= a.width);
  BVLIB_ASSERT(b.occupied_width <= b.width);

  const bv_width bitsNeeded = min(a.occupied_width, b.occupied_width);
  const bv_width wordsNeeded = numWordsNeeded(bitsNeeded);
  if (wordsNeeded > 1)
    BVLIB_ASSERT(false);

  bv_word bits = a.bits.data & b.bits.data;
  bitvector res = {a.width, bitsNeeded, {bits}};
  return res;
}

bitvector bv_or(bitvector a, bitvector b) {
  BVLIB_ASSERT(a.width == b.width);
  BVLIB_ASSERT(a.occupied_width <= a.width);
  BVLIB_ASSERT(b.occupied_width <= b.width);

  const bv_width bitsNeeded = max(a.occupied_width, b.occupied_width);
  const bv_width wordsNeeded = numWordsNeeded(bitsNeeded);
  if (wordsNeeded > 1)
    BVLIB_ASSERT(false);

  bv_word bits = a.bits.data | b.bits.data;
  bitvector res = {a.width, bitsNeeded, {bits}};
  return res;
}

bitvector bv_eq(bitvector a, bitvector b) {
  BVLIB_ASSERT(a.width == b.width);
  BVLIB_ASSERT(a.occupied_width <= a.width);
  BVLIB_ASSERT(b.occupied_width <= b.width);

  const bv_width wordsNeeded =
      numWordsNeeded(max(a.occupied_width, b.occupied_width));
  if (wordsNeeded > 1)
    BVLIB_ASSERT(false);

  const int res = a.bits.data == b.bits.data;
  return bv_bool(res);
}

bitvector bv_concat(bitvector a, bitvector b) {
  BVLIB_ASSERT(a.occupied_width <= a.width);
  BVLIB_ASSERT(b.occupied_width <= b.width);

  const bv_width bitsNeeded = a.width + a.occupied_width;
  const bv_width wordsNeeded = numWordsNeeded(bitsNeeded);
  if (wordsNeeded > 1)
    BVLIB_ASSERT(false);

  const bv_word bits = (a.bits.data << a.width) | b.bits.data;
  bitvector res = {a.width + a.width, bitsNeeded, {bits}};
  return res;
}

bitvector bv_extract(bitvector a, bv_width from, bv_width to) {
  BVLIB_ASSERT(a.occupied_width <= a.width);
  BVLIB_ASSERT(from <= to);

  const bv_width end = to + 1;
  const bv_width bitsNeeded =
      min(end - from, max(a.occupied_width, from) - from);
  const bv_width wordsNeeded = numWordsNeeded(bitsNeeded);
  if (wordsNeeded > 1)
    BVLIB_ASSERT(false);

  const bv_width lsh_amount = BVWordBits - to;
  const bv_width rsh_amount = lsh_amount + from;
  bv_word bits = (a.bits.data << lsh_amount) >> rsh_amount;
  bitvector res = {bitsNeeded, bitsNeeded, {bits}};
  return res;
}

bitvector bv_zext(bitvector n, bv_width width) {
  BVLIB_ASSERT(n.occupied_width <= n.width);
  return {width, n.occupied_width, n.bits};
}

bitvector bv_sext(bitvector n, bv_width width);

bitvector bva_select(bv_array *arr, bv_word n);

void bv_print(bitvector v);
void bva_print(bv_array *arr);
}
