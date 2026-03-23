## CacheSys

CacheSys 是基于 C++ 开发的一个可扩展缓存系统，主要用于学习和实践缓存相关技术。该系统里实现了多种经典的缓存淘汰算法，包括 LRU（最近最少使用）、LFU（最不经常使用）、ARC（自适应替换缓存）、LRU-K，同时也支持分片缓存的功能。 

除了核心的缓存淘汰算法外，该项目还通过组件化的方式给 CacheSys 补充了完整的缓存功能：比如支持带过期时间的缓存控制（TTL 装饰），实现了 CacheWithLoader 组件，能在缓存未命中时自动从数据源加载数据，也做了 CacheManager 来集中管理不同的缓存实例和相关参数规则。 

为了让这个缓存系统用起来更方便，本项目采用了基于配置驱动的方式（RuntimeConfig / StrategySelector）来自动装配缓存组件，降低了使用时的接入成本。另外，我还做了一套完整的验证环节来保证功能正确和性能达标：test 模块通过自动化测试验证缓存的容量限制、路由逻辑、TTL 规则和自动加载数据的功能是否正确；benchmark 模块用来测试缓存在高频读写混合场景下的底层性能；evaluation 模块可以加载外部的访问轨迹，评估不同缓存策略的命中率，还能把这些策略和理论最优的 OPT 算法做对比，为优化缓存策略提供数据参考。

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
5. CacheManager 管理与参数校验。
6. StrategySelector 与 RuntimeConfig 装配流程。

## 基准测试

```bash
cmake --build build --target cache_benchmarks
./build/benchmark/cache_benchmarks --benchmark_min_time=1s
```

仅运行部分基准：

```bash
./build/benchmark/cache_benchmarks --benchmark_filter='BM_(Lru|Lfu)_(MixedOps|HotSetGets)'
```





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

1. 策略推荐。
2. 配置文件驱动装配。
3. 典型缓存访问流程。



