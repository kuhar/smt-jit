cmake_minimum_required(VERSION 3.5)

set(CMAKE_CXX_STANDARD 11)

message(STATUS "LLVM_DIR is ${LLVM_DIR}")
find_package(LLVM 9.0 REQUIRED CONFIG EXACT PATHS ${LLVM_DIR})

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
        
list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(AddLLVM)
include(HandleLLVMOptions)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${LLVM_CXXFLAGS}")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${LLVM_LDFLAGS}")

include_directories(${LLVM_INCLUDE_DIRS})
link_directories(${LLVM_LIBRARY_DIRS})
add_definitions(${LLVM_DEFINITIONS})

option(SMTJIT_ENABLE_LLD "Use lld as C and C++ linker." OFF)

set(LLVM_LINK_COMPONENTS 
  CodeGen
  Core
  ExecutionEngine
  IRReader
  Interpreter
  Instrumentation 
  IPO
  MC
  Object
  OrcJIT
  RuntimeDyld
  ScalarOpts
  SelectionDAG
  Support
  Target
  TransformUtils
  native
  )

add_executable(smt-jit smt-jit.cpp)
llvm_config(smt-jit ${LLVM_LINK_COMPONENTS})

message(STATUS "CXX_FLAGS: ${CMAKE_CXX_FLAGS}")


