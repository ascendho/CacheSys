#pragma once

#include <cstddef>
#include <string>

#include "types.h"

// Eval：评估模块 → Trace：轨迹分析结果输出子模块
namespace CacheSys::Eval::Trace
{
    // 打印程序使用帮助信息（参数说明、运行方式）
    // prog：程序名（argv[0]）
    void printHelp(const char *prog);

    // 打印评估结果表格表头
    void printHeader();

    // 打印评估结果单行数据（表格行）
    // traceName：轨迹名称
    // capacity：缓存容量
    // policy：策略名称（OPT/LRU/LFU/ARC）
    // accesses：总访问次数
    // result：评估结果（未命中数、未命中率）
    // deltaOpt：与OPT策略未命中率的差值（百分点）
    void printRow(const std::string &traceName,
                  int capacity,
                  const std::string &policy,
                  size_t accesses,
                  const EvalResult &result,
                  double deltaOpt);
}