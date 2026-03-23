#include <benchmark/benchmark.h>

// Google Benchmark入口宏：
// 1) 自动生成main函数
// 2) 解析命令行参数（如 --benchmark_min_time）
// 3) 运行所有BENCHMARK注册的用例
BENCHMARK_MAIN();