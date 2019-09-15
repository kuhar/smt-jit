ARG CONFIG=debug

FROM kuhar/smt-jit-deps:$CONFIG

ARG BUILD_TYPE=debug

COPY . /smt-jit
WORKDIR /smt-jit/jit

RUN echo "Building smt-jit in the $BUILD_TYPE configuration" && \
  ./config-"$BUILD_TYPE".sh /deps/llvm /deps/z3 ./build && \
  cd ./build && ninja && cd .. && \
  cd ./build && ninja test-smt-jit && cd ..

RUN ./build/smt-jit --help && \
  ./build/test-smt-jit

