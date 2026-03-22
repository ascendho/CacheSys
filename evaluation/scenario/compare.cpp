/*
 * 缓存策略场景化测试工具 - 编译运行步骤（Release模式性能最优）：
 * 1. cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
 * 2. cmake --build build --target cache_policy_scenarios
 * 3. ./build/evaluation/cache_policy_scenarios
 */

#include <vector>

#include "evaluator.h"
#include "generators.h"
#include "report.h"

// 主函数：场景化测试工具入口，批量执行预设业务场景，对比LRU/LFU/ARC的GET命中率
int main()
{
    // 生成默认测试用例列表（包含3类典型业务场景：热点数据、循环扫描、负载漂移）
    const std::vector<CacheSys::Eval::Scenario::ScenarioCase> scenarios =
        CacheSys::Eval::Scenario::makeDefaultScenarioCases();

    // 遍历每个测试场景，执行策略评估并输出结果
    for (const auto &scenario : scenarios)
    {
        // 评估LRU策略在当前场景下的GET命中指标（总GET数、命中数）
        const CacheSys::Eval::Scenario::EvalResult lru =
            CacheSys::Eval::Scenario::evalLru(scenario.ops, scenario.capacity);
        // 评估LFU策略在当前场景下的GET命中指标
        const CacheSys::Eval::Scenario::EvalResult lfu =
            CacheSys::Eval::Scenario::evalLfu(scenario.ops, scenario.capacity);
        // 评估ARC策略在当前场景下的GET命中指标
        const CacheSys::Eval::Scenario::EvalResult arc =
            CacheSys::Eval::Scenario::evalArc(scenario.ops, scenario.capacity);

        // 格式化打印当前场景的汇总结果（标题+容量+各策略命中率）
        CacheSys::Eval::Scenario::printScenarioSummary(scenario.title, scenario.capacity, lru, lfu, arc);
    }

    return 0;
}