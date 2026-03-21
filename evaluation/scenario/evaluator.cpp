#include "evaluator.h"

#include "ArcCache.h"
#include "LfuCache.h"
#include "LruCache.h"

namespace CacheSys::Eval::Scenario
{
    EvalResult evalLru(const std::vector<Operation> &ops, int capacity)
    {
        CacheSys::LruCache<int, int> cache(capacity);
        EvalResult result;

        for (const Operation &op : ops)
        {
            if (op.isPut)
            {
                cache.put(op.key, op.value);
                continue;
            }

            ++result.gets;
            int value = 0;
            if (cache.get(op.key, value))
            {
                ++result.hits;
            }
        }

        return result;
    }

    EvalResult evalLfu(const std::vector<Operation> &ops, int capacity)
    {
        CacheSys::LfuCache<int, int> cache(capacity);
        EvalResult result;

        for (const Operation &op : ops)
        {
            if (op.isPut)
            {
                cache.put(op.key, op.value);
                continue;
            }

            ++result.gets;
            int value = 0;
            if (cache.get(op.key, value))
            {
                ++result.hits;
            }
        }

        return result;
    }

    EvalResult evalArc(const std::vector<Operation> &ops, int capacity)
    {
        CacheSys::ArcCache<int, int> cache(static_cast<size_t>(capacity));
        EvalResult result;

        for (const Operation &op : ops)
        {
            if (op.isPut)
            {
                cache.put(op.key, op.value);
                continue;
            }

            ++result.gets;
            int value = 0;
            if (cache.get(op.key, value))
            {
                ++result.hits;
            }
        }

        return result;
    }
}
