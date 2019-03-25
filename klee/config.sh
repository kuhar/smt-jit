#! /bin/sh

set -e
mkdir -p build
cd build
cmake .. -GNinja -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=1 \
      -DCMAKE_C_COMPILER=clang-6.0 -DCMAKE_CXX_COMPILER=clang++-6.0 \
      -DENABLE_SOLVER_STP=ON \
      -DENABLE_POSIX_RUNTIME=ON \
	  -DENABLE_KLEE_UCLIBC=ON \
	  -DKLEE_UCLIBC_PATH=/home/kuba/projects/klee-uclibc \
	  -DENABLE_UNIT_TESTS=ON \
	  -DGTEST_SRC_DIR=~/projects/klee/googletest-release-1.7.0 \
	  -DSTP_DIR=~/projects/stp/build \
	  -DLLVM_CONFIG_BINARY=/usr/bin/llvm-config-6.0 \
	  -DLLVMCC=/usr/bin/clang-6.0 \
	  -DLLVMCXX=/usr/bin/clang++-6.0 \
	  -DENABLE_KLEE_ASSERTS=OFF \
	  -DLLVM_ENABLE_ASSERTIONS=OFF