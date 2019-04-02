#! /bin/sh

set -e
mkdir -p build
cd build

cmake .. -GNinja -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_EXPORT_COMPILE_COMMANDS=1 \
      -DCMAKE_C_COMPILER=clang-6.0 \
      -DCMAKE_CXX_COMPILER=clang++-6.0
