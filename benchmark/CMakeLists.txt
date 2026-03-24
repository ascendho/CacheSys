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
    cases.cpp
    main.cpp
)

target_include_directories(cache_benchmarks
    PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(cache_benchmarks
    PRIVATE
    benchmark::benchmark
)

target_compile_options(cache_benchmarks
    PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-O3 -DNDEBUG>
    $<$<CXX_COMPILER_ID:MSVC>:/O2 /DNDEBUG>
)

add_custom_target(cache_benchmark_thread_curve
    COMMAND /bin/bash ${CMAKE_CURRENT_SOURCE_DIR}/export_thread_curve.sh ${PROJECT_BINARY_DIR}
    DEPENDS cache_benchmarks
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    COMMENT "Export thread-scaling benchmark CSV and SVG curve"
)
