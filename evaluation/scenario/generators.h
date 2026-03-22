#pragma once

#include <string>
#include <vector>

#include "types.h"

// Eval：评估模块 → Scenario：场景化测试子模块（测试用例/操作序列生成）
namespace CacheSys::Eval::Scenario
{
    /**
     * 场景化测试用例结构体
     * 封装单个测试场景的标题、缓存容量、操作序列
     */
    struct ScenarioCase
    {
        std::string title;          // 场景标题（如"热点商品访问"）
        int capacity = 0;           // 该场景测试的缓存容量
        std::vector<Operation> ops; // 场景对应的缓存操作序列（PUT/GET组合）
    };

    // 生成热点数据场景操作序列（模拟冷热数据分离，如爆款商品访问）
    // operations：总操作次数 | hotKeys：热点key数量 | coldKeys：冷数据key数量
    std::vector<Operation> makeHotDataScenarioOps(int operations, int hotKeys, int coldKeys);

    // 生成循环扫描场景操作序列（模拟顺序遍历全量数据，如批量对账/报表生成）
    // operations：总操作次数 | loopSize：单次循环的key数量
    std::vector<Operation> makeLoopScanScenarioOps(int operations, int loopSize);

    // 生成负载漂移场景操作序列（模拟热点动态切换，如直播带货换款/活动流量迁移）
    // operations：总操作次数
    std::vector<Operation> makeWorkloadShiftScenarioOps(int operations);

    // 生成默认的场景化测试用例列表（包含典型业务场景+对应容量+操作序列）
    std::vector<ScenarioCase> makeDefaultScenarioCases();
}