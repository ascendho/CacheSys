#pragma once

#include <string>
#include <vector>

#include "types.h"

/**
 * @brief 缓存评估场景模拟命名空间
 */
namespace CacheSys::Eval::Scenario
{
    /**
     * @struct ScenarioCase
     * @brief 定义一个完整的测试用例
     * 
     * 包含场景的元数据以及预先生成的待回放操作序列。
     */
    struct ScenarioCase
    {
        std::string title;          ///< 场景标题（如 "热点数据测试"）
        int capacity = 0;           ///< 运行该场景时推荐的缓存容量
        std::vector<Operation> ops; ///< 预生成的缓存操作序列（Put/Get 组合）
    };

    /**
     * @brief 构造“热点数据”场景的操作序列
     * 
     * 模拟典型的 80/20 法则或 Zipf 分布。
     * 少量 HotKeys 会被高频访问，大量 ColdKeys 会被偶尔访问。
     * 该场景主要考察算法识别并保留高频热点的能力（LFU 和 ARC 通常表现优秀）。
     * 
     * @param operations 总操作次数
     * @param hotKeys 热点键的数量
     * @param coldKeys 冷数据键的数量
     */
    std::vector<Operation> makeHotDataScenarioOps(int operations, int hotKeys, int coldKeys);

    /**
     * @brief 构造“循环扫描”场景的操作序列
     * 
     * 模拟线性扫描一个巨大的数据集（如数据库全表扫描）。
     * 访问顺序为：1, 2, 3, ..., N, 1, 2, 3...
     * 如果 loopSize > capacity，传统的 LRU 会产生 0% 的命中率（缓存污染）。
     * 
     * @param operations 总操作次数
     * @param loopSize 循环的数据集大小
     */
    std::vector<Operation> makeLoopScanScenarioOps(int operations, int loopSize);

    /**
     * @brief 构造“负载偏移”场景的操作序列
     * 
     * 模拟实际应用中访问模式的突然改变。
     * 例如：前 50% 的时间访问 A 区域热点，后 50% 的时间突然切换到访问 B 区域热点。
     * 该场景专门用于测试算法的自适应能力（ARC 的强项）。
     * 
     * @param operations 总操作次数
     */
    std::vector<Operation> makeWorkloadShiftScenarioOps(int operations);

    /**
     * @brief 获取一套预设的默认测试用例
     * 
     * 组合上述各种工厂函数，生成一组标准化的对比测试集，
     * 方便一键运行 LRU、LFU 和 ARC 的横向评测。
     * 
     * @return std::vector<ScenarioCase> 包含多个不同难度和模式的测试场景
     */
    std::vector<ScenarioCase> makeDefaultScenarioCases();
}