#include "evaluator.h"
#include "ArcCache.h"
#include "LfuCache.h"
#include "LruCache.h"

/**
 * @brief 缓存评估场景执行命名空间
 */
namespace CacheSys::Eval::Scenario
{
    /**
     * @brief 执行 LRU 算法的仿真评估
     * @param ops 待执行的操作指令序列
     * @param capacity 仿真缓存的容量
     * @return EvalResult 包含总请求数和命中次数的统计结果
     */
    EvalResult evalLru(const std::vector<Operation> &ops, int capacity)
    {
        // 实例化一个标准的 LRU 缓存（Key 和 Value 均为 int 以简化仿真）
        CacheSys::LruCache<int, int> cache(capacity); 
        EvalResult result;                            

        // 遍历并回放所有预设操作
        for (const Operation &op : ops)
        {
            if (op.isPut)
            {
                // 如果是写入操作，将数据存入缓存
                cache.put(op.key, op.value); 
                continue;
            }

            // 如果是读取操作，记录总读取次数
            ++result.gets; 
            int value = 0;

            // 尝试获取数据并统计是否命中
            if (cache.get(op.key, value)) 
            {
                ++result.hits; // 命中计数增加
            }
        }

        return result;
    }

    /**
     * @brief 执行 LFU 算法的仿真评估
     * 逻辑同上，但底层使用的是频率优先的 LfuCache 策略。
     */
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

    /**
     * @brief 执行 ARC 算法的仿真评估
     * 逻辑同上，但底层使用的是自适应的 ArcCache 策略。
     */
    EvalResult evalArc(const std::vector<Operation> &ops, int capacity)
    {
        // 注意：ArcCache 的构造函数接收 size_t 类型，执行显式类型转换
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