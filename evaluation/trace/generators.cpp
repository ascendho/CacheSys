#include "generators.h"

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace CacheSys::Eval::Trace
{
    namespace
    {
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
    }

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
}
