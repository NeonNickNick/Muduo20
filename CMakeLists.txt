cmake_minimum_required(VERSION 3.16)
if(POLICY CMP0167)
  cmake_policy(SET CMP0167 NEW)
endif()

project(muduo20_20 VERSION 0.1.0 LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# Require C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(ZLIB REQUIRED)

# Compiler version requirements for full C++20 support
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "13.0")
    message(FATAL_ERROR "GCC version must be at least 13.0 for full C++20 support")
  endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "18.0")
    message(FATAL_ERROR "Clang version must be at least 18.0 for full C++20 support")
  endif()
endif()

include(CTest)
if(BUILD_TESTING)
  # Prefer system-provided GTest; fallback to FetchContent only when unavailable.
  find_package(GTest CONFIG QUIET)
  if(NOT GTest_FOUND)
    include(FetchContent)
    FetchContent_Declare(
      googletest
      URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
  endif()

  find_package(benchmark CONFIG QUIET)
  if(NOT benchmark_FOUND)
    message(STATUS "Google Benchmark not found, benchmarks will be skipped.")
  endif()

  # Note: Tests are now handled in muduo20/base/tests/CMakeLists.txt
endif()

# Build muduo20_base library
add_subdirectory(muduo20/base)
add_subdirectory(muduo20/net)
add_subdirectory(muduo20/co-net)

option(MUDUO20_BUILD_EXAMPLES "Build Muduo examples" ON)
if(MUDUO20_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

