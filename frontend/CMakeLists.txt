cmake_minimum_required(VERSION 3.20)

# LLVM installs clang-cl into /usr/local/bin. On Linux it uses the MSVC
# driver and cannot link; CMake may pick it when CMAKE_PREFIX_PATH=/usr/local.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NOT CMAKE_CXX_COMPILER)
  find_program(_XAI_CXX c++ clang++
    HINTS /usr/bin /usr/local/bin
    NO_CMAKE_PATH NO_CMAKE_ENVIRONMENT_PATH)
  if(_XAI_CXX AND NOT _XAI_CXX MATCHES "clang-cl$")
    set(CMAKE_CXX_COMPILER "${_XAI_CXX}" CACHE STRING "C++ compiler")
  endif()
  unset(_XAI_CXX)
endif()
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NOT CMAKE_C_COMPILER AND CMAKE_CXX_COMPILER)
  get_filename_component(_XAI_CXX_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
  find_program(_XAI_C cc clang
    HINTS "${_XAI_CXX_DIR}" /usr/bin
    NO_CMAKE_PATH NO_CMAKE_ENVIRONMENT_PATH)
  if(_XAI_C AND NOT _XAI_C MATCHES "clang-cl$")
    set(CMAKE_C_COMPILER "${_XAI_C}" CACHE STRING "C compiler")
  endif()
  unset(_XAI_C)
  unset(_XAI_CXX_DIR)
endif()

project(XAIFrontend LANGUAGES CXX C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 17 CACHE STRING "C++ standard")
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")

find_package(MLIR REQUIRED CONFIG)
find_package(LLVM REQUIRED CONFIG)

message(STATUS "Using MLIRConfig.cmake in: ${MLIR_DIR}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

list(APPEND CMAKE_MODULE_PATH "${MLIR_CMAKE_DIR}")
include(TableGen)
include(AddLLVM)
include(AddMLIR)
include(HandleLLVMOptions)

include_directories(${LLVM_INCLUDE_DIRS})
include_directories(${MLIR_INCLUDE_DIRS})
include_directories(${PROJECT_SOURCE_DIR}/include)
add_definitions(${LLVM_DEFINITIONS})

set(XAI_LLC_DEFAULT
    "${PROJECT_SOURCE_DIR}/../third_party/llvm-project/build/bin/llc"
    CACHE FILEPATH "llc built with the XAI backend")

enable_testing()
add_subdirectory(lib)
add_subdirectory(tools)
add_subdirectory(test)
