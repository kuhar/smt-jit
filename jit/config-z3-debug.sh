#! /bin/sh

set -e
Z3_DIR=$(readlink -f "$1")
BUILD_DIR="$2"

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

cmake "$Z3_DIR" -GNinja -DCMAKE_BUILD_TYPE=Debug \
			-DCMAKE_CXX_COMPILER=clang++-8 -DCMAKE_C_COMPILER=clang-8 \
			-DCMAKE_EXPORT_COMPILE_COMMANDS=1 \
			-DCMAKE_INSTALL_PREFIX=./run

