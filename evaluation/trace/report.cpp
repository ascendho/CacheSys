#include "report.h"

#include <iomanip>
#include <iostream>

// Eval：评估模块 → Trace：轨迹分析报告输出实现
namespace CacheSys::Eval::Trace
{
    // 打印程序帮助信息
    // prog：可执行程序名（argv[0]）
    void printHelp(const char *prog)
    {
        std::cout << "Usage: " << prog << " [--trace-file=PATH] [--capacities=64,128,256]\n";
        std::cout << "Without --trace-file, built-in synthetic traces are used.\n";
    }

    // 打印对比结果表头
    void printHeader()
    {
        std::cout << "\nTrace-Driven Cache Policy Comparison (Miss Ratio)\n";
        std::cout << "--------------------------------------------------------------------------------\n";
        // 列定义：轨迹名 | 容量 | 策略 | 访问数 | 未命中数 | 未命中率 | 与OPT差值
        std::cout << std::left << std::setw(18) << "Trace"
                  << std::setw(10) << "Capacity"
                  << std::setw(8) << "Policy"
                  << std::setw(12) << "Accesses"
                  << std::setw(12) << "Misses"
                  << std::setw(14) << "MissRatio(%)"
                  << std::setw(14) << "DeltaOPT(pp)" << "\n";
        std::cout << "--------------------------------------------------------------------------------\n";
    }

    // 打印单行结果
    // deltaOpt 单位为百分点（pp）
    void printRow(const std::string &traceName,
                  int capacity,
                  const std::string &policy,
                  size_t accesses,
                  const EvalResult &result,
                  double deltaOpt)
    {
        // 与表头同列宽，未命中率与差值保留3位小数
        std::cout << std::left << std::setw(18) << traceName
                  << std::setw(10) << capacity
                  << std::setw(8) << policy
                  << std::setw(12) << accesses
                  << std::setw(12) << result.misses
                  << std::setw(14) << std::fixed << std::setprecision(3) << (result.missRatio * 100.0)
                  << std::setw(14) << std::fixed << std::setprecision(3) << deltaOpt << "\n";
    }
}