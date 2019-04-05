#! /bin/sh

set -e
mkdir -p build-llvm-release
cd build-llvm-release

cmake ../llvm-project/llvm -GNinja -DCMAKE_BUILD_TYPE=Release \
			-DLLVM_ENABLE_PROJECTS='clang' \
			-DLLVM_TARGETS_TO_BUILD='X86' \
			-DLLVM_OPTIMIZED_TABLEGEN=1 \
			-DLLVM_ENABLE_LLD=1 \
			-DCMAKE_CXX_COMPILER=clang++-6.0 -DCMAKE_C_COMPILER=clang-6.0 \
			-DCMAKE_EXPORT_COMPILE_COMMANDS=1 \
			-DCMAKE_INSTALL_PREFIX=./run
