#include <benchmark/benchmark.h>

#include "LfuCache.h"
#include "LruCache.h"
#include "common.h"

// 匿名命名空间：仅当前编译单元可见，避免符号冲突
namespace
{
    /**
     * LRU缓存-混合操作性能测试（75% GET + 25% PUT）
     * @param state range(0)=缓存容量，range(1)=随机key空间
     */
    void BM_Lru_MixedOps(benchmark::State &state)
    {
        const int capacity = static_cast<int>(state.range(0)); // 缓存容量
        const int keySpace = static_cast<int>(state.range(1)); // 随机key空间

        // 预填充缓存，模拟系统运行中的常态缓存状态
        CacheSys::LruCache<int, int> cache(capacity);
        for (int i = 0; i < capacity; ++i)
        {
            cache.put(i, i);
        }

        FastRng rng(0x12345678ULL); // 固定种子，保证结果可复现
        int sink = 0;               // 汇总值，防止编译器优化掉核心逻辑

        // Benchmark框架控制循环次数；每轮按概率执行GET或PUT
        for (auto _ : state)
        {
            const int key = static_cast<int>(rng.next() % static_cast<uint64_t>(keySpace));
            const bool doGet = (rng.next() & 3ULL) != 0ULL; // 75% GET，25% PUT
            if (doGet)
            {
                int value = 0;
                if (cache.get(key, value))
                {
                    sink ^= value;
                }
            }
            else
            {
                cache.put(key, key);
            }
        }

        // 告知编译器该变量有副作用，不可优化
        benchmark::DoNotOptimize(sink);
    }

    /**
     * LFU缓存-混合操作性能测试（逻辑同LRU，仅缓存类型不同）
     * @param state range(0)=缓存容量，range(1)=随机key空间
     */
    void BM_Lfu_MixedOps(benchmark::State &state)
    {
        const int capacity = static_cast<int>(state.range(0));
        const int keySpace = static_cast<int>(state.range(1));

        // 预填充缓存
        CacheSys::LfuCache<int, int> cache(capacity);
        for (int i = 0; i < capacity; ++i)
        {
            cache.put(i, i);
        }

        FastRng rng(0x87654321ULL); // 与LRU不同种子，避免完全同序列
        int sink = 0;

        for (auto _ : state)
        {
            const int key = static_cast<int>(rng.next() % static_cast<uint64_t>(keySpace));
            const bool doGet = (rng.next() & 3ULL) != 0ULL; // 75% GET，25% PUT
            if (doGet)
            {
                int value = 0;
                if (cache.get(key, value))
                {
                    sink ^= value;
                }
            }
            else
            {
                cache.put(key, key);
            }
        }

        benchmark::DoNotOptimize(sink);
    }

    /**
     * LRU缓存-热点GET性能测试（仅GET，访问小范围热点key）
     * @param state range(0)=缓存容量，range(1)=热点集合大小
     */
    void BM_Lru_HotSetGets(benchmark::State &state)
    {
        const int capacity = static_cast<int>(state.range(0)); // 缓存容量
        const int hotSet = static_cast<int>(state.range(1));   // 热点集合大小

        // 预填充缓存
        CacheSys::LruCache<int, int> cache(capacity);
        for (int i = 0; i < capacity; ++i)
        {
            cache.put(i, i);
        }

        FastRng rng(0x31415926ULL); // 固定种子，便于复现
        int sink = 0;

        // 仅GET操作，聚焦热点访问场景
        for (auto _ : state)
        {
            const int key = static_cast<int>(rng.next() % static_cast<uint64_t>(hotSet));
            int value = 0;
            if (cache.get(key, value))
            {
                sink += value;
            }
        }

        benchmark::DoNotOptimize(sink);
    }

    /**
     * LFU缓存-热点GET性能测试（逻辑同LRU，仅缓存类型不同）
     * @param state range(0)=缓存容量，range(1)=热点集合大小
     */
    void BM_Lfu_HotSetGets(benchmark::State &state)
    {
        const int capacity = static_cast<int>(state.range(0));
        const int hotSet = static_cast<int>(state.range(1));

        // 预填充缓存
        CacheSys::LfuCache<int, int> cache(capacity);
        for (int i = 0; i < capacity; ++i)
        {
            cache.put(i, i);
        }

        FastRng rng(0x27182818ULL); // 固定种子，便于复现
        int sink = 0;

        // 仅GET操作，聚焦热点访问场景
        for (auto _ : state)
        {
            const int key = static_cast<int>(rng.next() % static_cast<uint64_t>(hotSet));
            int value = 0;
            if (cache.get(key, value))
            {
                sink += value;
            }
        }

        benchmark::DoNotOptimize(sink);
    }

    // 注册LRU混合操作测试
    BENCHMARK(BM_Lru_MixedOps)
        ->Args({1024, 4096})
        ->Args({4096, 16384})
        ->MinTime(0.5);

    // 注册LFU混合操作测试
    BENCHMARK(BM_Lfu_MixedOps)
        ->Args({1024, 4096})
        ->Args({4096, 16384})
        ->MinTime(0.5);

    // 注册LRU热点GET测试
    BENCHMARK(BM_Lru_HotSetGets)
        ->Args({1024, 64})
        ->Args({4096, 128})
        ->MinTime(0.5);

    // 注册LFU热点GET测试
    BENCHMARK(BM_Lfu_HotSetGets)
        ->Args({1024, 64})
        ->Args({4096, 128})
        ->MinTime(0.5);
}
