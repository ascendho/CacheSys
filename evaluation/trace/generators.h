#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Eval：评估模块 → Trace：轨迹生成/解析工具（构建不同特征的访问轨迹用于测试）
namespace CacheSys::Eval::Trace
{
    // 解析CSV格式的容量字符串为整数数组（如"64,128,256" → [64,128,256]）
    // csv：逗号分隔的容量字符串（支持无空格/有空格格式）
    std::vector<int> parseCapacities(const std::string &csv);

    // 从文件加载缓存访问轨迹（文件每行一个整数key）
    // path：轨迹文件路径（如trace.txt）
    std::vector<int> loadTraceFromFile(const std::string &path);

    // 生成循环扫描型轨迹（模拟顺序遍历全量数据场景，如批量对账）
    // uniqueKeys：唯一key总数 | rounds：循环遍历轮数
    std::vector<int> makeLoopScanTrace(int uniqueKeys, int rounds);

    // 生成热点集型轨迹（模拟冷热数据分离场景，如爆款商品访问）
    // totalOps：总访问次数 | hotKeys：热点key数量 | coldKeys：冷数据key数量 | hotPercent：热点访问占比（0~100）
    std::vector<int> makeHotsetTrace(int totalOps, int hotKeys, int coldKeys, int hotPercent);

    // 生成热点漂移型轨迹（模拟热点动态变化场景，如直播带货换款）
    // totalOps：总访问次数 | hotKeys：每轮热点key数量（热点会周期性切换）
    std::vector<int> makeShiftedHotsetTrace(int totalOps, int hotKeys);
}