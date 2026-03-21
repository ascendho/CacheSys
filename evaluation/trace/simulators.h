#pragma once

#include <vector>

#include "types.h"

namespace CacheSys::Eval::Trace
{
    EvalResult simulateLru(const std::vector<int> &trace, int capacity);
    EvalResult simulateLfu(const std::vector<int> &trace, int capacity);
    EvalResult simulateArc(const std::vector<int> &trace, int capacity);
    EvalResult simulateOpt(const std::vector<int> &trace, int capacity);
}
