cmake_minimum_required(VERSION 3.16)
project(CacheSys LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(CACHESYS_BUILD_TESTS "Build CacheSys unit tests" ON)
option(CACHESYS_BUILD_BENCHMARKS "Build CacheSys benchmarks" ON)
option(CACHESYS_BUILD_TRACE_COMPARE "Build trace-driven policy comparison tool" ON)

if(CACHESYS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()

if(CACHESYS_BUILD_BENCHMARKS)
    add_subdirectory(benchmark)
endif()

if(CACHESYS_BUILD_TRACE_COMPARE)
    add_subdirectory(trace)
endif()
