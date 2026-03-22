/*
 * 缓存策略性能测试工具 - 编译运行步骤（Release模式保证性能数据准确）：
 * 1. cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
 *    生成Release模式构建文件（关闭调试、开启优化，避免影响性能测试结果）
 * 2. cmake --build build --target cache_benchmarks
 *    编译性能测试程序（链接Google Benchmark框架）
 * 3. ./build/benchmark/cache_benchmarks --benchmark_min_time=1s
 *    运行测试，--benchmark_min_time=1s 指定每个测试用例至少运行1秒，提升统计稳定性
 */

#include <benchmark/benchmark.h>

// BENCHMARK_MAIN() 是Google Benchmark框架的核心宏：
// 1. 自动生成标准main函数，作为性能测试程序的入口
// 2. 解析命令行参数（如--benchmark_min_time）
// 3. 自动运行所有已注册的性能测试用例（如BM_Lru_MixedOps、BM_Lfu_HotSetGets）
// 4. 输出结构化的性能报告（每秒操作数OPS、耗时、误差等）
BENCHMARK_MAIN();