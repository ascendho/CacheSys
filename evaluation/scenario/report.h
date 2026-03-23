#pragma once

#include <string>

#include "types.h"

/**
 * @brief 缓存评估与场景模拟命名空间
 */
namespace CacheSys::Eval::Scenario
{
    /**
     * @brief 打印不同缓存策略在特定场景下的对比汇总报告
     * 
     * 该函数接收多个策略的评估结果，并在控制台输出一个格式化的对比表格。
     * 输出内容通常包括：场景名称、缓存容量、各策略的命中次数、总请求数以及命中率。
     * 
     * @param title 场景的描述性标题（例如："Zipfian 分布测试"、"大数据量扫描测试"等）
     * @param capacity 运行该测试场景时统一设置的缓存最大容量
     * @param lru LRU (最近最少使用) 算法的运行结果数据
     * @param lfu LFU (最不经常使用) 算法的运行结果数据
     * @param arc ARC (自适应替换缓存) 算法的运行结果数据
     * 
     * @note 该函数主要用于性能分析阶段，帮助开发者根据可视化数据选择最适合当前业务的缓存算法。
     */
    void printScenarioSummary(const std::string &title,
                              int capacity,
                              const EvalResult &lru,
                              const EvalResult &lfu,
                              const EvalResult &arc);
}