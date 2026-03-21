#pragma once

#include <string>
#include <vector>

#include "types.h"

namespace CacheSys::Eval::Scenario
{
    struct ScenarioCase
    {
        std::string title;
        int capacity = 0;
        std::vector<Operation> ops;
    };

    std::vector<Operation> makeHotDataScenarioOps(int operations, int hotKeys, int coldKeys);
    std::vector<Operation> makeLoopScanScenarioOps(int operations, int loopSize);
    std::vector<Operation> makeWorkloadShiftScenarioOps(int operations);
    std::vector<ScenarioCase> makeDefaultScenarioCases();
}
