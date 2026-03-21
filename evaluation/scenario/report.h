#pragma once

#include <string>

#include "types.h"

namespace CacheSys::Eval::Scenario
{
    void printScenarioSummary(const std::string &title,
                              int capacity,
                              const EvalResult &lru,
                              const EvalResult &lfu,
                              const EvalResult &arc);
}
