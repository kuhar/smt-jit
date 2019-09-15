FROM buildpack-deps:bionic

ARG BUILD_TYPE=release

RUN apt-get update && \
  apt-get install -yqq cmake ninja-build clang-8 lld-8 python-pip && \
  pip install lit OutputCheck && \
  mkdir -p /jit-deps

COPY ./ /jit-deps
RUN cd /jit-deps && rm -rf $(ls | grep -v ".sh")

WORKDIR /deps

RUN git clone https://github.com/llvm/llvm-project --branch release/9.x --depth=1 llvm-src && \
  /jit-deps/config-llvm-"$BUILD_TYPE".sh ./llvm-src build-llvm && \
  cd ./build-llvm && ninja install && cd .. && \
  mv ./build-llvm/run ./llvm && rm -rf ./build-llvm

RUN git clone https://github.com/Z3Prover/z3 --depth=1 z3-src && \
  /jit-deps/config-z3-"$BUILD_TYPE".sh ./z3-src build-z3 && \
  cd ./build-z3 && ninja install && cd .. && \
  mv ./build-z3/run ./z3 && rm -rf ./build-z3

