#include <vector>

#include "evaluator.h"
#include "generators.h"
#include "report.h"

/**
 * @brief 缓存系统性能评估主程序
 * 
 * 该程序的主要流程：
 * 1. 初始化一组标准化的测试场景（包含热点访问、循环扫描、负载偏移等）。
 * 2. 遍历每个场景，在相同的操作序列和容量限制下，分别运行 LRU、LFU 和 ARC 算法。
 * 3. 收集各算法的命中率数据。
 * 4. 输出格式化的对比报告，用于分析不同算法在不同负载下的优劣。
 * 
 * @return int 程序退出状态码
 */
int main()
{
    // --- 1. 初始化测试场景 ---
    // 获取系统中预定义的一系列默认测试用例（ScenarioCases）
    // 每个 Case 包含了场景标题、建议的缓存容量以及预生成的指令序列 (Ops)
    const std::vector<CacheSys::Eval::Scenario::ScenarioCase> scenarios =
        CacheSys::Eval::Scenario::makeDefaultScenarioCases();

    // --- 2. 遍历并执行评估 ---
    // 对每一个预设的场景进行闭环测试
    for (const auto &scenario : scenarios)
    {
        // a. 运行 LRU (最近最少使用) 算法仿真
        // 传入相同的 ops 序列和 capacity，确保对比的公平性（控制变量法）
        const CacheSys::Eval::Scenario::EvalResult lru =
            CacheSys::Eval::Scenario::evalLru(scenario.ops, scenario.capacity);
        
        // b. 运行 LFU (最不经常使用) 算法仿真
        const CacheSys::Eval::Scenario::EvalResult lfu =
            CacheSys::Eval::Scenario::evalLfu(scenario.ops, scenario.capacity);
        
        // c. 运行 ARC (自适应替换缓存) 算法仿真
        const CacheSys::Eval::Scenario::EvalResult arc =
            CacheSys::Eval::Scenario::evalArc(scenario.ops, scenario.capacity);

        // --- 3. 输出对比报告 ---
        // 将三个算法的评估结果 (EvalResult) 汇总并打印成表格或列表形式
        CacheSys::Eval::Scenario::printScenarioSummary(
            scenario.title, 
            scenario.capacity, 
            lru, 
            lfu, 
            arc
        );
    }

    return 0;
}