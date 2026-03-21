#include "generators.h"

#include <random>

namespace CacheSys::Eval::Scenario
{
    std::vector<Operation> makeHotDataScenarioOps(int operations, int hotKeys, int coldKeys)
    {
        std::vector<Operation> ops;
        ops.reserve(static_cast<size_t>(operations));

        std::mt19937 rng(20260316);
        std::uniform_int_distribution<int> p01to100(1, 100);
        std::uniform_int_distribution<int> hotDist(0, hotKeys - 1);
        std::uniform_int_distribution<int> coldDist(0, coldKeys - 1);

        for (int i = 0; i < operations; ++i)
        {
            const bool isPut = p01to100(rng) <= 30;
            const bool hitHot = p01to100(rng) <= 70;
            const int key = hitHot ? hotDist(rng) : (hotKeys + coldDist(rng));

            ops.push_back(Operation{isPut, key, key * 10 + (i % 97)});
        }

        return ops;
    }

    std::vector<Operation> makeLoopScanScenarioOps(int operations, int loopSize)
    {
        std::vector<Operation> ops;
        ops.reserve(static_cast<size_t>(operations));

        std::mt19937 rng(20260317);
        std::uniform_int_distribution<int> p01to100(1, 100);
        std::uniform_int_distribution<int> inRange(0, loopSize - 1);
        std::uniform_int_distribution<int> outRange(0, loopSize - 1);

        int cursor = 0;
        for (int i = 0; i < operations; ++i)
        {
            const bool isPut = p01to100(rng) <= 20;
            int key = 0;

            const int selector = p01to100(rng);
            if (selector <= 60)
            {
                key = cursor;
                cursor = (cursor + 1) % loopSize;
            }
            else if (selector <= 90)
            {
                key = inRange(rng);
            }
            else
            {
                key = loopSize + outRange(rng);
            }

            ops.push_back(Operation{isPut, key, key * 10 + (i % 113)});
        }

        return ops;
    }

    std::vector<Operation> makeWorkloadShiftScenarioOps(int operations)
    {
        std::vector<Operation> ops;
        ops.reserve(static_cast<size_t>(operations));

        std::mt19937 rng(20260318);
        std::uniform_int_distribution<int> p01to100(1, 100);

        const int phaseLen = operations / 5;

        for (int i = 0; i < operations; ++i)
        {
            const int phase = i / phaseLen;

            int putProbability = 20;
            switch (phase)
            {
            case 0:
                putProbability = 15;
                break;
            case 1:
                putProbability = 30;
                break;
            case 2:
                putProbability = 10;
                break;
            case 3:
                putProbability = 25;
                break;
            default:
                putProbability = 20;
                break;
            }

            const bool isPut = p01to100(rng) <= putProbability;
            int key = 0;

            if (phase == 0)
            {
                key = p01to100(rng) % 5;
            }
            else if (phase == 1)
            {
                key = p01to100(rng) % 400;
            }
            else if (phase == 2)
            {
                key = (i - phaseLen * 2) % 100;
            }
            else if (phase == 3)
            {
                const int locality = (i / 800) % 5;
                key = locality * 15 + (p01to100(rng) % 15);
            }
            else
            {
                const int r = p01to100(rng);
                if (r <= 40)
                {
                    key = p01to100(rng) % 5;
                }
                else if (r <= 70)
                {
                    key = 5 + (p01to100(rng) % 45);
                }
                else
                {
                    key = 50 + (p01to100(rng) % 350);
                }
            }

            ops.push_back(Operation{isPut, key, key * 10 + (phase * 100 + i % 97)});
        }

        return ops;
    }

    std::vector<ScenarioCase> makeDefaultScenarioCases()
    {
        return {
            ScenarioCase{"测试场景1：热点数据访问测试", 20, makeHotDataScenarioOps(500000, 20, 5000)},
            ScenarioCase{"测试场景2：循环扫描测试", 50, makeLoopScanScenarioOps(200000, 500)},
            ScenarioCase{"测试场景3：工作负载剧烈变化测试", 30, makeWorkloadShiftScenarioOps(80000)}};
    }
}
