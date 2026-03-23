#pragma once

#include <vector>

#include "types.h"

// Eval：评估模块（Evaluation）
// Trace：轨迹分析子模块（基于访问轨迹模拟缓存策略）
namespace CacheSys::Eval::Trace
{
    // 模拟LRU缓存策略访问过程，返回未命中指标
    // trace：访问轨迹 | capacity：缓存容量
    EvalResult simulateLru(const std::vector<int> &trace, int capacity);

    // 模拟LFU缓存策略访问过程，返回未命中指标
    EvalResult simulateLfu(const std::vector<int> &trace, int capacity);

    // 模拟ARC缓存策略访问过程，返回未命中指标
    EvalResult simulateArc(const std::vector<int> &trace, int capacity);

    // 模拟OPT（理论最优替换）策略访问过程，返回未命中指标
    EvalResult simulateOpt(const std::vector<int> &trace, int capacity);
}