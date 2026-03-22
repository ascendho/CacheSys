/*
 * 编译运行步骤（Release模式，性能最优）：
 * 1. cmake -S . -B build -DCMAKE_BUILD_TYPE=Release  
 * 2. cmake --build build --target cache_trace_compare 
 * 3. ./build/evaluation/cache_trace_compare          
 * 可选参数：
 *   --trace-file=PATH    指定外部轨迹文件（每行一个key）
 *   --capacities=64,128  指定要测试的缓存容量列表
 *   -h/--help            查看帮助
 */

#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "generators.h"
#include "report.h"
#include "simulators.h"

int main(int argc, char **argv)
{
    std::string traceFilePath;                      // 外部轨迹文件路径（可选）
    std::vector<int> capacities{64, 128, 256, 512}; // 默认测试的缓存容量列表

    // 解析命令行参数
    for (int i = 1; i < argc; ++i)
    {
        const std::string arg(argv[i]);
        // 打印帮助信息并退出
        if (arg == "--help" || arg == "-h")
        {
            CacheSys::Eval::Trace::printHelp(argv[0]);
            return 0;
        }
        // 指定外部轨迹文件路径
        if (arg.rfind("--trace-file=", 0) == 0)
        {
            traceFilePath = arg.substr(std::string("--trace-file=").size());
            continue;
        }
        // 解析自定义缓存容量列表（CSV格式）
        if (arg.rfind("--capacities=", 0) == 0)
        {
            capacities = CacheSys::Eval::Trace::parseCapacities(arg.substr(std::string("--capacities=").size()));
            continue;
        }

        // 未知参数：提示错误并打印帮助
        std::cerr << "Unknown arg: " << arg << "\n";
        CacheSys::Eval::Trace::printHelp(argv[0]);
        return 1;
    }

    // 校验：缓存容量列表不能为空
    if (capacities.empty())
    {
        std::cerr << "Capacities list is empty.\n";
        return 1;
    }

    // 存储轨迹名称+轨迹数据（支持多轨迹批量测试）
    std::vector<std::pair<std::string, std::vector<int>>> traces;
    try
    {
        // 优先加载外部轨迹文件；无外部文件则生成3种内置模拟轨迹
        if (!traceFilePath.empty())
        {
            traces.push_back({"FileTrace", CacheSys::Eval::Trace::loadTraceFromFile(traceFilePath)});
        }
        else
        {
            traces.push_back({"LoopScan", CacheSys::Eval::Trace::makeLoopScanTrace(1024, 200)});              // 循环扫描轨迹
            traces.push_back({"Hotset80_20", CacheSys::Eval::Trace::makeHotsetTrace(300000, 128, 2048, 80)}); // 80%热点轨迹
            traces.push_back({"ShiftedHotset", CacheSys::Eval::Trace::makeShiftedHotsetTrace(300000, 256)});  // 热点漂移轨迹
        }
    }
    catch (const std::exception &ex)
    {
        // 捕获轨迹加载/生成异常（如文件不存在），打印错误并退出
        std::cerr << ex.what() << "\n";
        return 1;
    }

    // 打印评估结果表格表头
    CacheSys::Eval::Trace::printHeader();

    // 核心逻辑：遍历所有轨迹 + 所有容量，模拟各策略并输出对比结果
    for (const auto &item : traces)
    {
        const std::string &traceName = item.first;   // 轨迹名称（用于表格展示）
        const std::vector<int> &trace = item.second; // 访问轨迹数据

        // 遍历每个缓存容量，测试不同容量下的策略性能
        for (int capacity : capacities)
        {
            // 模拟各缓存策略的执行，获取评估结果
            const CacheSys::Eval::Trace::EvalResult opt = CacheSys::Eval::Trace::simulateOpt(trace, capacity); // 理论最优基准
            const CacheSys::Eval::Trace::EvalResult lru = CacheSys::Eval::Trace::simulateLru(trace, capacity); // LRU策略
            const CacheSys::Eval::Trace::EvalResult lfu = CacheSys::Eval::Trace::simulateLfu(trace, capacity); // LFU策略
            const CacheSys::Eval::Trace::EvalResult arc = CacheSys::Eval::Trace::simulateArc(trace, capacity); // ARC策略

            // 打印各策略评估结果（DeltaOPT为与OPT的未命中率差值，百分点）
            CacheSys::Eval::Trace::printRow(traceName, capacity, "OPT", trace.size(), opt, 0.0);
            CacheSys::Eval::Trace::printRow(traceName, capacity, "LRU", trace.size(), lru, (lru.missRatio - opt.missRatio) * 100.0);
            CacheSys::Eval::Trace::printRow(traceName, capacity, "LFU", trace.size(), lfu, (lfu.missRatio - opt.missRatio) * 100.0);
            CacheSys::Eval::Trace::printRow(traceName, capacity, "ARC", trace.size(), arc, (arc.missRatio - opt.missRatio) * 100.0);
            std::cout << "--------------------------------------------------------------------------------\n";
        }
    }

    return 0;
}