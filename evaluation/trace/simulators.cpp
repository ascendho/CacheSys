#include "simulators.h"

#include <limits>
#include <unordered_map>
#include <unordered_set>

#include "ArcCache.h"
#include "LfuCache.h"
#include "LruCache.h"

namespace CacheSys::Eval::Trace
{
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

    EvalResult simulateArc(const std::vector<int> &trace, int capacity)
    {
        if (capacity <= 0)
        {
            return EvalResult{trace.size(), trace.empty() ? 0.0 : 1.0};
        }

        CacheSys::ArcCache<int, int> cache(static_cast<size_t>(capacity));
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
}
