/*
 * 1. cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
 * 2. cmake --build build --target cache_policy_scenarios
 * 3. ./build/trace/cache_policy_scenarios
 *
 */

#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#include "LfuCache.h"
#include "LruCache.h"

namespace
{
    struct Operation
    {
        bool isPut = false;
        int key = 0;
        int value = 0;
    };

    struct EvalResult
    {
        size_t gets = 0;
        size_t hits = 0;
    };

    std::vector<Operation> makeHotDataScenarioOps(int operations, int hotKeys, int coldKeys)
    {
        std::vector<Operation> ops;
        ops.reserve(static_cast<size_t>(operations));

        std::mt19937 rng(20260316);
        std::uniform_int_distribution<int> p01to100(1, 100);
        std::uniform_int_distribution<int> hotDist(0, hotKeys - 1);
        std::uniform_int_distribution<int> coldDist(0, coldKeys - 1);

        for (int i = 0; i < operations; ++i)
        {
            const bool isPut = p01to100(rng) <= 30;  // 30% write, 70% read
            const bool hitHot = p01to100(rng) <= 70; // 70% hotset, 30% coldset
            const int key = hitHot ? hotDist(rng) : (hotKeys + coldDist(rng));

            ops.push_back(Operation{isPut, key, key * 10 + (i % 97)});
        }

        return ops;
    }

    std::vector<Operation> makeLoopScanScenarioOps(int operations, int loopSize)
    {
        std::vector<Operation> ops;
        ops.reserve(static_cast<size_t>(operations));

        std::mt19937 rng(20260317);
        std::uniform_int_distribution<int> p01to100(1, 100);
        std::uniform_int_distribution<int> inRange(0, loopSize - 1);
        std::uniform_int_distribution<int> outRange(0, loopSize - 1);

        int cursor = 0;
        for (int i = 0; i < operations; ++i)
        {
            const bool isPut = p01to100(rng) <= 20; // 20% write
            int key = 0;

            const int selector = p01to100(rng);
            if (selector <= 60)
            {
                key = cursor;
                cursor = (cursor + 1) % loopSize;
            }
            else if (selector <= 90)
            {
                key = inRange(rng);
            }
            else
            {
                key = loopSize + outRange(rng);
            }

            ops.push_back(Operation{isPut, key, key * 10 + (i % 113)});
        }

        return ops;
    }

    std::vector<Operation> makeWorkloadShiftScenarioOps(int operations)
    {
        std::vector<Operation> ops;
        ops.reserve(static_cast<size_t>(operations));

        std::mt19937 rng(20260318);
        std::uniform_int_distribution<int> p01to100(1, 100);

        const int phaseLen = operations / 5;

        for (int i = 0; i < operations; ++i)
        {
            const int phase = i / phaseLen;

            int putProbability = 20;
            switch (phase)
            {
            case 0:
                putProbability = 15;
                break;
            case 1:
                putProbability = 30;
                break;
            case 2:
                putProbability = 10;
                break;
            case 3:
                putProbability = 25;
                break;
            default:
                putProbability = 20;
                break;
            }

            const bool isPut = p01to100(rng) <= putProbability;
            int key = 0;

            if (phase == 0)
            {
                key = p01to100(rng) % 5;
            }
            else if (phase == 1)
            {
                key = p01to100(rng) % 400;
            }
            else if (phase == 2)
            {
                key = (i - phaseLen * 2) % 100;
            }
            else if (phase == 3)
            {
                const int locality = (i / 800) % 5;
                key = locality * 15 + (p01to100(rng) % 15);
            }
            else
            {
                const int r = p01to100(rng);
                if (r <= 40)
                {
                    key = p01to100(rng) % 5;
                }
                else if (r <= 70)
                {
                    key = 5 + (p01to100(rng) % 45);
                }
                else
                {
                    key = 50 + (p01to100(rng) % 350);
                }
            }

            ops.push_back(Operation{isPut, key, key * 10 + (phase * 100 + i % 97)});
        }

        return ops;
    }

    EvalResult evalLru(const std::vector<Operation> &ops, int capacity)
    {
        CacheSys::LruCache<int, int> cache(capacity);
        EvalResult result;

        for (const Operation &op : ops)
        {
            if (op.isPut)
            {
                cache.put(op.key, op.value);
                continue;
            }

            ++result.gets;
            int value = 0;
            if (cache.get(op.key, value))
            {
                ++result.hits;
            }
        }

        return result;
    }

    EvalResult evalLfu(const std::vector<Operation> &ops, int capacity)
    {
        CacheSys::LfuCache<int, int> cache(capacity);
        EvalResult result;

        for (const Operation &op : ops)
        {
            if (op.isPut)
            {
                cache.put(op.key, op.value);
                continue;
            }

            ++result.gets;
            int value = 0;
            if (cache.get(op.key, value))
            {
                ++result.hits;
            }
        }

        return result;
    }

    void printScenarioSummary(const std::string &title, int capacity, const EvalResult &lru, const EvalResult &lfu)
    {
        const double lruHitRate = lru.gets == 0 ? 0.0 : (100.0 * static_cast<double>(lru.hits) / static_cast<double>(lru.gets));
        const double lfuHitRate = lfu.gets == 0 ? 0.0 : (100.0 * static_cast<double>(lfu.hits) / static_cast<double>(lfu.gets));

        std::cout << "\n=== " << title << " ===\n";
        std::cout << "缓存大小: " << capacity << "\n";

        std::cout << std::fixed << std::setprecision(2);
        std::cout << "LRU - 命中率: " << lruHitRate << "% (" << lru.hits << "/" << lru.gets << ")\n";
        std::cout << "LFU - 命中率: " << lfuHitRate << "% (" << lfu.hits << "/" << lfu.gets << ")\n";
    }

} // namespace

int main()
{
    {
        const int capacity = 20;
        const std::vector<Operation> ops = makeHotDataScenarioOps(500000, 20, 5000);
        const EvalResult lru = evalLru(ops, capacity);
        const EvalResult lfu = evalLfu(ops, capacity);
        printScenarioSummary("测试场景1：热点数据访问测试", capacity, lru, lfu);
    }

    {
        const int capacity = 50;
        const std::vector<Operation> ops = makeLoopScanScenarioOps(200000, 500);
        const EvalResult lru = evalLru(ops, capacity);
        const EvalResult lfu = evalLfu(ops, capacity);
        printScenarioSummary("测试场景2：循环扫描测试", capacity, lru, lfu);
    }

    {
        const int capacity = 30;
        const std::vector<Operation> ops = makeWorkloadShiftScenarioOps(80000);
        const EvalResult lru = evalLru(ops, capacity);
        const EvalResult lfu = evalLfu(ops, capacity);
        printScenarioSummary("测试场景3：工作负载剧烈变化测试", capacity, lru, lfu);
    }

    return 0;
}
