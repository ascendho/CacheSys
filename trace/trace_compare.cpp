/*
 * cd /Users/ascendho/Downloads/project/CacheSys
 * cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
 * cmake --build build --target cache_trace_compare
 * ./build/trace/cache_trace_compare
 *
 */

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "LfuCache.h"
#include "LruCache.h"

namespace
{
    struct EvalResult
    {
        size_t misses = 0;
        double missRatio = 0.0;
    };

    struct FastRng
    {
        explicit FastRng(uint64_t seed) : state(seed) {}

        uint64_t next()
        {
            state = state * 6364136223846793005ULL + 1ULL;
            return state;
        }

        uint64_t state;
    };

    std::vector<int> parseCapacities(const std::string &csv)
    {
        std::vector<int> capacities;
        std::stringstream ss(csv);
        std::string item;
        while (std::getline(ss, item, ','))
        {
            if (!item.empty())
            {
                capacities.push_back(std::stoi(item));
            }
        }
        return capacities;
    }

    std::vector<int> loadTraceFromFile(const std::string &path)
    {
        std::ifstream ifs(path);
        if (!ifs)
        {
            throw std::runtime_error("Failed to open trace file: " + path);
        }

        std::vector<int> trace;
        int key = 0;
        while (ifs >> key)
        {
            trace.push_back(key);
        }
        return trace;
    }

    std::vector<int> makeLoopScanTrace(int uniqueKeys, int rounds)
    {
        std::vector<int> trace;
        trace.reserve(static_cast<size_t>(uniqueKeys * rounds));
        for (int r = 0; r < rounds; ++r)
        {
            for (int k = 0; k < uniqueKeys; ++k)
            {
                trace.push_back(k);
            }
        }
        return trace;
    }

    std::vector<int> makeHotsetTrace(int totalOps, int hotKeys, int coldKeys, int hotPercent)
    {
        std::vector<int> trace;
        trace.reserve(static_cast<size_t>(totalOps));
        FastRng rng(0x1029384756ULL);
        for (int i = 0; i < totalOps; ++i)
        {
            const bool fromHotset = static_cast<int>(rng.next() % 100ULL) < hotPercent;
            if (fromHotset)
            {
                trace.push_back(static_cast<int>(rng.next() % static_cast<uint64_t>(hotKeys)));
            }
            else
            {
                trace.push_back(hotKeys + static_cast<int>(rng.next() % static_cast<uint64_t>(coldKeys)));
            }
        }
        return trace;
    }

    std::vector<int> makeShiftedHotsetTrace(int totalOps, int hotKeys)
    {
        std::vector<int> trace;
        trace.reserve(static_cast<size_t>(totalOps));
        FastRng rng(0x55667788ULL);
        const int half = totalOps / 2;

        for (int i = 0; i < half; ++i)
        {
            trace.push_back(static_cast<int>(rng.next() % static_cast<uint64_t>(hotKeys)));
        }
        for (int i = half; i < totalOps; ++i)
        {
            trace.push_back(hotKeys + static_cast<int>(rng.next() % static_cast<uint64_t>(hotKeys)));
        }
        return trace;
    }

    EvalResult simulateLru(const std::vector<int> &trace, int capacity)
    {
        if (capacity <= 0)
        {
            return EvalResult{trace.size(), trace.empty() ? 0.0 : 1.0};
        }

        CacheSys::LruCache<int, int> cache(capacity);
        size_t misses = 0;
        for (int key : trace)
        {
            int value = 0;
            if (!cache.get(key, value))
            {
                ++misses;
                cache.put(key, key);
            }
        }

        EvalResult result;
        result.misses = misses;
        result.missRatio = trace.empty() ? 0.0 : static_cast<double>(misses) / static_cast<double>(trace.size());
        return result;
    }

    EvalResult simulateLfu(const std::vector<int> &trace, int capacity)
    {
        if (capacity <= 0)
        {
            return EvalResult{trace.size(), trace.empty() ? 0.0 : 1.0};
        }

        CacheSys::LfuCache<int, int> cache(capacity);
        size_t misses = 0;
        for (int key : trace)
        {
            int value = 0;
            if (!cache.get(key, value))
            {
                ++misses;
                cache.put(key, key);
            }
        }

        EvalResult result;
        result.misses = misses;
        result.missRatio = trace.empty() ? 0.0 : static_cast<double>(misses) / static_cast<double>(trace.size());
        return result;
    }

    EvalResult simulateOpt(const std::vector<int> &trace, int capacity)
    {
        if (capacity <= 0)
        {
            return EvalResult{trace.size(), trace.empty() ? 0.0 : 1.0};
        }

        std::unordered_map<int, std::vector<size_t>> positions;
        positions.reserve(trace.size());
        for (size_t i = 0; i < trace.size(); ++i)
        {
            positions[trace[i]].push_back(i);
        }

        std::unordered_map<int, size_t> consumed;
        consumed.reserve(positions.size());

        std::unordered_set<int> cache;
        cache.reserve(static_cast<size_t>(capacity * 2));

        size_t misses = 0;
        const size_t inf = std::numeric_limits<size_t>::max();

        auto nextUse = [&](int key) -> size_t
        {
            const auto pIt = positions.find(key);
            if (pIt == positions.end())
            {
                return inf;
            }
            const auto cIt = consumed.find(key);
            const size_t used = (cIt == consumed.end()) ? 0 : cIt->second;
            if (used >= pIt->second.size())
            {
                return inf;
            }
            return pIt->second[used];
        };

        for (int key : trace)
        {
            consumed[key]++;

            if (cache.find(key) != cache.end())
            {
                continue;
            }

            ++misses;

            if (cache.size() < static_cast<size_t>(capacity))
            {
                cache.insert(key);
                continue;
            }

            int victim = *cache.begin();
            size_t farthest = 0;
            bool victimChosen = false;

            for (int candidate : cache)
            {
                const size_t nu = nextUse(candidate);
                if (!victimChosen || nu > farthest)
                {
                    victim = candidate;
                    farthest = nu;
                    victimChosen = true;
                }
            }

            cache.erase(victim);
            cache.insert(key);
        }

        EvalResult result;
        result.misses = misses;
        result.missRatio = trace.empty() ? 0.0 : static_cast<double>(misses) / static_cast<double>(trace.size());
        return result;
    }

    void printHelp(const char *prog)
    {
        std::cout << "Usage: " << prog << " [--trace-file=PATH] [--capacities=64,128,256]\n";
        std::cout << "Without --trace-file, built-in synthetic traces are used.\n";
    }

    void printHeader()
    {
        std::cout << "\nTrace-Driven Cache Policy Comparison (Miss Ratio)\n";
        std::cout << "--------------------------------------------------------------------------------\n";
        std::cout << std::left << std::setw(18) << "Trace"
                  << std::setw(10) << "Capacity"
                  << std::setw(8) << "Policy"
                  << std::setw(12) << "Accesses"
                  << std::setw(12) << "Misses"
                  << std::setw(14) << "MissRatio(%)"
                  << std::setw(14) << "DeltaOPT(pp)" << "\n";
        std::cout << "--------------------------------------------------------------------------------\n";
    }

    void printRow(const std::string &traceName,
                  int capacity,
                  const std::string &policy,
                  size_t accesses,
                  const EvalResult &result,
                  double deltaOpt)
    {
        std::cout << std::left << std::setw(18) << traceName
                  << std::setw(10) << capacity
                  << std::setw(8) << policy
                  << std::setw(12) << accesses
                  << std::setw(12) << result.misses
                  << std::setw(14) << std::fixed << std::setprecision(3) << (result.missRatio * 100.0)
                  << std::setw(14) << std::fixed << std::setprecision(3) << deltaOpt << "\n";
    }

} // namespace

int main(int argc, char **argv)
{
    std::string traceFilePath;
    std::vector<int> capacities{64, 128, 256, 512};

    for (int i = 1; i < argc; ++i)
    {
        const std::string arg(argv[i]);
        if (arg == "--help" || arg == "-h")
        {
            printHelp(argv[0]);
            return 0;
        }
        if (arg.rfind("--trace-file=", 0) == 0)
        {
            traceFilePath = arg.substr(std::string("--trace-file=").size());
            continue;
        }
        if (arg.rfind("--capacities=", 0) == 0)
        {
            capacities = parseCapacities(arg.substr(std::string("--capacities=").size()));
            continue;
        }

        std::cerr << "Unknown arg: " << arg << "\n";
        printHelp(argv[0]);
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
            traces.push_back({"FileTrace", loadTraceFromFile(traceFilePath)});
        }
        else
        {
            traces.push_back({"LoopScan", makeLoopScanTrace(1024, 200)});
            traces.push_back({"Hotset80_20", makeHotsetTrace(300000, 128, 2048, 80)});
            traces.push_back({"ShiftedHotset", makeShiftedHotsetTrace(300000, 256)});
        }
    }
    catch (const std::exception &ex)
    {
        std::cerr << ex.what() << "\n";
        return 1;
    }

    printHeader();

    for (const auto &item : traces)
    {
        const std::string &traceName = item.first;
        const std::vector<int> &trace = item.second;

        for (int capacity : capacities)
        {
            const EvalResult opt = simulateOpt(trace, capacity);
            const EvalResult lru = simulateLru(trace, capacity);
            const EvalResult lfu = simulateLfu(trace, capacity);

            printRow(traceName, capacity, "OPT", trace.size(), opt, 0.0);
            printRow(traceName, capacity, "LRU", trace.size(), lru, (lru.missRatio - opt.missRatio) * 100.0);
            printRow(traceName, capacity, "LFU", trace.size(), lfu, (lfu.missRatio - opt.missRatio) * 100.0);
            std::cout << "--------------------------------------------------------------------------------\n";
        }
    }

    return 0;
}
