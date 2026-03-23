#pragma once

#include <vector>

#include "types.h"

/**
 * @brief 缓存评估场景空间
 * 该命名空间提供了用于模拟和对比不同缓存淘汰算法性能的仿真接口。
 */
namespace CacheSys::Eval::Scenario
{
    /**
     * @brief 对 LRU (Least Recently Used) 算法进行仿真评估
     * 
     * 模拟 LRU 逻辑：维护数据的访问时间顺序，当容量满时淘汰最久未访问的项。
     * 
     * @param ops 预先生成的缓存操作指令序列 (Put/Get)
     * @param capacity 仿真过程中设定的缓存最大容量
     * @return EvalResult 包含该场景下 LRU 算法的命中总数和请求总数
     */
    EvalResult evalLru(const std::vector<Operation> &ops, int capacity);

    /**
     * @brief 对 LFU (Least Frequently Used) 算法进行仿真评估
     * 
     * 模拟 LFU 逻辑：根据数据的访问频率进行排序，当容量满时淘汰访问次数最少的项。
     * 
     * @param ops 预先生成的缓存操作指令序列 (Put/Get)
     * @param capacity 仿真过程中设定的缓存最大容量
     * @return EvalResult 包含该场景下 LFU 算法的命中总数和请求总数
     */
    EvalResult evalLfu(const std::vector<Operation> &ops, int capacity);

    /**
     * @brief 对 ARC (Adaptive Replacement Cache) 算法进行仿真评估
     * 
     * 模拟 ARC 逻辑：自适应地在最近访问（Recency）和最常访问（Frequency）之间寻找平衡。
     * 
     * @param ops 预先生成的缓存操作指令序列 (Put/Get)
     * @param capacity 仿真过程中设定的缓存最大容量
     * @return EvalResult 包含该场景下 ARC 算法的命中总数和请求总数
     */
    EvalResult evalArc(const std::vector<Operation> &ops, int capacity);
}