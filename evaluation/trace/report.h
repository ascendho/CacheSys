#pragma once

#include <cstddef>
#include <string>

#include "types.h"

// Eval：评估模块 → Trace：轨迹分析子模块（格式化输出评估结果）
namespace CacheSys::Eval::Trace
{
    // 打印程序使用帮助信息（参数说明、运行方式）
    // prog：程序名称（argv[0]）
    void printHelp(const char *prog);

    // 打印评估结果表格的表头
    void printHeader();

    // 打印评估结果单行数据（表格行），展示单组策略的评估指标
    // traceName：轨迹名称（如"商品详情访问轨迹"）
    // capacity：缓存容量
    // policy：缓存策略名称（如"LRU"/"LFU"/"OPT"）
    // accesses：总访问次数
    // result：核心评估结果（未命中数、未命中率）
    // deltaOpt：与OPT策略的未命中率差值（衡量策略接近理论最优的程度）
    void printRow(const std::string &traceName,
                  int capacity,
                  const std::string &policy,
                  size_t accesses,
                  const EvalResult &result,
                  double deltaOpt);
}