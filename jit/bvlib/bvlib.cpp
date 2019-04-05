#include "bvlib.h"

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>

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
  bv_width res = 0;

  for (bv_width i = 0; i != BVWordBits; ++i) {
    res += n != 0 ? 1 : 0;
    n >>= 1;
  }

  return res;
}

constexpr bv_width min(bv_width a, bv_width b) { return a < b ? a : b; }
constexpr bv_width max(bv_width a, bv_width b) { return a < b ? b : a; }

constexpr bv_word maskOverflow(bv_word n, bv_width width) {
  const bv_width shiftAmount = (width < BVWordBits) ? BVWordBits - width : 0;
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
    return ctx;
  }

  bv_width remainingWords() const { return (memEnd - memNext) / BVWordBytes; }

  [[ gnu::alloc_size(2), gnu::returns_nonnull ]] char *alloc_bytes(bv_width n) {
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
    memEnd = memBegin + PoolBytes;
  }

  void reset() {
    memset(memBegin, 0, (memNext - memBegin) * BVWordBytes);
    memNext = memBegin;
  }

  void teardown() { free(memBegin); }
};

} // namespace

extern "C" {

bitvector bv_zero() { return {1, 0, {0}}; }
bitvector bv_one() { return {1, 1, {1}}; }
bitvector bv_bool(int b) { return b == 0 ? bv_zero() : bv_one(); }

bitvector bv_mk(bv_width width, bv_word n) {
  if (width < BVWordBits)
    BVLIB_ASSERT(n <= (1ull << width) && "BV literal does not fit the width");
  BVLIB_ASSERT(n <= BVWordMax);
  BVLIB_ASSERT(n >= 0);
  (void)BVWordMax;

  const bv_word bits = maskOverflow(n, width);
  const bv_width bitsNeeded = numBitsNeeded(bits);
  BVLIB_ASSERT(numWordsNeeded(bitsNeeded) == 1);
  bitvector res = {width, bitsNeeded, {bits}};
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

int bv_ult(bitvector a, bitvector b) {
  BVLIB_ASSERT(a.width == b.width);
  BVLIB_ASSERT(a.occupied_width <= a.width);
  BVLIB_ASSERT(b.occupied_width <= b.width);

  return a.bits.data < b.bits.data;
}

int bv_slt(bitvector a, bitvector b) {
  BVLIB_ASSERT(a.width == b.width);
  BVLIB_ASSERT(a.occupied_width <= a.width);
  BVLIB_ASSERT(b.occupied_width <= b.width);

  const bv_word a_sign_bit = a.bits.data >> (a.width - 1);
  const bv_word b_sign_bit = b.bits.data >> (b.width - 1);
  const bool flipRes = a_sign_bit != b_sign_bit;
  const bool cmp = (a.bits.data < b.bits.data);
  return flipRes ? !cmp : cmp;
}

int bv_eq(bitvector a, bitvector b) {
  BVLIB_ASSERT(a.width == b.width);
  BVLIB_ASSERT(a.occupied_width <= a.width);
  BVLIB_ASSERT(b.occupied_width <= b.width);

  return a.bits.data == b.bits.data;
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
  BVLIB_ASSERT(n.width <= width);

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
  bv_width wordsToAlloc = len + 4;
  char *bytes = BVContext::get().alloc_words(wordsToAlloc);
  bv_array *arr = (bv_array *)bytes;
  arr->len = len;

  for (bv_width i = 0; i != len + 1; ++i)
    arr->values[i].width = width;

  return arr;
}

bv_array *bva_mk_init(bv_width width, bv_width len, bv_word *constants) {
  BVLIB_ASSERT(constants);

  bv_width wordsToAlloc = len + 4;
  char *bytes = BVContext::get().alloc_words(wordsToAlloc);
  bv_array *arr = (bv_array *)bytes;
  arr->len = len;

  for (bv_width i = 0; i != len; ++i) {
    if (constants[i] != 0)
      arr->values[i] = bv_mk(width, constants[i]);
    else
      arr->values[i].width = width;
  }

  arr->values[len] = bv_zero();

  return arr;
}

void bv_init_context() { BVContext::get().init(); }
void bv_reset_context() { BVContext::get().reset(); }
void bv_teardown_context() { BVContext::get().teardown(); }

bitvector bva_select(bv_array *arr, bitvector n) {
  BVLIB_ASSERT(arr);
  BVLIB_ASSERT(n.occupied_width < BVWordBits);

  bv_word i = n.bits.data;
  bv_word idx = min(i, arr->len);

  return arr->values[idx];
}

void bv_fprint(void *file, bitvector v) {
  BVLIB_ASSERT(v.occupied_width <= BVWordBits);

  fprintf((FILE *)file, "{w: %u, ow: %u, ", v.width, v.occupied_width);
  if (v.occupied_width <= BVWordBits)
    fprintf((FILE *)file, "n: %llu, [", v.bits.data);
  else
    fprintf((FILE *)file, "[");

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
  BVLIB_ASSERT(arr);

  fprintf((FILE *)file, "(arr.len: %llu) [", arr->len);

  for (bv_width i = 0, e = arr->len; i != e; ++i) {
    bv_fprint(file, arr->values[i]);
    if (i + 1 != e)
      fprintf((FILE *)file, ", ");
  }

  fprintf((FILE *)file, "]");
}

void bva_print(bv_array *arr) { bva_fprint(stdout, arr); }

} // extern "C"
