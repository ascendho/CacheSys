#include "report.h"

#include <iomanip>
#include <iostream>

// Eval：评估模块 → Scenario：场景化测试子模块
namespace CacheSys::Eval::Scenario
{
    // 打印场景化测试汇总结果：对比LRU/LFU/ARC的GET命中率（保留2位小数）
    // title：场景名称 | capacity：缓存容量 | lru/lfu/arc：对应策略的GET命中统计
    void printScenarioSummary(const std::string &title,
                              int capacity,
                              const EvalResult &lru,
                              const EvalResult &lfu,
                              const EvalResult &arc)
    {
        // 计算各策略命中率（避免除零：GET总次数为0时命中率为0）
        const double lruHitRate = lru.gets == 0 ? 0.0 : (100.0 * static_cast<double>(lru.hits) / static_cast<double>(lru.gets));
        const double lfuHitRate = lfu.gets == 0 ? 0.0 : (100.0 * static_cast<double>(lfu.hits) / static_cast<double>(lfu.gets));
        const double arcHitRate = arc.gets == 0 ? 0.0 : (100.0 * static_cast<double>(arc.hits) / static_cast<double>(arc.gets));

        // 打印场景标题和缓存容量
        std::cout << "\n=== " << title << " ===\n";
        std::cout << "缓存大小: " << capacity << "\n";

        // 格式化输出：命中率保留2位小数，附带「命中数/总GET数」便于核对
        std::cout << std::fixed << std::setprecision(2);
        std::cout << "LRU - 命中率: " << lruHitRate << "% (" << lru.hits << "/" << lru.gets << ")\n";
        std::cout << "LFU - 命中率: " << lfuHitRate << "% (" << lfu.hits << "/" << lfu.gets << ")\n";
        std::cout << "ARC - 命中率: " << arcHitRate << "% (" << arc.hits << "/" << arc.gets << ")\n";
    }
}