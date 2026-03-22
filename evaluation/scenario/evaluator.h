#pragma once

#include <vector>

#include "types.h"

// Eval：评估模块 → Scenario：场景化测试子模块（策略评估核心函数声明）
namespace CacheSys::Eval::Scenario
{
    // 评估LRU缓存策略在指定操作序列下的GET命中指标
    // ops：缓存操作序列（PUT/GET组合） | capacity：缓存容量
    // 返回：EvalResult（GET总次数、GET命中次数）
    EvalResult evalLru(const std::vector<Operation> &ops, int capacity);

    // 评估LFU缓存策略在指定操作序列下的GET命中指标（参数/返回同LRU）
    EvalResult evalLfu(const std::vector<Operation> &ops, int capacity);

    // 评估ARC缓存策略在指定操作序列下的GET命中指标（参数/返回同LRU）
    EvalResult evalArc(const std::vector<Operation> &ops, int capacity);
}