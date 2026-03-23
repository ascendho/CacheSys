#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "generators.h"
#include "report.h"
#include "simulators.h"

// 主程序入口：支持加载外部轨迹或使用内置轨迹，按容量批量评估 OPT/LRU/LFU/ARC
int main(int argc, char **argv)
{
    std::string traceFilePath;                       // 外部轨迹文件路径（可选）
    std::vector<int> capacities{64, 128, 256, 512}; // 默认测试容量集合

    // 解析命令行参数
    for (int i = 1; i < argc; ++i)
    {
        const std::string arg(argv[i]);

        // 帮助参数
        if (arg == "--help" || arg == "-h")
        {
            CacheSys::Eval::Trace::printHelp(argv[0]);
            return 0;
        }

        // 指定外部轨迹文件
        if (arg.rfind("--trace-file=", 0) == 0)
        {
            traceFilePath = arg.substr(std::string("--trace-file=").size());
            continue;
        }

        // 指定容量列表（CSV）
        if (arg.rfind("--capacities=", 0) == 0)
        {
            capacities = CacheSys::Eval::Trace::parseCapacities(arg.substr(std::string("--capacities=").size()));
            continue;
        }

        // 未知参数：报错并提示帮助
        std::cerr << "Unknown arg: " << arg << "\n";
        CacheSys::Eval::Trace::printHelp(argv[0]);
        return 1;
    }

    // 基本校验：容量列表不能为空
    if (capacities.empty())
    {
        std::cerr << "Capacities list is empty.\n";
        return 1;
    }

    // traces: (轨迹名, 轨迹数据)
    std::vector<std::pair<std::string, std::vector<int>>> traces;
    try
    {
        // 优先使用外部轨迹；否则使用内置轨迹样本
        if (!traceFilePath.empty())
        {
            traces.push_back({"FileTrace", CacheSys::Eval::Trace::loadTraceFromFile(traceFilePath)});
        }
        else
        {
            traces.push_back({"LoopScan", CacheSys::Eval::Trace::makeLoopScanTrace(1024, 200)});
            traces.push_back({"Hotset80_20", CacheSys::Eval::Trace::makeHotsetTrace(300000, 128, 2048, 80)});
            traces.push_back({"ShiftedHotset", CacheSys::Eval::Trace::makeShiftedHotsetTrace(300000, 256)});
        }
    }
    catch (const std::exception &ex)
    {
        // 轨迹加载/生成异常
        std::cerr << ex.what() << "\n";
        return 1;
    }

    // 输出表头
    CacheSys::Eval::Trace::printHeader();

    // 双层循环：遍历轨迹与容量，逐项输出策略对比结果
    for (const auto &item : traces)
    {
        const std::string &traceName = item.first;
        const std::vector<int> &trace = item.second;

        // 同一条轨迹在不同容量下进行评估
        for (int capacity : capacities)
        {
            // 依次模拟理论最优和各缓存策略
            const CacheSys::Eval::Trace::EvalResult opt = CacheSys::Eval::Trace::simulateOpt(trace, capacity);
            const CacheSys::Eval::Trace::EvalResult lru = CacheSys::Eval::Trace::simulateLru(trace, capacity);
            const CacheSys::Eval::Trace::EvalResult lfu = CacheSys::Eval::Trace::simulateLfu(trace, capacity);
            const CacheSys::Eval::Trace::EvalResult arc = CacheSys::Eval::Trace::simulateArc(trace, capacity);

            // DeltaOPT = 当前策略未命中率 - OPT未命中率（单位：百分点）
            CacheSys::Eval::Trace::printRow(traceName, capacity, "OPT", trace.size(), opt, 0.0);
            CacheSys::Eval::Trace::printRow(traceName, capacity, "LRU", trace.size(), lru, (lru.missRatio - opt.missRatio) * 100.0);
            CacheSys::Eval::Trace::printRow(traceName, capacity, "LFU", trace.size(), lfu, (lfu.missRatio - opt.missRatio) * 100.0);
            CacheSys::Eval::Trace::printRow(traceName, capacity, "ARC", trace.size(), arc, (arc.missRatio - opt.missRatio) * 100.0);
            std::cout << "--------------------------------------------------------------------------------\n";
        }
    }

    return 0;
}