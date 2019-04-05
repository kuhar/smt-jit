SMT-JIT
=======
### A toy Just-In-Time Compiler for evaluating SMT formulas (QF_AUFBV)
### [CS 842] Final Course Project
---

This project implements a simple JIT compiler for the QF_AUFBV subset of SMT emitted by the KLEE symbolic execution engine, including unbounded arrays of bitvectors and fixed-width bitvectors. *Smt-jit* uses the LLVM's ORCv2 API for jitting.

Given an SMT formula and a bunch of _full_ assignments in the SMT-LIB2 format, *smt-jit* will compile the SMT formula to LLVM IR, emit x86-64 machine code, and evalute the formula with the assignments. The main motivation is to be able to evaluate or fuzz SMT formulas before quering the main SMT solver for statisfability, e.g., in the KLEE's CexCachingSolver or Z3's local search phase.

Example:
```
(set-logic QF_AUFBV )
(declare-fun arg00 () (Array (_ BitVec 32) (_ BitVec 8) ) )
(assert (let ( (?B1 (select  arg00 (_ bv5 32) ) ) ) (and  (and  (=  false (=  (_ bv0 8) ?B1 ) ) (=  false (=  (_ bv61 8) ?B1 ) ) ) (=  false (=  (_ bv112 8) ?B1 ) ) ) ) )
(check-sat)
(exit)
```
The above formula will be compiled into the following IR:
```
define i32 @smt_0(%struct.bv_array_t** nocapture readonly %arrays) #5 {
entry:
  %arg00 = load %struct.bv_array_t*, %struct.bv_array_t** %arrays, align 8
  %0 = getelementptr inbounds %struct.bv_array_t, %struct.bv_array_t* %arg00, i64 0, i32 0
  %1 = load i64, i64* %0, align 8, !tbaa !2, !alias.scope !6
  %2 = trunc i64 %1 to i32
  %3 = icmp ugt i32 %2, 5
  %.op = and i64 %1, 4294967295
  %4 = select i1 %3, i64 5, i64 %.op
  %5 = getelementptr inbounds %struct.bv_array_t, %struct.bv_array_t* %arg00, i64 0, i32 1, i64 %4, i32 2, i32 0
  %6 = load i64, i64* %5, align 8, !alias.scope !6
  switch i64 %6, label %cont [
    i64 112, label %fail_1
    i64 61, label %fail_1
    i64 0, label %fail_1
  ]

fail_1:                                           ; preds = %entry, %entry, %entry, %cont
  %merge = phi i32 [ 1, %entry ], [ 0, %cont ], [ 1, %entry ], [ 1, %entry ]
  ret i32 %merge

cont:                                             ; preds = %entry
  br label %fail_1
}
```
Compilation of the above formula and evaluation over 1 milion full assignments takes about 30 milliseconds.

---

## Table of Contents

1. High-level Overview of the SMT-JIT Architecture
2. Benchmark Collection
3. bvlib: A custom Mixed-Precision BitVector library
4. Handling of BitVector Arrays
5. SMT-JIT Optimization Pipeline
6. Experimental Evaluation
