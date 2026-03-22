#include "evaluator.h"

#include "ArcCache.h"
#include "LfuCache.h"
#include "LruCache.h"

// Eval：评估模块 → Scenario：场景化测试子模块（策略评估核心实现）
namespace CacheSys::Eval::Scenario
{
    // 评估LRU策略在PUT/GET混合操作序列下的GET命中指标
    // ops：PUT/GET混合操作序列 | capacity：缓存容量
    // 返回：仅统计GET的总次数和命中次数（PUT仅执行，不参与统计）
    EvalResult evalLru(const std::vector<Operation> &ops, int capacity)
    {
        CacheSys::LruCache<int, int> cache(capacity); // 创建LRU缓存实例
        EvalResult result;                            // 评估结果（gets=GET总数，hits=GET命中数）

        // 遍历所有操作，模拟缓存执行并统计命中
        for (const Operation &op : ops)
        {
            if (op.isPut)
            {
                // PUT操作：直接写入缓存，不统计
                cache.put(op.key, op.value); 
                continue;
            }

            // GET操作：总次数+1
            ++result.gets; 
            int value = 0;

            // GET命中：命中数+1
            if (cache.get(op.key, value)) 
            {
                ++result.hits;
            }
        }

        return result;
    }

    // 评估LFU策略在PUT/GET混合操作序列下的GET命中指标（逻辑同LRU，仅缓存实例不同）
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

    // 评估ARC策略在PUT/GET混合操作序列下的GET命中指标（逻辑同LRU，仅缓存实例不同）
    EvalResult evalArc(const std::vector<Operation> &ops, int capacity)
    {
        // ARC容量需转为size_t类型，且内部实现分为LRU/LFU两部分，这里直接传入总容量，由ARC内部折算分配
        CacheSys::ArcCache<int, int> cache(static_cast<size_t>(capacity)); 
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
}