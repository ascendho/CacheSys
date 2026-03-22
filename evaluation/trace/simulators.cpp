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
    // 模拟LRU缓存策略执行，返回未命中数/未命中率
    // trace：缓存访问轨迹序列（如商品ID访问顺序）
    // capacity：缓存最大容量
    EvalResult simulateLru(const std::vector<int> &trace, int capacity)
    {
        // 容量非法：所有访问均未命中
        if (capacity <= 0)
        {
            return EvalResult{trace.size(), trace.empty() ? 0.0 : 1.0};
        }

        CacheSys::LruCache<int, int> cache(capacity); // 创建LRU缓存实例
        size_t misses = 0;                            // 未命中次数统计
        for (int key : trace)
        {
            int value = 0;

            // 缓存未命中：计数+写入缓存
            if (!cache.get(key, value))
            {
                ++misses;
                cache.put(key, key);
            }
        }

        // 计算未命中率并返回结果
        EvalResult result;
        result.misses = misses;
        result.missRatio = trace.empty() ? 0.0 : static_cast<double>(misses) / static_cast<double>(trace.size());
        return result;
    }

    // 模拟LFU缓存策略执行，返回未命中数/未命中率（逻辑同LRU，仅缓存实例不同）
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

    // 模拟ARC缓存策略执行，返回未命中数/未命中率
    EvalResult simulateArc(const std::vector<int> &trace, int capacity)
    {
        if (capacity <= 0)
        {
            return EvalResult{trace.size(), trace.empty() ? 0.0 : 1.0};
        }

        // 当前 ArcCache 实现内部包含 LRU/LFU 两个子区，且都按构造容量初始化。
        // 为保证与 LRU/LFU/OPT 的容量口径一致，这里做一次折算输入。
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

    // 模拟OPT（最优替换）缓存策略执行（理论最优基准，仅离线模拟用）
    EvalResult simulateOpt(const std::vector<int> &trace, int capacity)
    {
        if (capacity <= 0)
        {
            return EvalResult{trace.size(), trace.empty() ? 0.0 : 1.0};
        }

        // 预存每个key在轨迹中出现的所有位置（用于判断"未来是否使用"）
        std::unordered_map<int, std::vector<size_t>> positions;
        positions.reserve(trace.size());
        for (size_t i = 0; i < trace.size(); ++i)
        {
            positions[trace[i]].push_back(i);
        }

        // 记录每个key已消费的位置索引（避免重复判断）
        std::unordered_map<int, size_t> consumed;
        consumed.reserve(positions.size());

        // 模拟OPT缓存容器（仅存key，无value）
        std::unordered_set<int> cache;
        cache.reserve(static_cast<size_t>(capacity * 2));

        size_t misses = 0;

        // 标记"未来不再使用"
        const size_t inf = std::numeric_limits<size_t>::max();

        // 计算key的下一次使用位置：返回inf表示未来不再使用
        auto nextUse = [&](int key) -> size_t
        {
            const auto pIt = positions.find(key);
            if (pIt == positions.end())
                return inf;

            const auto cIt = consumed.find(key);
            const size_t used = (cIt == consumed.end()) ? 0 : cIt->second;
            if (used >= pIt->second.size())
                return inf; // 已用完所有出现位置

            // 返回下一次出现的索引
            return pIt->second[used];
        };

        // 遍历访问轨迹，模拟OPT核心逻辑：淘汰"未来最久不使用"的key
        for (int key : trace)
        {
            // 标记当前key的位置已消费
            consumed[key]++;

            // 缓存命中：跳过
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

            // 缓存已满：选择"未来最久不使用"的key淘汰
            int victim = *cache.begin();
            size_t farthest = 0;
            bool victimChosen = false;

            for (int candidate : cache)
            {
                const size_t nu = nextUse(candidate);

                // 选下一次使用位置最远（或永不使用）的key作为淘汰目标
                if (!victimChosen || nu > farthest)
                {
                    victim = candidate;
                    farthest = nu;
                    victimChosen = true;
                }
            }

            cache.erase(victim); // 淘汰目标key
            cache.insert(key);   // 插入当前key
        }

        // 计算未命中率并返回
        EvalResult result;
        result.misses = misses;
        result.missRatio = trace.empty() ? 0.0 : static_cast<double>(misses) / static_cast<double>(trace.size());
        return result;
    }
}