#include "report.h"

#include <iomanip>
#include <iostream>

// Eval：评估模块 → Trace：轨迹分析子模块
namespace CacheSys::Eval::Trace
{
    // 打印程序使用帮助信息，提示命令行参数用法
    // prog：程序可执行文件名称（argv[0]）
    void printHelp(const char *prog)
    {
        std::cout << "Usage: " << prog << " [--trace-file=PATH] [--capacities=64,128,256]\n";
        std::cout << "Without --trace-file, built-in synthetic traces are used.\n";
    }

    // 打印缓存策略评估结果表格的表头，统一列格式便于多策略对比
    void printHeader()
    {
        std::cout << "\nTrace-Driven Cache Policy Comparison (Miss Ratio)\n";
        std::cout << "--------------------------------------------------------------------------------\n";
        // 表头列：轨迹名 | 缓存容量 | 策略 | 总访问数 | 未命中数 | 未命中率(%) | 与OPT差值(百分点)
        std::cout << std::left << std::setw(18) << "Trace"
                  << std::setw(10) << "Capacity"
                  << std::setw(8) << "Policy"
                  << std::setw(12) << "Accesses"
                  << std::setw(12) << "Misses"
                  << std::setw(14) << "MissRatio(%)"
                  << std::setw(14) << "DeltaOPT(pp)" << "\n";
        std::cout << "--------------------------------------------------------------------------------\n";
    }

    // 打印单组缓存策略的评估结果行（与表头列一一对应）
    // traceName：轨迹名称 | capacity：缓存容量 | policy：策略名称（LRU/LFU等）
    // accesses：总访问次数 | result：核心评估结果 | deltaOpt：与OPT的未命中率差值（百分点）
    void printRow(const std::string &traceName,
                  int capacity,
                  const std::string &policy,
                  size_t accesses,
                  const EvalResult &result,
                  double deltaOpt)
    {
        // 格式化输出单行数据，列宽与表头一致，未命中率/差值保留3位小数
        std::cout << std::left << std::setw(18) << traceName
                  << std::setw(10) << capacity
                  << std::setw(8) << policy
                  << std::setw(12) << accesses
                  << std::setw(12) << result.misses
                  << std::setw(14) << std::fixed << std::setprecision(3) << (result.missRatio * 100.0)
                  << std::setw(14) << std::fixed << std::setprecision(3) << deltaOpt << "\n";
    }
}