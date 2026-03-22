#pragma once

#include <string>

#include "types.h"

// Eval：评估模块 → Scenario：场景化测试子模块
namespace CacheSys::Eval::Scenario
{
    // 打印场景化测试的汇总结果（对比LRU/LFU/ARC的GET命中指标）
    // title：场景名称（如"热点商品访问"）
    // capacity：测试用的缓存容量
    // lru/lfu/arc：对应策略的评估结果（GET总次数+命中次数）
    void printScenarioSummary(const std::string &title,
                              int capacity,
                              const EvalResult &lru,
                              const EvalResult &lfu,
                              const EvalResult &arc);
}