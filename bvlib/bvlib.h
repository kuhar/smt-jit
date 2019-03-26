#ifndef BVLIB_H
#define BVLIB_H

extern "C" {
typedef unsigned bv_width;
typedef unsigned long long bv_word;

typedef union {
  bv_word data;
  bv_word *ptr;
} WordPtrUnion;

struct bitvector_t {
  bv_width width;
  bv_width occupied_width;
  WordPtrUnion bits;
};
typedef struct bitvector_t bitvector;

struct bv_array_t {
  bv_word len;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc99-extensions"
  bitvector values[];
#pragma clang diagnostic pop
};
typedef struct bv_array_t bv_array;

bitvector bv_false();
bitvector bv_true();
bitvector bv_bool(int b);

bitvector bv_mk(bv_width width, bv_word n);

bitvector bv_add(bitvector a, bitvector b);
bitvector bv_mul(bitvector a, bitvector b);

bitvector bv_ult(bitvector a, bitvector b);
bitvector bv_slt(bitvector a, bitvector b);

bitvector bv_and(bitvector a, bitvector b);
bitvector bv_or(bitvector a, bitvector b);
bitvector bv_eq(bitvector a, bitvector b);

bitvector bv_concat(bitvector a, bitvector b);

bitvector bv_extract(bitvector a, bv_width from, bv_width to);
bitvector bv_zext(bitvector n, bv_width width);
bitvector bv_sext(bitvector n, bv_width width);

bitvector bva_select(bv_array *arr, bv_word n);

void bv_print(bitvector v);
void bva_print(bv_array *arr);
}

#endif