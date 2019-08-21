#! /bin/sh

LLVM="$1"

set -e
mkdir -p release
cd release

cmake .. -GNinja -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_EXPORT_COMPILE_COMMANDS=1 \
      -DCMAKE_C_COMPILER=clang-8 \
      -DCMAKE_CXX_COMPILER=clang++-8 \
      -DSMTJIT_USE_LINKER=lld-8 \
      -DSMTJIT_USE_SPLIT_DWARF=1 \
      -DLLVM_DIR="$LLVM"
