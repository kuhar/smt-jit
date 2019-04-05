#! /bin/sh

LLVM="$1"

set -e
mkdir -p release
cd release

cmake .. -GNinja -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_EXPORT_COMPILE_COMMANDS=1 \
      -DCMAKE_C_COMPILER=clang-6.0 \
      -DCMAKE_CXX_COMPILER=clang++-6.0 \
      -DSMTJIT_USE_LINKER=lld-6.0 \
      -DLLVM_DIR="$LLVM"
