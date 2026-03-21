#include <benchmark/benchmark.h>

#include "LfuCache.h"
#include "LruCache.h"
#include "common.h"

namespace
{
    void BM_Lru_MixedOps(benchmark::State &state)
    {
        const int capacity = static_cast<int>(state.range(0));
        const int keySpace = static_cast<int>(state.range(1));

        CacheSys::LruCache<int, int> cache(capacity);
        for (int i = 0; i < capacity; ++i)
        {
            cache.put(i, i);
        }

        FastRng rng(0x12345678ULL);
        int sink = 0;

        for (auto _ : state)
        {
            const int key = static_cast<int>(rng.next() % static_cast<uint64_t>(keySpace));
            const bool doGet = (rng.next() & 3ULL) != 0ULL; // 75% get, 25% put
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

    void BM_Lfu_MixedOps(benchmark::State &state)
    {
        const int capacity = static_cast<int>(state.range(0));
        const int keySpace = static_cast<int>(state.range(1));

        CacheSys::LfuCache<int, int> cache(capacity);
        for (int i = 0; i < capacity; ++i)
        {
            cache.put(i, i);
        }

        FastRng rng(0x87654321ULL);
        int sink = 0;

        for (auto _ : state)
        {
            const int key = static_cast<int>(rng.next() % static_cast<uint64_t>(keySpace));
            const bool doGet = (rng.next() & 3ULL) != 0ULL; // 75% get, 25% put
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

    void BM_Lru_HotSetGets(benchmark::State &state)
    {
        const int capacity = static_cast<int>(state.range(0));
        const int hotSet = static_cast<int>(state.range(1));

        CacheSys::LruCache<int, int> cache(capacity);
        for (int i = 0; i < capacity; ++i)
        {
            cache.put(i, i);
        }

        FastRng rng(0x31415926ULL);
        int sink = 0;

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

    void BM_Lfu_HotSetGets(benchmark::State &state)
    {
        const int capacity = static_cast<int>(state.range(0));
        const int hotSet = static_cast<int>(state.range(1));

        CacheSys::LfuCache<int, int> cache(capacity);
        for (int i = 0; i < capacity; ++i)
        {
            cache.put(i, i);
        }

        FastRng rng(0x27182818ULL);
        int sink = 0;

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

    BENCHMARK(BM_Lru_MixedOps)
        ->Args({1024, 4096})
        ->Args({4096, 16384})
        ->MinTime(0.5);

    BENCHMARK(BM_Lfu_MixedOps)
        ->Args({1024, 4096})
        ->Args({4096, 16384})
        ->MinTime(0.5);

    BENCHMARK(BM_Lru_HotSetGets)
        ->Args({1024, 64})
        ->Args({4096, 128})
        ->MinTime(0.5);

    BENCHMARK(BM_Lfu_HotSetGets)
        ->Args({1024, 64})
        ->Args({4096, 128})
        ->MinTime(0.5);
} // namespace
