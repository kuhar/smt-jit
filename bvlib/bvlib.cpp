#include "bvlib.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>

namespace {
constexpr bv_word BVWordMax = (bv_word)~0;
constexpr bv_literal BVWordBytes = sizeof(bv_word);
constexpr bv_literal BVWordPtrBytes = sizeof(bv_word *);
static_assert(BVWordPtrBytes >= BVWordBytes, "Word size too small");

constexpr bv_literal BVWordBits = BVWordBytes * 8;

constexpr bv_literal numWordsNeeded(bv_literal width) {
  return (width - 1) / BVWordBits + 1;
}

} // namespace

extern "C" {
bitvector bv_mk(bv_literal width, bv_literal n) {
  if (width < BVWordBits)
    assert(n <= (1ull << width) && "BV literal does not fit the width");
  assert(n <= BVWordMax);
  (void)BVWordMax;

  bitvector res = {width, n, {bv_word{0}}};
  const bv_literal numWords = numWordsNeeded(width);
  if (numWords <= 2)
    return res;

  res.rest.ptr = (bv_word *)calloc(numWords - 1, BVWordBytes);
  return res;
}

bitvector bv_add(bitvector a, bitvector b);
bitvector bv_sub(bitvector a, bitvector b);
bitvector bv_mul(bitvector a, bitvector b);

bitvector bv_and(bitvector a, bitvector b);
bitvector bv_or(bitvector a, bitvector b);
bitvector bv_eq(bitvector a, bitvector b);

bitvector bv_concat(bitvector a, bitvector b);
bitvector bv_extract(bitvector a, bv_literal n);
bitvector bv_zext(bitvector n, bv_literal width);
bitvector bv_sext(bitvector n, bv_literal width);

bitvector bva_select(bv_array *arr, bv_literal n);

void bv_print(bitvector v);
void bva_print(bv_array *arr);
}
