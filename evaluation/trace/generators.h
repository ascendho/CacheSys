#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace CacheSys::Eval::Trace
{
    std::vector<int> parseCapacities(const std::string &csv);
    std::vector<int> loadTraceFromFile(const std::string &path);
    std::vector<int> makeLoopScanTrace(int uniqueKeys, int rounds);
    std::vector<int> makeHotsetTrace(int totalOps, int hotKeys, int coldKeys, int hotPercent);
    std::vector<int> makeShiftedHotsetTrace(int totalOps, int hotKeys);
}
