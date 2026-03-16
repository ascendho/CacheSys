include(FetchContent)

FetchContent_Declare(
    googlebenchmark
    URL https://github.com/google/benchmark/archive/refs/tags/v1.8.3.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googlebenchmark)

add_executable(cache_benchmarks
    cache_benchmark.cpp
)

target_include_directories(cache_benchmarks
    PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(cache_benchmarks
    PRIVATE
    benchmark::benchmark
)
