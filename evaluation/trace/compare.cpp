/*
 * 1. cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
 * 2. cmake --build build --target cache_trace_compare
 * 3. ./build/evaluation/cache_trace_compare
 *
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
    std::string traceFilePath;
    std::vector<int> capacities{64, 128, 256, 512};

    for (int i = 1; i < argc; ++i)
    {
        const std::string arg(argv[i]);
        if (arg == "--help" || arg == "-h")
        {
            CacheSys::Eval::Trace::printHelp(argv[0]);
            return 0;
        }
        if (arg.rfind("--trace-file=", 0) == 0)
        {
            traceFilePath = arg.substr(std::string("--trace-file=").size());
            continue;
        }
        if (arg.rfind("--capacities=", 0) == 0)
        {
            capacities = CacheSys::Eval::Trace::parseCapacities(arg.substr(std::string("--capacities=").size()));
            continue;
        }

        std::cerr << "Unknown arg: " << arg << "\n";
        CacheSys::Eval::Trace::printHelp(argv[0]);
        return 1;
    }

    if (capacities.empty())
    {
        std::cerr << "Capacities list is empty.\n";
        return 1;
    }

    std::vector<std::pair<std::string, std::vector<int>>> traces;
    try
    {
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
        std::cerr << ex.what() << "\n";
        return 1;
    }

    CacheSys::Eval::Trace::printHeader();

    for (const auto &item : traces)
    {
        const std::string &traceName = item.first;
        const std::vector<int> &trace = item.second;

        for (int capacity : capacities)
        {
            const CacheSys::Eval::Trace::EvalResult opt = CacheSys::Eval::Trace::simulateOpt(trace, capacity);
            const CacheSys::Eval::Trace::EvalResult lru = CacheSys::Eval::Trace::simulateLru(trace, capacity);
            const CacheSys::Eval::Trace::EvalResult lfu = CacheSys::Eval::Trace::simulateLfu(trace, capacity);
            const CacheSys::Eval::Trace::EvalResult arc = CacheSys::Eval::Trace::simulateArc(trace, capacity);

            CacheSys::Eval::Trace::printRow(traceName, capacity, "OPT", trace.size(), opt, 0.0);
            CacheSys::Eval::Trace::printRow(traceName, capacity, "LRU", trace.size(), lru, (lru.missRatio - opt.missRatio) * 100.0);
            CacheSys::Eval::Trace::printRow(traceName, capacity, "LFU", trace.size(), lfu, (lfu.missRatio - opt.missRatio) * 100.0);
            CacheSys::Eval::Trace::printRow(traceName, capacity, "ARC", trace.size(), arc, (arc.missRatio - opt.missRatio) * 100.0);
            std::cout << "--------------------------------------------------------------------------------\n";
        }
    }

    return 0;
}
