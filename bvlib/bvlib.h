#ifndef BVLIB_H
#define BVLIB_H

extern "C" {
	typedef unsigned long long bv_literal;
	typedef unsigned long long bv_word;

	struct bitvector_t {
		bv_literal width;
		bv_word first[2];
		bv_word *rest;
	};
	typedef struct bitvector_t bitvector;

	struct bv_array_t {
		bv_literal len;
		bitvector values[];
	};
	typedef struct bv_array_t bv_array;

	bitvector bv_mk(bv_literal n);

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

#endif 