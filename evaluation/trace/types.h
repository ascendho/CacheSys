#pragma once

#include <cstddef>

// Eval：评估模块（Evaluation）
// Trace：追踪子模块（记录缓存访问轨迹）
namespace CacheSys::Eval::Trace
{
    /**
     * 缓存策略评估结果结构体
     * 记录缓存访问轨迹分析后的核心指标
     */
    struct EvalResult
    {
        size_t misses = 0;      // 缓存未命中次数（总访问数 - 命中数）
        double missRatio = 0.0; // 缓存未命中率（misses / 总访问数，0~1之间）
    };
}