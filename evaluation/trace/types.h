#pragma once

#include <cstddef>



namespace CacheSys::Eval::Trace
{
    
        // Eval：评估模块（Evaluation）
        // Trace：轨迹分析子模块（基于访问轨迹评估缓存策略）



    struct EvalResult
    {
        size_t misses = 0;      
        double missRatio = 0.0; 
    };
            size_t misses = 0;      // 缓存未命中次数
            double missRatio = 0.0; // 缓存未命中率（misses / 总访问次数）