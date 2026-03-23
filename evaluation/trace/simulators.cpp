#include "simulators.h"

#include <algorithm>
#include <limits>
#include <unordered_map>
#include <unordered_set>

#include "ArcCache.h"
#include "LfuCache.h"
#include "LruCache.h"

// Eval：评估模块 → Trace：基于访问轨迹模拟缓存策略执行
namespace CacheSys::Eval::Trace
{
    // 模拟LRU策略，返回未命中次数和未命中率
    // trace：访问轨迹 | capacity：缓存容量
    EvalResult simulateLru(const std::vector<int> &trace, int capacity)
    {
        // 容量无效：视为所有访问均未命中
        if (capacity <= 0)
        {
            return EvalResult{trace.size(), trace.empty() ? 0.0 : 1.0};
        }

        CacheSys::LruCache<int, int> cache(capacity);
        size_t misses = 0;
        for (int key : trace)
        {
            int value = 0;

            // 未命中则写回 key 作为 value，便于统一仿真流程
            if (!cache.get(key, value))
            {
                ++misses;
                cache.put(key, key);
            }
        }

        // 汇总结果
        EvalResult result;
        result.misses = misses;
        result.missRatio = trace.empty() ? 0.0 : static_cast<double>(misses) / static_cast<double>(trace.size());
        return result;
    }

    // 模拟LFU策略（流程与LRU一致，仅缓存实现不同）
    EvalResult simulateLfu(const std::vector<int> &trace, int capacity)
    {
        if (capacity <= 0)
        {
            return EvalResult{trace.size(), trace.empty() ? 0.0 : 1.0};
        }

        CacheSys::LfuCache<int, int> cache(capacity);
        size_t misses = 0;
        for (int key : trace)
        {
            int value = 0;
            if (!cache.get(key, value))
            {
                ++misses;
                cache.put(key, key);
            }
        }

        EvalResult result;
        result.misses = misses;
        result.missRatio = trace.empty() ? 0.0 : static_cast<double>(misses) / static_cast<double>(trace.size());
        return result;
    }

    // 模拟ARC策略
    EvalResult simulateArc(const std::vector<int> &trace, int capacity)
    {
        if (capacity <= 0)
        {
            return EvalResult{trace.size(), trace.empty() ? 0.0 : 1.0};
        }

        // 当前ArcCache实现内部包含LRU/LFU两个部分，按统一容量口径进行折算输入
        const size_t arcInputCapacity = static_cast<size_t>(std::max(1, capacity / 2));
        CacheSys::ArcCache<int, int> cache(arcInputCapacity);
        size_t misses = 0;
        for (int key : trace)
        {
            int value = 0;
            if (!cache.get(key, value))
            {
                ++misses;
                cache.put(key, key);
            }
        }

        EvalResult result;
        result.misses = misses;
        result.missRatio = trace.empty() ? 0.0 : static_cast<double>(misses) / static_cast<double>(trace.size());
        return result;
    }

    // 模拟OPT（离线理论最优）策略，作为对照基准
    EvalResult simulateOpt(const std::vector<int> &trace, int capacity)
    {
        if (capacity <= 0)
        {
            return EvalResult{trace.size(), trace.empty() ? 0.0 : 1.0};
        }

        // positions: 预存每个key在轨迹中出现的位置列表
        std::unordered_map<int, std::vector<size_t>> positions;
        positions.reserve(trace.size());
        for (size_t i = 0; i < trace.size(); ++i)
        {
            positions[trace[i]].push_back(i);
        }

        // consumed: 记录每个key当前位置已消费到第几次出现
        std::unordered_map<int, size_t> consumed;
        consumed.reserve(positions.size());

        // cache: OPT缓存集合（仅存key）
        std::unordered_set<int> cache;
        cache.reserve(static_cast<size_t>(capacity * 2));

        size_t misses = 0;

        // inf：表示未来不再访问
        const size_t inf = std::numeric_limits<size_t>::max();

        // 返回key下一次访问位置；若未来不再访问则返回inf
        auto nextUse = [&](int key) -> size_t
        {
            const auto pIt = positions.find(key);
            if (pIt == positions.end())
                return inf;

            const auto cIt = consumed.find(key);
            const size_t used = (cIt == consumed.end()) ? 0 : cIt->second;
            if (used >= pIt->second.size())
                return inf;

            // 下一个尚未消费的位置
            return pIt->second[used];
        };

        // 逐步模拟访问，命中跳过，未命中执行插入/淘汰
        for (int key : trace)
        {
            // 当前访问位置计入已消费
            consumed[key]++;

            // 命中：无需变更
            if (cache.find(key) != cache.end())
            {
                continue;
            }

            // 未命中计数
            ++misses;

            // 缓存未满：直接插入
            if (cache.size() < static_cast<size_t>(capacity))
            {
                cache.insert(key);
                continue;
            }

            // 缓存已满：淘汰“下一次使用最远”的key
            int victim = *cache.begin();
            size_t farthest = 0;
            bool victimChosen = false;

            for (int candidate : cache)
            {
                const size_t nu = nextUse(candidate);

                // 选择下一次访问位置最远（或不再访问）的候选
                if (!victimChosen || nu > farthest)
                {
                    victim = candidate;
                    farthest = nu;
                    victimChosen = true;
                }
            }

            cache.erase(victim); // 淘汰
            cache.insert(key);   // 插入当前key
        }

        // 汇总结果
        EvalResult result;
        result.misses = misses;
        result.missRatio = trace.empty() ? 0.0 : static_cast<double>(misses) / static_cast<double>(trace.size());
        return result;
    }
}