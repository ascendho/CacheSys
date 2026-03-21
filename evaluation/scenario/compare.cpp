/*
 * 1. cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
 * 2. cmake --build build --target cache_policy_scenarios
 * 3. ./build/evaluation/cache_policy_scenarios
 *
 */

#include <vector>

#include "evaluator.h"
#include "generators.h"
#include "report.h"

int main()
{
    const std::vector<CacheSys::Eval::Scenario::ScenarioCase> scenarios =
        CacheSys::Eval::Scenario::makeDefaultScenarioCases();

    for (const auto &scenario : scenarios)
    {
        const CacheSys::Eval::Scenario::EvalResult lru =
            CacheSys::Eval::Scenario::evalLru(scenario.ops, scenario.capacity);
        const CacheSys::Eval::Scenario::EvalResult lfu =
            CacheSys::Eval::Scenario::evalLfu(scenario.ops, scenario.capacity);
        const CacheSys::Eval::Scenario::EvalResult arc =
            CacheSys::Eval::Scenario::evalArc(scenario.ops, scenario.capacity);

        CacheSys::Eval::Scenario::printScenarioSummary(scenario.title, scenario.capacity, lru, lfu, arc);
    }

    return 0;
}
