#! /bin/sh

set -e
KLEE_DIR="$1"
KLEE="$KLEE_DIR/bin/klee"

clang-5.0 -I "$KLEE_DIR/include/klee" -emit-llvm -c -g -O0 -Xclang -disable-O0-optnone tutorial1.c -o tutorial1.bc
$KLEE tutorial1.bc
