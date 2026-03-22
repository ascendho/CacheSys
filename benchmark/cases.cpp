#include <benchmark/benchmark.h>

#include "LfuCache.h"
#include "LruCache.h"
#include "common.h"

// 匿名命名空间：仅当前编译单元可见，避免符号冲突
namespace
{
    /**
     * LRU缓存-混合操作性能测试（75% GET + 25% PUT）
     * @param state Benchmark框架状态对象，range(0)=缓存容量，range(1)=key总空间大小
     */
    void BM_Lru_MixedOps(benchmark::State &state)
    {
        const int capacity = static_cast<int>(state.range(0)); // 缓存容量
        const int keySpace = static_cast<int>(state.range(1)); // 随机key的取值范围

        // 初始化LRU缓存，预填充capacity个key（模拟缓存已有数据）
        CacheSys::LruCache<int, int> cache(capacity);
        for (int i = 0; i < capacity; ++i)
        {
            cache.put(i, i);
        }

        FastRng rng(0x12345678ULL); // 固定种子，保证测试可复现
        int sink = 0;               // 接收缓存返回值，避免编译器优化掉核心逻辑

        // 循环执行测试（Benchmark框架自动控制迭代次数，统计耗时）
        for (auto _ : state)
        {
            const int key = static_cast<int>(rng.next() % static_cast<uint64_t>(keySpace)); // 随机生成key
            const bool doGet = (rng.next() & 3ULL) != 0ULL;                                 // 位运算控制比例：75% GET，25% PUT
            if (doGet)
            {
                // GET操作：命中则更新sink（防止优化）
                int value = 0;
                if (cache.get(key, value))
                {
                    sink ^= value;
                }
            }
            else
            {
                // PUT操作：写入随机key-value
                cache.put(key, key);
            }
        }

        // 显式告诉编译器不优化sink，保证测试准确性
        benchmark::DoNotOptimize(sink); 
    }

    /**
     * LFU缓存-混合操作性能测试（逻辑同LRU，仅缓存类型不同）
     * @param state range(0)=缓存容量，range(1)=key总空间大小
     */
    void BM_Lfu_MixedOps(benchmark::State &state)
    {
        const int capacity = static_cast<int>(state.range(0));
        const int keySpace = static_cast<int>(state.range(1));

        // 初始化LFU缓存，预填充数据
        CacheSys::LfuCache<int, int> cache(capacity);
        for (int i = 0; i < capacity; ++i)
        {
            cache.put(i, i);
        }

        // 不同种子，避免和LRU生成完全相同的key序列
        FastRng rng(0x87654321ULL); 
        int sink = 0;

        for (auto _ : state)
        {
            const int key = static_cast<int>(rng.next() % static_cast<uint64_t>(keySpace));

            // 75% GET，25% PUT
            const bool doGet = (rng.next() & 3ULL) != 0ULL; 
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
     * @param state range(0)=缓存容量，range(1)=热点key集合大小
     */
    void BM_Lru_HotSetGets(benchmark::State &state)
    {
        const int capacity = static_cast<int>(state.range(0)); // 缓存容量
        const int hotSet = static_cast<int>(state.range(1));   // 热点key的取值范围（小集合）

        // 初始化LRU缓存，预填充数据
        CacheSys::LruCache<int, int> cache(capacity);
        for (int i = 0; i < capacity; ++i)
        {
            cache.put(i, i);
        }

        // 固定种子，可复现
        FastRng rng(0x31415926ULL); 
        int sink = 0;

        // 仅执行GET操作，访问小范围热点key（模拟爆款商品等高频访问场景）
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
     * @param state range(0)=缓存容量，range(1)=热点key集合大小
     */
    void BM_Lfu_HotSetGets(benchmark::State &state)
    {
        const int capacity = static_cast<int>(state.range(0));
        const int hotSet = static_cast<int>(state.range(1));

        // 初始化LFU缓存，预填充数据
        CacheSys::LfuCache<int, int> cache(capacity);
        for (int i = 0; i < capacity; ++i)
        {
            cache.put(i, i);
        }

        // 不同种子，避免和LRU生成完全相同的key序列
        FastRng rng(0x27182818ULL);
        int sink = 0;

        // 仅GET操作，访问小热点集
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

    // 注册LRU混合操作测试，指定两组参数（容量, key空间），最小运行0.5秒保证统计稳定
    BENCHMARK(BM_Lru_MixedOps)
        ->Args({1024, 4096})
        ->Args({4096, 16384})
        ->MinTime(0.5);

    // 注册LFU混合操作测试，参数和LRU一致，保证对比公平
    BENCHMARK(BM_Lfu_MixedOps)
        ->Args({1024, 4096})
        ->Args({4096, 16384})
        ->MinTime(0.5);

    // 注册LRU热点GET测试，参数（容量, 热点集大小），最小运行0.5秒
    BENCHMARK(BM_Lru_HotSetGets)
        ->Args({1024, 64})
        ->Args({4096, 128})
        ->MinTime(0.5);

    // 注册LFU热点GET测试，参数和LRU一致
    BENCHMARK(BM_Lfu_HotSetGets)
        ->Args({1024, 64})
        ->Args({4096, 128})
        ->MinTime(0.5);
}