#include "report.h"

#include <iomanip>
#include <iostream>

/**
 * @brief 缓存评估与场景模拟命名空间
 */
namespace CacheSys::Eval::Scenario
{
    /**
     * @brief 实现：打印不同缓存策略在特定场景下的对比汇总报告
     * 
     * 该函数负责将评估产生的原始计数数据（hits/gets）转换为直观的百分比，
     * 并按照固定的格式输出到标准控制台。
     */
    void printScenarioSummary(const std::string &title,
                              int capacity,
                              const EvalResult &lru,
                              const EvalResult &lfu,
                              const EvalResult &arc)
    {
        // --- 1. 计算命中率百分比 (0.0% - 100.0%) ---
        // 逻辑：如果总请求数为 0，则命中率设为 0.0，防止除以零错误。
        
        // 计算 LRU 的百分比命中率
        const double lruHitRate = lru.gets == 0 ? 0.0 : (100.0 * static_cast<double>(lru.hits) / static_cast<double>(lru.gets));
        
        // 计算 LFU 的百分比命中率
        const double lfuHitRate = lfu.gets == 0 ? 0.0 : (100.0 * static_cast<double>(lfu.hits) / static_cast<double>(lfu.gets));
        
        // 计算 ARC 的百分比命中率
        const double arcHitRate = arc.gets == 0 ? 0.0 : (100.0 * static_cast<double>(arc.hits) / static_cast<double>(arc.gets));

        // --- 2. 打印页眉信息 ---
        std::cout << "\n=== " << title << " ===\n";
        std::cout << "缓存大小: " << capacity << "\n";

        // --- 3. 打印对比数据表 ---
        // 设置输出格式：std::fixed 结合 std::setprecision(2) 确保显示两位小数 (如 85.50%)
        std::cout << std::fixed << std::setprecision(2);
        
        // 打印 LRU 统计结果
        std::cout << "LRU - 命中率: " << lruHitRate << "% (" << lru.hits << "/" << lru.gets << ")\n";
        
        // 打印 LFU 统计结果
        std::cout << "LFU - 命中率: " << lfuHitRate << "% (" << lfu.hits << "/" << lfu.gets << ")\n";
        
        // 打印 ARC 统计结果
        std::cout << "ARC - 命中率: " << arcHitRate << "% (" << arc.hits << "/" << arc.gets << ")\n";
    }
}