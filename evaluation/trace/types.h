#pragma once

#include <cstddef>

namespace CacheSys::Eval::Trace
{
    struct EvalResult
    {
        size_t misses = 0;
        double missRatio = 0.0;
    };
}