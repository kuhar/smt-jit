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

bitvector bv_zero();
bitvector bv_one();
bitvector bv_bool(int b);

bitvector bv_mk(bv_width width, bv_word n);

bitvector bv_add(bitvector a, bitvector b);
bitvector bv_mul(bitvector a, bitvector b);

// LT: 1, GEQ: 0
int bv_ult(bitvector a, bitvector b);
// LT: 1, GEQ: 0
int bv_slt(bitvector a, bitvector b);
// EQ: 1, NEQ: 0
int bv_eq(bitvector a, bitvector b);

bitvector bv_and(bitvector a, bitvector b);
bitvector bv_or(bitvector a, bitvector b);

bitvector bv_concat(bitvector a, bitvector b);

bitvector bv_extract(bitvector a, bv_width from, bv_width to);
bitvector bv_zext(bitvector n, bv_width width);
bitvector bv_sext(bitvector n, bv_width width);

bv_array *bva_mk(bv_width width, bv_width len);
bv_array *bva_mk_init(bv_width width, bv_width len, bv_word *constants);

bitvector bva_select(bv_array *arr, bitvector n);

void bv_init_context();
void bv_reset_context();
void bv_teardown_context();

void bv_print(bitvector v);
void bv_fprint(void *file, bitvector v);
void bva_print(bv_array *arr);
void bva_fprint(void *file, bv_array *arr);
}

#endif