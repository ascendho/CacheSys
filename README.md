## CacheSys

CacheSys 是基于 C++ 开发的一个可扩展缓存系统，主要用于学习和实践缓存相关技术。该系统里实现了多种经典的缓存淘汰算法，包括 LRU（最近最少使用）、LFU（最不经常使用）、ARC（自适应替换缓存）、LRU-K，同时也支持分片缓存的功能。 

除了核心的缓存淘汰算法外，该项目还通过组件化的方式给 CacheSys 补充了完整的缓存能力：支持带过期时间的缓存控制（TTL 装饰），并实现了 CacheWithLoader 组件，在缓存未命中时自动从数据源回源加载。 

项目提供完整验证链路来保证功能正确和性能可量化：test 模块通过自动化测试验证容量限制、路由逻辑、TTL 规则和自动加载语义；benchmark 模块测试高频混合读写下的吞吐表现；evaluation 模块可加载外部访问轨迹评估不同策略命中率，并与理论最优 OPT 做对比。

## 构建

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

可选构建开关：

1. CACHESYS_BUILD_TESTS：是否构建测试，默认 ON。
2. CACHESYS_BUILD_BENCHMARKS：是否构建基准，默认 ON。
3. CACHESYS_BUILD_EVALUATION：是否构建评估工具，默认 ON。
4. CACHESYS_BUILD_DEMO：是否构建 Demo，默认 ON。
5. CACHESYS_BUILD_TRACE_COMPARE：历史兼容开关，默认 ON。

示例：只构建评估程序。

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DCACHESYS_BUILD_TESTS=OFF \
  -DCACHESYS_BUILD_BENCHMARKS=OFF \
  -DCACHESYS_BUILD_DEMO=OFF
cmake --build build
```

## 测试

```bash
cmake --build build --target cache_tests
ctest --test-dir build --output-on-failure
```

覆盖范围包含：

1. LRU、LFU、ARC 基础行为与容量约束。
2. LRU-K 与分片缓存路由行为。
3. TTL 过期语义。
4. CacheWithLoader 回源语义。

## 基准测试

```bash
cmake --build build --target cache_benchmarks
./build/benchmark/cache_benchmarks --benchmark_min_time=1s
```

仅运行部分基准：

```bash
./build/benchmark/cache_benchmarks --benchmark_filter='BM_(Lru|Lfu)_(MixedOps|HotSetGets)'
```

一键导出线程数-吞吐曲线（CSV + SVG）：

```bash
cmake --build build --target cache_benchmark_thread_curve
```

输出文件：

1. `build/benchmark/thread_curve.csv`
2. `build/benchmark/thread_curve.svg`
3. `build/benchmark/thread_curve_raw.txt`





## 实验评估

### 1) Trace-Driven 对比

```bash
cmake --build build --target cache_trace_compare
./build/evaluation/cache_trace_compare
```

使用外部轨迹：

```bash
./build/evaluation/cache_trace_compare --trace-file=trace.txt --capacities=64,128,256
```

### 2) 场景化命中率评估

```bash
cmake --build build --target cache_policy_scenarios
./build/evaluation/cache_policy_scenarios
```

### 3) 关于 OPT 对比口径

1. OPT 是给定容量下的理论最优离线下界。
2. 评估中若出现某策略优于 OPT，通常是容量口径不一致导致。
3. 当前 trace 评估已对 ARC 输入容量做口径归一，避免出现虚假的优于 OPT 结果。

## 演示 Demo

```bash
cmake --build build --target cache_demo
./build/demo/cache_demo
```

Demo 会展示：

1. LRU/LFU/ARC 基础策略行为。
2. CacheWithLoader 自动回源流程。
3. TTL 过期语义与组合缓存流程。



