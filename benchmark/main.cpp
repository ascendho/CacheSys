/*
 * cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
 * cmake --build build --target cache_benchmarks
 * ./build/benchmark/cache_benchmarks --benchmark_min_time=1s
 *
 */

#include <benchmark/benchmark.h>

BENCHMARK_MAIN();
