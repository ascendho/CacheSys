#include <benchmark/benchmark.h>

#include <memory>
#include <mutex>

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

    /**
     * 分片LRU-多线程混合操作测试（共享同一缓存实例）
     * @param state range(0)=总容量，range(1)=key空间，range(2)=分片数
     */
    void BM_ShardedLru_MixedOps_MT(benchmark::State &state)
    {
        const int capacity = static_cast<int>(state.range(0));
        const int keySpace = static_cast<int>(state.range(1));
        const int shardNum = static_cast<int>(state.range(2));

        using CacheType = CacheSys::ShardedLruCache<int, int>;
        static std::mutex initMutex;
        static std::shared_ptr<CacheType> sharedCache;
        static int lastCapacity = -1;
        static int lastKeySpace = -1;
        static int lastShardNum = -1;

        std::shared_ptr<CacheType> cache;
        {
            std::lock_guard<std::mutex> lock(initMutex);
            if (!sharedCache ||
                lastCapacity != capacity ||
                lastKeySpace != keySpace ||
                lastShardNum != shardNum)
            {
                sharedCache = std::make_shared<CacheType>(static_cast<size_t>(capacity), shardNum);
                for (int i = 0; i < capacity; ++i)
                {
                    sharedCache->put(i, i);
                }
                lastCapacity = capacity;
                lastKeySpace = keySpace;
                lastShardNum = shardNum;
            }
            cache = sharedCache;
        }

        FastRng rng(0xA1B2C3D4ULL + static_cast<uint64_t>(state.thread_index()));
        int sink = 0;

        for (auto _ : state)
        {
            const int key = static_cast<int>(rng.next() % static_cast<uint64_t>(keySpace));
            const bool doGet = (rng.next() & 3ULL) != 0ULL;
            if (doGet)
            {
                int value = 0;
                if (cache->get(key, value))
                {
                    sink ^= value;
                }
            }
            else
            {
                cache->put(key, key);
            }
        }

        benchmark::DoNotOptimize(sink);
        state.SetItemsProcessed(state.iterations());
        state.counters["ops/s"] = benchmark::Counter(static_cast<double>(state.iterations()), benchmark::Counter::kIsRate);
    }

    /**
     * 分片LFU-多线程混合操作测试（共享同一缓存实例）
     * @param state range(0)=总容量，range(1)=key空间，range(2)=分片数
     */
    void BM_ShardedLfu_MixedOps_MT(benchmark::State &state)
    {
        const int capacity = static_cast<int>(state.range(0));
        const int keySpace = static_cast<int>(state.range(1));
        const int shardNum = static_cast<int>(state.range(2));

        using CacheType = CacheSys::ShardedLfuCache<int, int>;
        static std::mutex initMutex;
        static std::shared_ptr<CacheType> sharedCache;
        static int lastCapacity = -1;
        static int lastKeySpace = -1;
        static int lastShardNum = -1;

        std::shared_ptr<CacheType> cache;
        {
            std::lock_guard<std::mutex> lock(initMutex);
            if (!sharedCache ||
                lastCapacity != capacity ||
                lastKeySpace != keySpace ||
                lastShardNum != shardNum)
            {
                sharedCache = std::make_shared<CacheType>(static_cast<size_t>(capacity), shardNum, 1000000);
                for (int i = 0; i < capacity; ++i)
                {
                    sharedCache->put(i, i);
                }
                lastCapacity = capacity;
                lastKeySpace = keySpace;
                lastShardNum = shardNum;
            }
            cache = sharedCache;
        }

        FastRng rng(0xE5F60718ULL + static_cast<uint64_t>(state.thread_index()));
        int sink = 0;

        for (auto _ : state)
        {
            const int key = static_cast<int>(rng.next() % static_cast<uint64_t>(keySpace));
            const bool doGet = (rng.next() & 3ULL) != 0ULL;
            if (doGet)
            {
                int value = 0;
                if (cache->get(key, value))
                {
                    sink ^= value;
                }
            }
            else
            {
                cache->put(key, key);
            }
        }

        benchmark::DoNotOptimize(sink);
        state.SetItemsProcessed(state.iterations());
        state.counters["ops/s"] = benchmark::Counter(static_cast<double>(state.iterations()), benchmark::Counter::kIsRate);
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

    // 注册分片LRU多线程混合测试（吞吐曲线）
    BENCHMARK(BM_ShardedLru_MixedOps_MT)
        ->Args({16384, 65536, 16})
        ->Threads(1)
        ->Threads(2)
        ->Threads(4)
        ->Threads(8)
        ->UseRealTime()
        ->MinTime(0.5);

    // 注册分片LFU多线程混合测试（吞吐曲线）
    BENCHMARK(BM_ShardedLfu_MixedOps_MT)
        ->Args({16384, 65536, 16})
        ->Threads(1)
        ->Threads(2)
        ->Threads(4)
        ->Threads(8)
        ->UseRealTime()
        ->MinTime(0.5);
}
