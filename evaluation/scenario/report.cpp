#include "report.h"

#include <iomanip>
#include <iostream>

namespace CacheSys::Eval::Scenario
{
    void printScenarioSummary(const std::string &title,
                              int capacity,
                              const EvalResult &lru,
                              const EvalResult &lfu,
                              const EvalResult &arc)
    {
        const double lruHitRate = lru.gets == 0 ? 0.0 : (100.0 * static_cast<double>(lru.hits) / static_cast<double>(lru.gets));
        const double lfuHitRate = lfu.gets == 0 ? 0.0 : (100.0 * static_cast<double>(lfu.hits) / static_cast<double>(lfu.gets));
        const double arcHitRate = arc.gets == 0 ? 0.0 : (100.0 * static_cast<double>(arc.hits) / static_cast<double>(arc.gets));

        std::cout << "\n=== " << title << " ===\n";
        std::cout << "缓存大小: " << capacity << "\n";

        std::cout << std::fixed << std::setprecision(2);
        std::cout << "LRU - 命中率: " << lruHitRate << "% (" << lru.hits << "/" << lru.gets << ")\n";
        std::cout << "LFU - 命中率: " << lfuHitRate << "% (" << lfu.hits << "/" << lfu.gets << ")\n";
        std::cout << "ARC - 命中率: " << arcHitRate << "% (" << arc.hits << "/" << arc.gets << ")\n";
    }
}
