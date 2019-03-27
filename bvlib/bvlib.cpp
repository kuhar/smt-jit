#include "bvlib.h"

#include <cstddef>
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
  bv_width res = n == 0 ? 0 : 1;
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

constexpr bv_word maskLowerBits(bv_word n, bv_width width) {
  return (n >> width) << width;
}

struct BVContext {
  char *memBegin = nullptr;
  char *memEnd = nullptr;
  char *memNext = nullptr;
  static constexpr size_t PoolBytes = 1 << 24;
  static constexpr size_t PoolWords = PoolBytes / BVWordBytes;

  static BVContext &get() {
    static BVContext ctx;

    if (!ctx.memBegin)
      ctx.init();

    return ctx;
  }

  bv_width remainingWords() const { return (memEnd - memNext) / BVWordBytes; }

  char *alloc_bytes(bv_width n) {
    char *const ret = memNext;

    const bv_width toBump = n + n % BVWordBytes;
    BVLIB_ASSERT(toBump <= memEnd - memBegin);
    memNext += toBump;

    return ret;
  }

  char *alloc_words(bv_width n) {
    BVLIB_ASSERT(remainingWords() >= n);
    return alloc_bytes(n * BVWordBytes);
  }

  void init() {
    memNext = memBegin = (char *)calloc(PoolWords, BVWordBytes);
    memEnd = memBegin + PoolWords * BVWordBytes;
  }

  static void teardown() {
    BVContext &ctx = get();
    free(ctx.memBegin);
  }
};

} // namespace

extern "C" {

bitvector bv_true() { return {1, 1, {1}}; }
bitvector bv_false() { return {1, 1, {0}}; }
bitvector bv_bool(int b) { return {1, 1, {bv_word(b == 0 ? 0 : 1)}}; }

bitvector bv_mk(bv_width width, bv_word n) {
  if (width < BVWordBits)
    BVLIB_ASSERT(n <= (1ull << width) && "BV literal does not fit the width");
  BVLIB_ASSERT(n <= BVWordMax);
  BVLIB_ASSERT(n >= 0);
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

  const bv_width bitsNeeded = a.width + b.occupied_width;
  const bv_width wordsNeeded = numWordsNeeded(bitsNeeded);
  if (wordsNeeded > 1)
    BVLIB_ASSERT(false);

  const bv_word bits = (b.bits.data << a.width) | a.bits.data;
  bitvector res = {a.width + b.width, bitsNeeded, {bits}};
  return res;
}

bitvector bv_extract(bitvector a, bv_width from, bv_width to) {
  BVLIB_ASSERT(a.occupied_width <= a.width);
  BVLIB_ASSERT(from <= to);

  const bv_width end = to + 1;
  const bv_width newWidth = end - from;
  const bv_width bitsNeeded = min(newWidth, max(a.occupied_width, from) - from);
  const bv_width wordsNeeded = numWordsNeeded(bitsNeeded);
  if (wordsNeeded > 1)
    BVLIB_ASSERT(false);

  const bv_width lsh_amount = BVWordBits - to - 1;
  const bv_width rsh_amount = lsh_amount + from;
  const bv_word shifted1 = a.bits.data << lsh_amount;
  const bv_word bits = shifted1 >> rsh_amount;
  // printf("orig %08llx, lsh_amount %u, rsh_amount %u, shifted1 %08llx,"
  //        " final: %08llx\n",
  // 	     a.bits.data, lsh_amount, rsh_amount, shifted1, bits);
  bitvector res = {newWidth, bitsNeeded, {bits}};
  return res;
}

bitvector bv_zext(bitvector n, bv_width width) {
  BVLIB_ASSERT(n.occupied_width <= n.width);
  return {width, n.occupied_width, n.bits};
}

bitvector bv_sext(bitvector n, bv_width width) {
  BVLIB_ASSERT(n.occupied_width <= n.width);

  const bv_word pad_bit = n.bits.data >> (n.width - 1);
  if (pad_bit == 1 && BVWordBits < width)
    BVLIB_ASSERT(false);

  const bv_word mask = (pad_bit == 0) ? 0 : maskLowerBits(~0, n.width);
  const bv_word bits = maskOverflow(n.bits.data | mask, width);

  const bv_width bitsNeeded = (pad_bit == 0) ? n.occupied_width : width;
  bitvector res = {width, bitsNeeded, {bits}};
  return res;
}

bv_array *bva_mk(bv_width width, bv_width len) {
  return bva_mk_init(width, len, nullptr);
}

bv_array *bva_mk_init(bv_width width, bv_width len, bv_word *constants) {
  bv_width wordsToAlloc = len + 2;
  char *bytes = BVContext::get().alloc_words(wordsToAlloc);
  bv_array *arr = (bv_array *)bytes;
  arr->len = len;

  for (bv_width i = 0; i != len; ++i)
    arr->values[i] = bv_mk(width, constants ? constants[i] : 0);

  return arr;
}

void bv_teardown_context() { BVContext::get().teardown(); }

bitvector bva_select(bv_array *arr, bv_word n) {
  BVLIB_ASSERT(arr);
  BVLIB_ASSERT(n < arr->len);
  return arr->values[n];
}

void bv_fprint(void *file, bitvector v) {
  BVLIB_ASSERT(v.occupied_width <= BVWordBits);

  fprintf((FILE *)file, "{w: %u, ow: %u, [", v.width, v.occupied_width);

  const bv_word n = v.bits.data;
  for (bv_width i = 0, e = v.occupied_width; i != e; ++i) {
    char c = ((n & (1 << i)) == 0) ? '0' : '1';
    fprintf((FILE *)file, (i + 1 == e) ? "%c" : "%c, ", c);
  }

  const bv_width numTrailingZeros = v.width - v.occupied_width;
  for (bv_width i = 0; i != numTrailingZeros; ++i)
    fprintf((FILE *)file, ", 0");

  fprintf((FILE *)file, "]}");
}

void bv_print(bitvector v) { bv_fprint(stdout, v); }

void bva_fprint(void *file, bv_array *arr) {
  fprintf((FILE *)file, "[");

  for (bv_width i = 0, e = arr->len; i != e; ++i) {
    bv_fprint(file, arr->values[i]);
    if (i + i != e)
      fprintf((FILE *)file, ", ");
  }

  fprintf((FILE *)file, "]");
}

void bva_print(bv_array *arr) { bva_fprint(stdout, arr); }

} // extern "C"
