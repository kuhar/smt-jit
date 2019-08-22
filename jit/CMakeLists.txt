cmake_minimum_required(VERSION 3.5)

project(smt-jit)

list (APPEND CMAKE_MODULE_PATH
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

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

find_package(Z3 REQUIRED)

message(STATUS "Z3_FOUND ${Z3_FOUND}")
message(STATUS "Z3_INCLUDE_DIR ${Z3_INCLUDE_DIR}")
message(STATUS "Z3_LIBRARY ${Z3_LIBRARY}")

include_directories(${Z3_INCLUDE_DIR})
link_directories(${Z3_LIBRARIES})

add_compile_options(-fno-rtti)
add_compile_options(-fno-exceptions)
add_compile_options(-march=native)

option(SMTJIT_USE_SPLIT_DWARF "Use -gsplit-dwarf. Decreases linker pressure." OFF)
if(SMTJIT_USE_SPLIT_DWARF)
  add_definitions("-gsplit-dwarf")
endif()

if (SMTJIT_USE_LINKER)
  set(OLD_CMAKE_REQUIRED_FLAGS ${CMAKE_REQUIRED_FLAGS})
  set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} -fuse-ld=${SMTJIT_USE_LINKER}")
  check_cxx_source_compiles("int main() { return 0; }" CXX_SUPPORTS_CUSTOM_LINKER)
  if (NOT CXX_SUPPORTS_CUSTOM_LINKER)
	  message(FATAL_ERROR "Host compiler does not support '-fuse-ld=${SMTJIT_USE_LINKER}'")
  endif()
  set(CMAKE_REQUIRED_FLAGS ${OLD_CMAKE_REQUIRED_FLAGS})
  append("-fuse-ld=${SMTJIT_USE_LINKER}"
    CMAKE_EXE_LINKER_FLAGS CMAKE_MODULE_LINKER_FLAGS CMAKE_SHARED_LINKER_FLAGS)
  message(STATUS "Using linker: ${SMTJIT_USE_LINKER}")
endif()

add_subdirectory(bvlib)
add_subdirectory(sexpresso)

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

set(SMTJIT_SOURCES
  bvlib_cloner.cpp
  smtlib_parser.cpp
  smtlib_to_llvm.cpp
)

add_executable(smt-jit smt-jit.cpp
  ${SMTJIT_SOURCES}
)

llvm_config(smt-jit ${LLVM_LINK_COMPONENTS})
target_link_libraries(smt-jit PRIVATE bvlib sexpresso ${Z3_LIBRARY})

message(STATUS "CXX_FLAGS: ${CMAKE_CXX_FLAGS}")

add_executable(test-smt-jit doctest_main_smt_jit.cpp
  ${SMTJIT_SOURCES}
  parser_tests.cpp
)
llvm_config(test-smt-jit ${LLVM_LINK_COMPONENTS})
target_link_libraries(test-smt-jit PRIVATE bvlib sexpresso ${Z3_LIBRARY})

enable_testing()
add_test(NAME test-smt-jit COMMAND test-smt-jit DEPENDS test-smt-jit)
