#pragma once

#include <vector>

#include "types.h"

// Eval：评估模块（Evaluation）
// Trace：轨迹分析子模块（基于访问轨迹模拟缓存策略）
namespace CacheSys::Eval::Trace
{
    // 模拟LRU缓存策略的访问过程，返回核心评估指标
    // trace：缓存访问轨迹（如商品ID/用户ID的访问序列）
    // capacity：缓存最大容量（可存放的条目数）
    EvalResult simulateLru(const std::vector<int> &trace, int capacity);

    // 模拟LFU缓存策略的访问过程，返回核心评估指标
    EvalResult simulateLfu(const std::vector<int> &trace, int capacity);

    // 模拟ARC缓存策略的访问过程，返回核心评估指标
    EvalResult simulateArc(const std::vector<int> &trace, int capacity);

    // 模拟OPT（最优替换）缓存策略的访问过程，返回核心评估指标（理论最优基准）
    EvalResult simulateOpt(const std::vector<int> &trace, int capacity);
}