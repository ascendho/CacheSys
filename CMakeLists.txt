cmake_minimum_required(VERSION 3.16)
project(CacheSys LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)
endif()

option(CACHESYS_BUILD_TESTS "Build CacheSys unit tests" ON)
option(CACHESYS_BUILD_BENCHMARKS "Build CacheSys benchmarks" ON)
option(CACHESYS_BUILD_TRACE_COMPARE "Build trace-driven policy comparison tool (deprecated alias, use CACHESYS_BUILD_EVALUATION)" ON)
option(CACHESYS_BUILD_EVALUATION "Build policy evaluation tools" ON)
option(CACHESYS_BUILD_DEMO "Build CacheSys end-to-end demo" ON)

if(CACHESYS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()

if(CACHESYS_BUILD_BENCHMARKS)
    add_subdirectory(benchmark)
endif()

if(CACHESYS_BUILD_EVALUATION OR CACHESYS_BUILD_TRACE_COMPARE)
    add_subdirectory(evaluation)
endif()

if(CACHESYS_BUILD_DEMO)
    add_subdirectory(demo)
endif()
